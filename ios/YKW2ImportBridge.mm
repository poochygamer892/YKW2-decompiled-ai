#import "YKW2ImportBridge.h"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "ykw2/core/import_contract.hpp"
#include "ykw2/core/romfs_extract.hpp"

namespace {

NSError *ImportError(std::string_view code) {
    const auto message = [NSString stringWithUTF8String:std::string(code).c_str()];
    return [NSError errorWithDomain:@"com.ykw2.port.import"
                               code:1
                           userInfo:@{NSLocalizedDescriptionKey: message ?: @"Import failed"}];
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
        @try {
            NSData *chunk = [NSData dataWithBytes:data length:size];
            [handle_ writeData:chunk];
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
    }

private:
    NSURL *root_;
    NSFileHandle *handle_ = nil;
    NSString *currentPath_ = nil;
};

}  // namespace

@implementation YKW2ImportBridge

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

    NSString *marker = [cacheURL.path stringByAppendingPathComponent:@".import-complete"];
    NSString *contents = [NSString stringWithFormat:@"files=%llu\nbytes=%llu\n",
                          static_cast<unsigned long long>(extraction.value->files),
                          static_cast<unsigned long long>(extraction.value->bytes)];
    if (![contents writeToFile:marker atomically:YES encoding:NSUTF8StringEncoding error:error]) {
        return NO;
    }
    return YES;
}

@end
