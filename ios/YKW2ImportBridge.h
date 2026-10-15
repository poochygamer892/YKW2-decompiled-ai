#import <Foundation/Foundation.h>
#include <stdint.h>

NS_ASSUME_NONNULL_BEGIN

@interface YKW2ImportBridge : NSObject

+ (BOOL)hasReadyImportAtURL:(NSURL *)cacheURL;
+ (NSString *)startRuntimeAtURL:(NSURL *)cacheURL;
+ (NSString *)handleRuntimeInput:(NSInteger)input;
+ (NSString *)advanceRuntime:(NSTimeInterval)deltaSeconds;
+ (NSInteger)runtimeBackgroundWidth;
+ (NSInteger)runtimeBackgroundHeight;
+ (NSData *)runtimeBackgroundRGBA;

+ (NSString *)openZakuroROMAtURL:(NSURL *)romURL dataDirectoryURL:(NSURL *)dataURL;
+ (NSInteger)runZakuroFrames:(NSInteger)frames;
+ (NSInteger)setZakuroButtons:(uint32_t)buttons
                      touchX:(NSInteger)touchX
                      touchY:(NSInteger)touchY;
+ (NSData *)zakuroTopScreenRGBA;
+ (NSData *)zakuroBottomScreenRGBA;
+ (NSData *)takeZakuroAudioFrames:(NSInteger)maximumFrames;
+ (void)closeZakuro;

+ (BOOL)importDocumentAtURL:(NSURL *)sourceURL
                     cacheURL:(NSURL *)cacheURL
                        error:(NSError * _Nullable * _Nullable)error;

@end

NS_ASSUME_NONNULL_END
