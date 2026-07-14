//
//  AutoLogin.h
//  SovietX
//
//  Created by MustangYM on 2026/6/26.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface AutoLogin : NSObject
+ (void)startWithEnabled:(BOOL)enabled;
+ (void)reset;
@end

NS_ASSUME_NONNULL_END
