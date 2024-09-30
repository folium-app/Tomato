//
//  TomatoObjC.m
//  Tomato
//
//  Created by Jarrod Norwell on 12/9/2024.
//  Copyright © 2024 Jarrod Norwell. All rights reserved.
//

#import "TomatoObjC.h"

#include "libgambatte/gambatte.h"

using namespace gambatte;

GB gameboyEmulator;

uint32_t *gbAB = new uint32_t[2064 * 2 * 4], *gbFB;

@implementation TomatoObjC
-(TomatoObjC *) init {
    if (self = [super init]) {
        gameboyEmulator.setSaveDir([[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask][0].path UTF8String]);
    } return self;
}

+(TomatoObjC *) sharedInstance {
    static TomatoObjC *sharedInstance = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[self alloc] init];
    });
    return sharedInstance;
}

-(void) insertCartridge:(NSURL *)url {
    _paused = FALSE;
    gameboyEmulator.load([url.path UTF8String]);
    
    double fps = 4194304.0 / 70224.0; // ~60fps
    frameInterval = fps;
    
    gbFB = (uint32_t *)malloc(160 * 144 * 4);
}

-(void) loop {
    NSTimeInterval realTime, emulatedTime = OEMonotonicTime();
        
    OESetThreadRealtime(1. / (1. * frameInterval), .007, .03);
    
    while (true) {
        size_t size = 2064;
        while(gameboyEmulator.runFor((gambatte::uint_least32_t*)gbFB, 160, (gambatte::uint_least32_t*)gbAB, size) == -1 && !_paused) {}
        
        NSTimeInterval advance = 1.0 / (1. * frameInterval);
                
        emulatedTime += advance;
        realTime = OEMonotonicTime();
        
        if(realTime - emulatedTime > 1.0) {
            NSLog(@"Synchronizing because we are %g seconds behind", realTime - emulatedTime);
            emulatedTime = realTime;
        }
        OEWaitUntil(emulatedTime);
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, 0);
        
        if (_buffers != nil)
            _buffers(gbAB, gbFB);
    }
}
@end
