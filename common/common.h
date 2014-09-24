/*
 Copyright (c) 2012-, ISO/IEC JTC1/SC29/WG11
 Written by Alex Giladi <alex.giladi@gmail.com>
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of the ISO/IEC nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdlib.h>
#include <inttypes.h>

// fun thing to do: rewrite with C11 Generic syntax...

#define SKIT_PRINT_SINT32(arg )     fprintf( stderr,  "%s:%d (in %s) \t%s=%"PRId32"\n",  __FILE__, __LINE__, __PRETTY_FUNCTION__, #arg,  (arg ) );
#define SKIT_PRINT_UINT32(arg )     fprintf( stderr,  "%s:%d (in %s) \t%s=%"PRIu32"\n",  __FILE__, __LINE__, __PRETTY_FUNCTION__, #arg,  (arg ) );

#define SKIT_PRINT_SINT64(arg )  fprintf( stderr,  "%s:%d (in %s) \t%s=%"PRId64"\n",  __FILE__, __LINE__, __PRETTY_FUNCTION__, #arg,  (arg ) );
#define SKIT_PRINT_UINT64(arg )  fprintf( stderr,  "%s:%d (in %s) \t%s=%"PRIu64"\n",  __FILE__, __LINE__, __PRETTY_FUNCTION__, #arg,  (arg ) );

#define SKIT_PRINT_UINT32_HEX(arg ) fprintf( stderr,  "%s:%d (in %s) \t%s=0x%"PRIX32"\n",  __FILE__, __LINE__, __PRETTY_FUNCTION__, #arg,  (arg ) );
#define SKIT_PRINT_UINT64_HEX(arg ) fprintf( stderr,  "%s:%d (in %s) \t%s=0x%"PRIX64"\n",  __FILE__, __LINE__, __PRETTY_FUNCTION__, #arg,  (arg ) );

#define SKIT_PRINT_STRING( arg )   fprintf( stderr,  "%s:%d (in %s)\t%s=%s\n",  __FILE__, __LINE__, __PRETTY_FUNCTION__, #arg,  (arg ) );

#define SKIT_PRINT_DOUBLE( arg )     fprintf( stderr,  "%s:%d (in %s)\t%s=%lf\n",  __FILE__, __LINE__, __PRETTY_FUNCTION__, #arg,  (arg ) );


#define BSWAP32(X)  __builtin_bswap32((X))
#define BSWAP64(X)  __builtin_bswap64((X));

#define ARRAYSIZE(x)	((sizeof(x))/(sizeof((x)[0])))

typedef struct 
{
  uint8_t* bytes;
  size_t   len;
} buf_t;

#endif /* _COMMON_H */
