
/* tracelog.h
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2009-2017 Christoph Ender.
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
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef tracelog_h_INCLUDED
#define tracelog_h_INCLUDED

#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>

#include "types.h"
#include "filesys.h"

#define DEFAULT_TRACE_FILE_NAME "tracelog.txt"

#ifndef tracelog_c_INCLUDED
extern z_file *stream_t;
#endif // tracelog_c_INCLUDED
 
#ifdef ENABLE_TRACING

#define TRACE_LOG(...) \
if (stream_t != NULL) \
{ fsi->fileprintf(stream_t, __VA_ARGS__); fsi->flushfile(stream_t); }
#define TRACE_LOG_Z_UCS _trace_log_z_ucs
void turn_on_trace(void);
void turn_off_trace(void);
void _trace_log_z_ucs(z_ucs *output);
#else /* ENABLE_TRACING */

#ifdef ENABLE_IOS_TRACING

#define TRACE_LOG(...) _trace_log_ios(__VA_ARGS__)
#define TRACE_LOG_Z_UCS _trace_log_ios_z_ucs
void turn_on_trace(void);
void turn_off_trace(void);
void _trace_log_ios(char *format, ...);
void _trace_log_ios_z_ucs(z_ucs *output);

#else /* ENABLE_IOS_TRACING */

#define TRACE_LOG(...) ;
#define TRACE_LOG_Z_UCS(...) ;

#endif /* ENABLE_IOS_TRACING */
#endif /* ENABLE_TRACING */

#endif // tracelog_h_INCLUDED

