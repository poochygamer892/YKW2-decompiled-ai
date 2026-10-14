#import "YKW2ImportBridge.h"

#include <array>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "ykw2/core/import_contract.hpp"
#include "ykw2/core/resource_catalog.hpp"
#include "ykw2/core/romfs_extract.hpp"
#include "ykw2/core/runtime_bootstrap.hpp"
#include "ykw2/core/runtime_scene.hpp"

namespace {

NSError *ImportError(std::string_view code) {
    const auto message = [NSString stringWithUTF8String:std::string(code).c_str()];
    return [NSError errorWithDomain:@"com.ykw2.port.import"
                               code:1
                           userInfo:@{NSLocalizedDescriptionKey: message ?: @"Import failed"}];
}

bool markerValue(NSString *marker, NSString *key, std::uint64_t& value) {
    for (NSString *line in [marker componentsSeparatedByString:@"\n"]) {
        NSArray<NSString *> *parts = [line componentsSeparatedByString:@"="];
        if (parts.count == 2 && [parts[0] isEqualToString:key]) {
            value = static_cast<std::uint64_t>(parts[1].longLongValue);
            return true;
        }
    }
    return false;
}

ykw2::core::RuntimeBootInfo loadRuntimeBoot(NSURL *cacheURL) {
    ykw2::core::RuntimeBootInfo boot;
    NSString *catalogPath = [cacheURL.path stringByAppendingPathComponent:@".catalog"];
    NSString *markerPath = [cacheURL.path stringByAppendingPathComponent:@".import-complete"];
    NSData *catalogData = [NSData dataWithContentsOfFile:catalogPath];
    NSString *marker = [NSString stringWithContentsOfFile:markerPath
                                                   encoding:NSUTF8StringEncoding error:nil];
    if (catalogData == nil || marker == nil) {
        boot.state = ykw2::core::RuntimeBootState::ImportRequired;
        return boot;
    }
    const auto catalogText = std::string(static_cast<const char *>(catalogData.bytes), catalogData.length);
    const auto catalog = ykw2::core::parse_resource_catalog(catalogText);
    if (!catalog) {
        boot.state = ykw2::core::RuntimeBootState::ImportIncomplete;
        return boot;
    }
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t fingerprint = 0;
    if (!markerValue(marker, @"files", files) || !markerValue(marker, @"bytes", bytes) ||
        !markerValue(marker, @"fingerprint", fingerprint)) {
        boot.state = ykw2::core::RuntimeBootState::ImportIncomplete;
        return boot;
    }
    return ykw2::core::evaluate_runtime_boot(
        *catalog.value, true, files, bytes, fingerprint);
}

std::mutex runtimeMutex;
ykw2::core::RuntimeScene runtimeScene;
bool runtimeStarted = false;

NSString *runtimeString(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()] ?: @"";
}

class FileSource final : public ykw2::core::ResourceSource {
public:
    FileSource(NSFileHandle *handle, std::uint64_t size) : handle_(handle), size_(size) {}

    std::uint64_t size() const override { return size_; }

    bool read(std::uint64_t offset, std::uint8_t *destination,
              std::size_t length) const override {
        if (!contains(offset, length)) {
            return false;
        }
        @try {
            [handle_ seekToFileOffset:offset];
            NSData *data = [handle_ readDataOfLength:length];
            if (data.length != length) {
                return false;
            }
            std::memcpy(destination, data.bytes, length);
            return true;
        } @catch (NSException *) {
            return false;
        }
    }

private:
    NSFileHandle *handle_;
    std::uint64_t size_;
};

class DirectorySink final : public ykw2::core::ResourceSink {
public:
    explicit DirectorySink(NSURL *root) : root_(root) {}

    bool begin_file(std::string_view path, std::uint64_t) override {
        NSString *relative = [NSString stringWithUTF8String:std::string(path).c_str()];
        if (relative == nil) {
            return false;
        }
        current_relative_path_ = std::string(path);
        current_size_ = 0;
        current_signature_.fill(0);
        current_signature_size_ = 0;
        NSString *fullPath = [root_.path stringByAppendingPathComponent:relative];
        NSString *parent = [fullPath stringByDeletingLastPathComponent];
        NSError *directoryError = nil;
        if (![[NSFileManager defaultManager] createDirectoryAtPath:parent
                                          withIntermediateDirectories:YES
                                                           attributes:nil
                                                                error:&directoryError]) {
            return false;
        }
        [[NSFileManager defaultManager] createFileAtPath:fullPath contents:nil attributes:nil];
        @try {
            handle_ = [NSFileHandle fileHandleForWritingAtPath:fullPath];
        } @catch (NSException *) {
            handle_ = nil;
        }
        currentPath_ = fullPath;
        return handle_ != nil;
    }

    bool write(const std::uint8_t *data, std::size_t size) override {
        if (handle_ == nil) {
            return false;
        }
        const auto signatureBytes = std::min<std::size_t>(
            current_signature_.size() - current_signature_size_, size);
        std::copy_n(data, signatureBytes, current_signature_.data() + current_signature_size_);
        current_signature_size_ += signatureBytes;
        @try {
            NSData *chunk = [NSData dataWithBytes:data length:size];
            [handle_ writeData:chunk];
            current_size_ += size;
            return true;
        } @catch (NSException *) {
            return false;
        }
    }

    bool end_file() override {
        if (handle_ == nil) {
            return false;
        }
        @try {
            [handle_ closeFile];
        } @catch (NSException *) {
            handle_ = nil;
            return false;
        }
        handle_ = nil;
        inputs_.push_back({current_relative_path_, current_size_, current_signature_});
        current_relative_path_.clear();
        currentPath_ = nil;
        return true;
    }

    void abort_file() override {
        if (handle_ != nil) {
            @try {
                [handle_ closeFile];
            } @catch (NSException *) {
            }
            handle_ = nil;
        }
        if (currentPath_ != nil) {
            [[NSFileManager defaultManager] removeItemAtPath:currentPath_ error:nil];
            currentPath_ = nil;
        }
        current_relative_path_.clear();
    }

    const std::vector<ykw2::core::CatalogInput>& inputs() const { return inputs_; }

private:
    NSURL *root_;
    NSFileHandle *handle_ = nil;
    NSString *currentPath_ = nil;
    std::string current_relative_path_;
    std::array<std::uint8_t, 4> current_signature_{};
    std::size_t current_signature_size_ = 0;
    std::uint64_t current_size_ = 0;
    std::vector<ykw2::core::CatalogInput> inputs_;
};

}  // namespace

@implementation YKW2ImportBridge

+ (BOOL)hasReadyImportAtURL:(NSURL *)cacheURL {
    NSString *catalogPath = [cacheURL.path stringByAppendingPathComponent:@".catalog"];
    NSString *markerPath = [cacheURL.path stringByAppendingPathComponent:@".import-complete"];
    NSData *catalogData = [NSData dataWithContentsOfFile:catalogPath];
    NSString *marker = [NSString stringWithContentsOfFile:markerPath
                                                   encoding:NSUTF8StringEncoding error:nil];
    if (catalogData == nil || marker == nil) {
        return NO;
    }
    const auto catalogText = std::string(static_cast<const char *>(catalogData.bytes), catalogData.length);
    const auto catalog = ykw2::core::parse_resource_catalog(catalogText);
    if (!catalog) {
        return NO;
    }

    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t fingerprint = 0;
    for (NSString *line in [marker componentsSeparatedByString:@"\n"]) {
        NSArray<NSString *> *parts = [line componentsSeparatedByString:@"="];
        if (parts.count != 2) {
            continue;
        }
        const auto value = static_cast<std::uint64_t>(parts[1].longLongValue);
        if ([parts[0] isEqualToString:@"files"]) {
            files = value;
        } else if ([parts[0] isEqualToString:@"bytes"]) {
            bytes = value;
        } else if ([parts[0] isEqualToString:@"fingerprint"]) {
            fingerprint = value;
        }
    }
    const auto boot = ykw2::core::evaluate_runtime_boot(
        *catalog.value, true, files, bytes, fingerprint);
    return boot.state == ykw2::core::RuntimeBootState::Ready;
}

+ (NSString *)startRuntimeAtURL:(NSURL *)cacheURL {
    std::lock_guard<std::mutex> lock(runtimeMutex);
    runtimeScene.start(loadRuntimeBoot(cacheURL));
    runtimeStarted = true;
    return runtimeString(ykw2::core::serialize_runtime_response(
        {false, ykw2::core::RuntimeCommand::None}, runtimeScene.snapshot()));
}

+ (NSString *)handleRuntimeInput:(NSInteger)input {
    std::lock_guard<std::mutex> lock(runtimeMutex);
    if (!runtimeStarted) {
        ykw2::core::RuntimeBootInfo boot;
        boot.state = ykw2::core::RuntimeBootState::ImportRequired;
        runtimeScene.start(boot);
        runtimeStarted = true;
    }
    ykw2::core::RuntimeInput runtimeInput;
    switch (input) {
        case 0:
            runtimeInput = ykw2::core::RuntimeInput::MoveNext;
            break;
        case 1:
            runtimeInput = ykw2::core::RuntimeInput::MovePrevious;
            break;
        case 2:
            runtimeInput = ykw2::core::RuntimeInput::Confirm;
            break;
        case 3:
            runtimeInput = ykw2::core::RuntimeInput::Back;
            break;
        default:
            return runtimeString(ykw2::core::serialize_runtime_response(
                {false, ykw2::core::RuntimeCommand::None}, runtimeScene.snapshot()));
    }
    const auto transition = runtimeScene.handle(runtimeInput);
    return runtimeString(ykw2::core::serialize_runtime_response(
        transition, runtimeScene.snapshot()));
}

+ (BOOL)importDocumentAtURL:(NSURL *)sourceURL
                     cacheURL:(NSURL *)cacheURL
                        error:(NSError **)error {
    NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:sourceURL.path error:error];
    if (attributes == nil) {
        return NO;
    }
    const auto imageSize = static_cast<std::uint64_t>([attributes[NSFileSize] unsignedLongLongValue]);
    NSFileHandle *handle = [NSFileHandle fileHandleForReadingAtPath:sourceURL.path];
    if (handle == nil) {
        if (error != nullptr) {
            *error = ImportError("source_open_failed");
        }
        return NO;
    }
    std::array<std::uint8_t, 0x200> header{};
    NSData *headerData = [handle readDataOfLength:header.size()];
    if (headerData.length != header.size()) {
        [handle closeFile];
        if (error != nullptr) {
            *error = ImportError("source_header_read_failed");
        }
        return NO;
    }
    std::memcpy(header.data(), headerData.bytes, header.size());
    const auto plan = ykw2::core::build_import_plan(
        ykw2::core::ByteView(header.data(), header.size()), imageSize, true);
    if (!plan) {
        [handle closeFile];
        if (error != nullptr) {
            *error = ImportError(plan.error->code);
        }
        return NO;
    }

    NSError *directoryError = nil;
    if (![[NSFileManager defaultManager] createDirectoryAtURL:cacheURL
                                    withIntermediateDirectories:YES
                                                     attributes:nil
                                                          error:&directoryError]) {
        [handle closeFile];
        if (error != nullptr) {
            *error = directoryError;
        }
        return NO;
    }
    [[NSFileManager defaultManager] removeItemAtPath:
        [cacheURL.path stringByAppendingPathComponent:@".import-complete"] error:nil];
    [[NSFileManager defaultManager] removeItemAtPath:
        [cacheURL.path stringByAppendingPathComponent:@".catalog"] error:nil];
    FileSource source(handle, imageSize);
    DirectorySink sink(cacheURL);
    const auto extraction = ykw2::core::extract_application_romfs(source, *plan.value, sink);
    [handle closeFile];
    if (!extraction) {
        sink.abort_file();
        if (error != nullptr) {
            *error = ImportError(extraction.error->code);
        }
        return NO;
    }

    const auto catalog = ykw2::core::build_resource_catalog(sink.inputs());
    if (!catalog) {
        if (error != nullptr) {
            *error = ImportError(catalog.error->code);
        }
        return NO;
    }
    NSString *catalogPath = [cacheURL.path stringByAppendingPathComponent:@".catalog"];
    NSString *catalogContents = [NSString stringWithUTF8String:
        ykw2::core::serialize_resource_catalog(*catalog.value).c_str()];
    if (catalogContents == nil ||
        ![catalogContents writeToFile:catalogPath atomically:YES
                              encoding:NSUTF8StringEncoding error:error]) {
        return NO;
    }
    NSString *marker = [cacheURL.path stringByAppendingPathComponent:@".import-complete"];
    NSString *contents = [NSString stringWithFormat:@"files=%llu\nbytes=%llu\nfingerprint=%llu\n",
                          static_cast<unsigned long long>(extraction.value->files),
                          static_cast<unsigned long long>(extraction.value->bytes),
                          static_cast<unsigned long long>(catalog.value->fingerprint())];
    if (![contents writeToFile:marker atomically:YES encoding:NSUTF8StringEncoding error:error]) {
        return NO;
    }
    return YES;
}

@end
