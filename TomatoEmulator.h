//
//  TomatoEmulator.h
//  Tomato
//
//  Created by Jarrod Norwell on 2/7/2025.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface TomatoEmulator : NSObject {
    NSString *name;
    NSURL *directory;
}

@property (nonatomic, strong) void (^buffer) (uint32_t*);
@property (nonatomic, strong) void (^framerate) (float);

+(TomatoEmulator *) sharedInstance NS_SWIFT_NAME(shared());

-(void) insertCartridge:(NSURL *)url NS_SWIFT_NAME(insert(_:));

-(void) start;
-(void) pause:(BOOL)paused;
-(BOOL) isPaused;
-(void) stop;

-(void) load:(NSURL *)url;
-(void) save:(NSURL *)url;

-(void) button:(uint8_t)button player:(int)player pressed:(BOOL)pressed;
@end

NS_ASSUME_NONNULL_END
