//
//  TomatoObjC.m
//  Tomato
//
//  Created by Jarrod Norwell on 12/9/2024.
//  Copyright © 2024 Jarrod Norwell. All rights reserved.
//

#import "TomatoObjC.h"

#include "libgambatte/gambatte.h"

gambatte::GB gameboyEmulator;

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
    
    gbFB = new uint32_t[160 * 144 * 4];
}

-(ptrdiff_t) step:(uint32_t *)video audio:(uint32_t *)audio {
    size_t samples = 35112 / 4;
    return gameboyEmulator.runFor((gambatte::uint_least32_t*)video, 160, (gambatte::uint_least32_t*)audio, samples);
}
@end
