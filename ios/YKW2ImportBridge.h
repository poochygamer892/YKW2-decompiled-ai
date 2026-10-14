#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface YKW2ImportBridge : NSObject

+ (BOOL)hasReadyImportAtURL:(NSURL *)cacheURL;
+ (NSString *)startRuntimeAtURL:(NSURL *)cacheURL;
+ (NSString *)handleRuntimeInput:(NSInteger)input;
+ (NSInteger)runtimeBackgroundWidth;
+ (NSInteger)runtimeBackgroundHeight;
+ (NSData *)runtimeBackgroundRGBA;

+ (BOOL)importDocumentAtURL:(NSURL *)sourceURL
                     cacheURL:(NSURL *)cacheURL
                        error:(NSError * _Nullable * _Nullable)error;

@end

NS_ASSUME_NONNULL_END
