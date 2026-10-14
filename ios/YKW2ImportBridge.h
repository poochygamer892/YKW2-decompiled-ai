#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface YKW2ImportBridge : NSObject

+ (BOOL)importDocumentAtURL:(NSURL *)sourceURL
                     cacheURL:(NSURL *)cacheURL
                        error:(NSError * _Nullable * _Nullable)error;

@end

NS_ASSUME_NONNULL_END
