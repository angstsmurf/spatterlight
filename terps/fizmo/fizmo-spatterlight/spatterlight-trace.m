
/* ios-trace.m
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2012 Andrew Plotkin.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>

#include <tools/tracelog.h>
#include <tools/z_ucs.h>

static BOOL trace_active = YES;

void turn_on_trace(void)
{
	if (trace_active) {
		TRACE_LOG("Tracelog already active.\n");
		return;
	}
	
	trace_active = YES;
}

void turn_off_trace(void)
{
	if (!trace_active) {
		TRACE_LOG("Tracelog already deactivated.\n");
		return;
	}
	
	trace_active = NO;
}

void _trace_log_ios(char *format, ...)
{
	char *buf = nil;
	
	va_list args;
	va_start(args, format);
	vasprintf(&buf, format, args);
	va_end(args);
	
	if (trace_active) {
		NSLog(@"fizmo: %s", buf);
	}
	
	free(buf);
}

void _trace_log_ios_z_ucs(z_ucs *output)
{
	if (trace_active) {
		/* Turn the buffer into an NSString. We'll release this at the end of the function. 
		 This is an endianness dependency; we're telling NSString that our array of 32-bit words in stored little-endian. (True for all iOS, as I write this.) */
		size_t len = z_ucs_len(output);
        @autoreleasepool {
            NSString *str = [[NSString alloc] initWithBytes:output length:len*sizeof(z_ucs) encoding:NSUTF32LittleEndianStringEncoding];
            NSLog(@"fizmo: %@", str);
        }
	}
}

