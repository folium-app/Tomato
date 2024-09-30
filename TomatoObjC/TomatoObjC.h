//
//  TomatoObjC.h
//  Tomato
//
//  Created by Jarrod Norwell on 12/9/2024.
//  Copyright © 2024 Jarrod Norwell. All rights reserved.
//

#import <Foundation/Foundation.h>

#include <mach/mach_time.h>
#include <mach/mach_init.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <pthread.h>

NS_ASSUME_NONNULL_BEGIN

static double mach_to_sec = 0;

static void init_mach_time(void)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        struct mach_timebase_info base;
        mach_timebase_info(&base);
        mach_to_sec = 1e-9 * (base.numer / (double)base.denom);
    });
}

NSTimeInterval OEMonotonicTime(void)
{
    init_mach_time();
    
    return mach_absolute_time() * mach_to_sec;
}

void OEWaitUntil(NSTimeInterval time)
{
    init_mach_time();
    
    mach_wait_until(time / mach_to_sec);
}

BOOL OESetThreadRealtime(NSTimeInterval period, NSTimeInterval computation, NSTimeInterval constraint)
{
    struct thread_time_constraint_policy ttcpolicy;
    thread_port_t threadport = pthread_mach_thread_np(pthread_self());
    
    init_mach_time();
    
    assert(computation < .05);
    assert(computation < constraint);
    
    ttcpolicy.period      = period / mach_to_sec;
    ttcpolicy.computation = computation / mach_to_sec;
    ttcpolicy.constraint  = constraint / mach_to_sec;
    ttcpolicy.preemptible = 1;
    
    if(thread_policy_set(threadport,
                         THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&ttcpolicy,
                         THREAD_TIME_CONSTRAINT_POLICY_COUNT) != KERN_SUCCESS)
    {
        NSLog(@"OESetThreadRealtime() failed.");
        return NO;
    }
    
    return YES;
}

@interface TomatoObjC : NSObject {
    NSInteger frameInterval;
    BOOL _paused;
}

+(TomatoObjC *) sharedInstance NS_SWIFT_NAME(shared());

@property (nonatomic, strong) void (^buffers) (uint32_t*, uint32_t*);

-(void) insertCartridge:(NSURL *)url;
-(void) loop;
@end

NS_ASSUME_NONNULL_END
