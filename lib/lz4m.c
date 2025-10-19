/*
 *  LZ4M - Fast LZ compression algorithm with 4-byte granularity
 *  Copyright (c) Yann Collet. All rights reserved.
 *  LZ4M modifications by LZ4m implementation team
 *
 *  BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following disclaimer
 *  in the documentation and/or other materials provided with the
 *  distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You can contact the author at :
 *   - LZ4 homepage : http://www.lz4.org
 *   - LZ4 source repository : https://github.com/lz4/lz4
 */

/*-************************************
*  Tuning parameters
**************************************/
/*
 * LZ4M_HEAPMODE :
 * Select how stateless compression functions like `LZ4M_compress_default()`
 * allocate memory for their hash table,
 * in memory stack (0:default, fastest), or in memory heap (1:requires malloc()).
 */
#ifndef LZ4M_HEAPMODE
#  define LZ4M_HEAPMODE 0
#endif

/*
 * LZ4M_ACCELERATION_DEFAULT :
 * Select "acceleration" for LZ4M_compress_fast() when parameter value <= 0
 */
#define LZ4M_ACCELERATION_DEFAULT 1
/*
 * LZ4M_ACCELERATION_MAX :
 * Any "acceleration" value higher than this threshold
 * get treated as LZ4M_ACCELERATION_MAX instead
 */
#define LZ4M_ACCELERATION_MAX 65537

/*-************************************
*  Dependency
**************************************/
#include "lz4m.h"
#include <stdlib.h>   /* malloc, calloc, free */
#include <string.h>   /* memset, memcpy */

/*-************************************
*  Memory routines
**************************************/
#include <stdlib.h>   /* malloc, calloc, free */
#define ALLOC(s)          malloc(s)
#define ALLOC_AND_ZERO(s) calloc(1,s)
#define FREEMEM(p)        free(p)

/*-************************************
*  Basic Types
**************************************/
#include <limits.h>
#include <stdint.h>

typedef  uint8_t BYTE;
typedef uint16_t U16;
typedef uint32_t U32;
typedef  int32_t S32;
typedef uint64_t U64;
typedef uintptr_t uptrval;

#if defined(__x86_64__)
  typedef U64    reg_t;   /* 64-bits in x32 mode */
#else
  typedef size_t reg_t;   /* 32-bits in x32 mode */
#endif

/*-************************************
*  Reading and writing into memory
**************************************/
static unsigned LZ4M_isLittleEndian(void)
{
    const union { U32 u; BYTE c[4]; } one = { 1 };   /* don't use static : performance detrimental */
    return one.c[0];
}

#if defined(LZ4_FORCE_MEMORY_ACCESS) && (LZ4_FORCE_MEMORY_ACCESS==2)
/* lie to the compiler about data alignment; use with caution */

static U16 LZ4M_read16(const void* memPtr) { return *(const U16*) memPtr; }
static U32 LZ4M_read32(const void* memPtr) { return *(const U32*) memPtr; }
static reg_t LZ4M_read_ARCH(const void* memPtr) { return *(const reg_t*) memPtr; }

static void LZ4M_write16(void* memPtr, U16 value) { *(U16*)memPtr = value; }
static void LZ4M_write32(void* memPtr, U32 value) { *(U32*)memPtr = value; }

#elif defined(LZ4_FORCE_MEMORY_ACCESS) && (LZ4_FORCE_MEMORY_ACCESS==1)

/* __pack instructions are safer, but compiler specific, hence potentially problematic for some compilers */
/* currently only defined for gcc and icc */
typedef union { U16 u16; U32 u32; reg_t uArch; } __attribute__((packed)) unalign;

static U16 LZ4M_read16(const void* ptr) { return ((const unalign*)ptr)->u16; }
static U32 LZ4M_read32(const void* ptr) { return ((const unalign*)ptr)->u32; }
static reg_t LZ4M_read_ARCH(const void* ptr) { return ((const unalign*)ptr)->uArch; }

static void LZ4M_write16(void* memPtr, U16 value) { ((unalign*)memPtr)->u16 = value; }
static void LZ4M_write32(void* memPtr, U32 value) { ((unalign*)memPtr)->u32 = value; }

#else  /* safe and portable access using memcpy() */

static U16 LZ4M_read16(const void* memPtr)
{
    U16 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

static U32 LZ4M_read32(const void* memPtr)
{
    U32 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

static reg_t LZ4M_read_ARCH(const void* memPtr)
{
    reg_t val; memcpy(&val, memPtr, sizeof(val)); return val;
}

static void LZ4M_write16(void* memPtr, U16 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

static void LZ4M_write32(void* memPtr, U32 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

#endif /* LZ4_FORCE_MEMORY_ACCESS */

static U16 LZ4M_readLE16(const void* memPtr)
{
    if (LZ4M_isLittleEndian()) {
        return LZ4M_read16(memPtr);
    } else {
        const BYTE* p = (const BYTE*)memPtr;
        return (U16)((U16)p[0] + (p[1]<<8));
    }
}

static void LZ4M_writeLE16(void* memPtr, U16 value)
{
    if (LZ4M_isLittleEndian()) {
        LZ4M_write16(memPtr, value);
    } else {
        BYTE* p = (BYTE*)memPtr;
        p[0] = (BYTE) value;
        p[1] = (BYTE)(value >> 8);
    }
}

/* customized variant of memcpy, which can overwrite up to 8 bytes beyond dstEnd */
static void LZ4M_wildCopy8(void* dstPtr, const void* srcPtr, void* dstEnd)
{
    BYTE* d = (BYTE*)dstPtr;
    const BYTE* s = (const BYTE*)srcPtr;
    BYTE* const e = (BYTE*)dstEnd;

    do { memcpy(d, s, 8); d+=8; s+=8; } while (d<e);
}

/*-************************************
*  Common Constants
**************************************/
#define MINMATCH 4  /* LZ4M uses 4-byte granularity */

#define WILDCOPYLENGTH 8
#define LASTLITERALS   5   /* see ../doc/lz4_Block_format.md#parsing-restrictions */
#define MFLIMIT        12  /* see ../doc/lz4_Block_format.md#parsing-restrictions */
#define MATCH_SAFEGUARD_DISTANCE  ((2*WILDCOPYLENGTH) - MINMATCH)   /* ensure it's possible to write 2 x wildcopy without overflowing output buffer */
static const int LZ4M_minLength = (MFLIMIT+1);

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define LZ4M_DISTANCE_ABSOLUTE_MAX 65535
#define LZ4M_DISTANCE_MAX 65535 /* Maximum supported distance */

#define ML_BITS  4
#define ML_MASK  ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)

/*-************************************
*  Error detection
**************************************/
#define LZ4M_STATIC_ASSERT(c)   { enum { LZ4M_static_assert = 1/(int)(!!(c)) }; }   /* use after variable declarations */

/*-************************************
*  Hash functions
**************************************/
static const U32 LZ4M_PRIME32_1 = 2654435761U;
static const U32 LZ4M_PRIME32_2 = 2246822519U;
static const U32 LZ4M_PRIME32_3 = 3266489917U;
static const U32 LZ4M_PRIME32_4 =  668265263U;
static const U32 LZ4M_PRIME32_5 =  374761393U;

static U32 LZ4M_hash4(U32 sequence, U32 tableType)
{
    return ((sequence * LZ4M_PRIME32_1) >> ((MINMATCH*8)-tableType));
}

static U32 LZ4M_hash4Ptr(const void* ptr, U32 tableType)
{
    return LZ4M_hash4(LZ4M_read32(ptr), tableType);
}

/*-************************************
*  Basic Types
**************************************/
typedef enum { notLimited = 0, limitedOutput = 1, fillOutput = 2 } limitedOutput_directive;
typedef enum { clearedTable = 0, byPtr, byU32, byU16 } tableType_t;

typedef enum { noDict = 0, withPrefix64k, usingExtDict, usingDictCtx } dict_directive;
typedef enum { noDictIssue = 0, dictSmall } dictIssue_directive;

typedef enum { endOnOutputSize = 0, endOnInputSize = 1 } endCondition_directive;
typedef enum { decode_full_block = 0, partial_decode = 1 } earlyEnd_directive;

/*-************************************
*  Local Structures and types
**************************************/
typedef struct LZ4M_stream_t_internal LZ4M_stream_t_internal;
struct LZ4M_stream_t_internal {
    U32 hashTable[1 << 12];
    U32 currentOffset;
    U32 tableType;
    const BYTE* dictionary;
    const LZ4M_stream_t_internal* dictCtx;
    U32 dictSize;
};

/* LZ4M_stream_t */
struct LZ4M_stream_t {
    LZ4M_stream_t_internal internal_donotuse;
};

/* LZ4M_streamDecode_t */
typedef struct {
    const BYTE* externalDict;
    size_t extDictSize;
    const BYTE* prefixEnd;
    size_t prefixSize;
} LZ4M_streamDecode_internal_t;
struct LZ4M_streamDecode_t {
    LZ4M_streamDecode_internal_t internal_donotuse;
};

/* *************************************
*  Compression
***************************************/

static int LZ4M_compress_generic(
        void* const ctx,
        const char* const src,
        char* const dst,
        const int srcSize,
        const int dstCapacity,
        const int acceleration)
{
    const BYTE* ip = (const BYTE*) src;
    const BYTE* const iend = ip + srcSize;
    
    BYTE* op = (BYTE*) dst;
    BYTE* const oend = op + dstCapacity;
    
    const BYTE* anchor = ip;
    const BYTE* const mflimit = iend - MFLIMIT;
    const BYTE* const matchlimit = iend - LASTLITERALS;
    
    /* Special cases */
    if (unlikely(srcSize <= LZ4M_minLength)) goto _last_literals;   /* Input too small, no compression */
    if (unlikely(dstCapacity <= 0)) return 0;                      /* Impossible to store anything */
    
    /* Main Loop */
    while (ip < mflimit) {
        const BYTE* match;
        {
            U32 const h = LZ4M_hash4Ptr(ip, 12);
            U32* const chainTable = ((U32*)ctx) + (1<<12);
            U32 const current = (U32)(ip-src);
            U32 matchIndex = ((U32*)ctx)[h];
            ((U32*)ctx)[h] = current;
            
            if (matchIndex + LZ4M_DISTANCE_MAX < current) goto _next_match;
            match = src + matchIndex;
            
            /* 4-byte match verification - LZ4M uses 4-byte granularity */
            if (LZ4M_read32(match) != LZ4M_read32(ip)) goto _next_match;
        }
        
        /* Catch up */
        while ((ip>anchor) && (match>src) && (ip[-1] == match[-1])) { ip--; match--; }
        
        {
            /* Encode Literal length */
            unsigned const litLength = (unsigned)(ip - anchor);
            BYTE* token = op++;
            if (op + litLength + (2 + 1 + LASTLITERALS) + (litLength/255) >= oend) return 0;   /* Check output limit */
            
            if (litLength >= RUN_MASK) {
                unsigned len = litLength - RUN_MASK;
                *token = (RUN_MASK<<ML_BITS);
                for(; len >= 255 ; len-=255) *op++ = 255;
                *op++ = (BYTE)len;
            } else {
                *token = (BYTE)(litLength<<ML_BITS);
            }
            
            /* Copy Literals */
            LZ4M_wildCopy8(op, anchor, op+litLength);
            op += litLength;
        }
        
_next_match:
        /* Start Counting */
        {
            /* LZ4M uses 4-byte granularity for matches */
            const BYTE* start = ip;
            const BYTE* const matchEnd = iend - LASTLITERALS;
            
            /* Find the longest match */
            size_t mLength = 0;
            while ((match+mLength < matchEnd) && (ip+mLength < matchEnd) && 
                   (LZ4M_read32(match+mLength) == LZ4M_read32(ip+mLength))) {
                mLength += 4;  /* 4-byte granularity */
            }
            
            /* Encode Match */
            if (mLength >= MINMATCH) {
                /* Encode Match Length */
                unsigned matchCode = (unsigned)mLength - MINMATCH;
                
                /* Encode Offset */
                U32 offset = (U32)(ip-match);
                if (offset > LZ4M_DISTANCE_MAX) return 0;  /* offset too large */
                
                ip += mLength;
                if (op + (1 + LASTLITERALS) + (matchCode>>8) >= oend) return 0;  /* Check output limit */
                
                if (matchCode >= ML_MASK) {
                    *op = (BYTE)(ML_MASK<<RUN_BITS);
                    matchCode -= ML_MASK;
                    LZ4M_write32(op, 0xFFFFFFFF);  /* Encode a long match */
                    while (matchCode >= 4*255) {
                        op += 4;
                        LZ4M_write32(op, 0xFFFFFFFF);
                        matchCode -= 4*255;
                    }
                    op += 4;
                    if (matchCode >= 255) { matchCode-=255; *op++ = 255; }
                    *op++ = (BYTE)matchCode;
                } else {
                    *op++ = (BYTE)(matchCode<<RUN_BITS);
                }
                
                /* Write Offset */
                LZ4M_writeLE16(op, (U16)offset); op+=2;
                
                /* Update table */
                anchor = ip;
                continue;
            }
        }
        
        /* No match found - skip forward 4 bytes as per paper's approach */
        ip += 4;
    }
    
_last_literals:
    /* Encode Last Literals */
    {
        size_t lastRunSize = (size_t)(iend - anchor);
        if (op + 1 + ((lastRunSize+255-RUN_MASK)/255) + lastRunSize > oend) return 0;  /* Check output limit */
        if (lastRunSize >= RUN_MASK) {
            size_t accumulator = lastRunSize - RUN_MASK;
            *op++ = RUN_MASK << ML_BITS;
            for(; accumulator >= 255 ; accumulator-=255) *op++ = 255;
            *op++ = (BYTE) accumulator;
        } else {
            *op++ = (BYTE)(lastRunSize<<ML_BITS);
        }
        memcpy(op, anchor, lastRunSize);
        op += lastRunSize;
    }
    
    /* End */
    return (int) (((char*)op) - dst);
}

/* LZ4M_compress_default() :
 * Compresses 'srcSize' bytes from buffer 'src'
 * into already allocated 'dst' buffer of size 'dstCapacity'.
 * Compression is guaranteed to succeed if 'dstCapacity' >= LZ4M_compressBound(srcSize).
 * It also runs faster, so it's a recommended setting.
 * If the function cannot compress 'src' into a more limited 'dst' budget,
 * compression stops *immediately*, and the function result is zero.
 * Note : as a consequence, 'dst' content is not valid.
 * Note 2 : This function is protected against buffer overflow scenarios (never writes outside 'dst' buffer).
 * Note 3 : srcSize must be > 0. If srcSize==0, the function will return 0.
 */
int LZ4M_compress_default(const char* src, char* dst, int srcSize, int dstCapacity)
{
    return LZ4M_compress_generic(NULL, src, dst, srcSize, dstCapacity, LZ4M_ACCELERATION_DEFAULT);
}

/* LZ4M_compress_fast() :
 * Same as LZ4M_compress_default(), but with 'acceleration' parameter.
 * The larger the acceleration value, the faster the algorithm, but also the lesser the compression.
 * It's a trade-off. It can be fine tuned, with each successive value providing roughly +~3% to speed.
 * An acceleration value of "1" is the same as regular LZ4M_compress_default()
 * Values <= 0 will be replaced by LZ4M_ACCELERATION_DEFAULT (currently == 1)
 */
int LZ4M_compress_fast(const char* src, char* dst, int srcSize, int dstCapacity, int acceleration)
{
    if (acceleration <= 0) acceleration = LZ4M_ACCELERATION_DEFAULT;
    return LZ4M_compress_generic(NULL, src, dst, srcSize, dstCapacity, acceleration);
}

/* LZ4M_compressBound() :
 * Provides the maximum size that LZ4M compression may output in a "worst case" scenario (input data not compressible)
 * This function is primarily useful for memory allocation purposes (destination buffer size).
 * Macro LZ4M_COMPRESSBOUND() is also provided for compilation-time evaluation (stack memory allocation for example).
 * Note that LZ4M_compress_default() compresses faster when dstCapacity is >= LZ4M_compressBound(srcSize)
 */
int LZ4M_compressBound(int inputSize)
{
    return inputSize + (inputSize/255) + 16;
}

/* *************************************
*  Decompression
***************************************/

static int LZ4M_decompress_generic(
                 const char* const src,
                 char* const dst,
                 int srcSize,
                 int outputSize)
{
    /* Local Variables */
    const BYTE* ip = (const BYTE*) src;
    const BYTE* const iend = ip + srcSize;
    
    BYTE* op = (BYTE*) dst;
    BYTE* const oend = op + outputSize;
    BYTE* cpy;
    
    const BYTE* const lowLimit = dst;
    
    /* Special cases */
    if (unlikely(srcSize == 0)) { return 0; }  /* Empty input */
    if (unlikely(outputSize == 0)) { return -1; } /* Invalid output size */
    
    /* Main Loop : decode sequences */
    while (ip < iend) {
        /* Get literal length */
        unsigned token = *ip++;
        unsigned litLength = token >> ML_BITS;
        
        if (litLength == RUN_MASK) {
            unsigned s;
            do {
                s = *ip++;
                litLength += s;
            } while (likely(ip < iend - RUN_MASK) & (s==255));
            if (unlikely(litLength > (unsigned)(oend-op))) return -1;  /* Output buffer overflow */
        }
        
        /* Copy literals */
        cpy = op+litLength;
        if (cpy > oend - WILDCOPYLENGTH) {
            if (cpy > oend) return -1;  /* Output buffer overflow */
            memcpy(op, ip, litLength);
            op += litLength;
            ip += litLength;
            break;  /* Necessarily EOF */
        }
        LZ4M_wildCopy8(op, ip, cpy);
        ip += litLength; op = cpy;
        
        /* Get offset */
        if (ip >= iend-2) return -1;  /* Offset outside valid range */
        {
            U16 offset = LZ4M_readLE16(ip); ip += 2;
            const BYTE* match = op - offset;
            if (match < lowLimit) return -1;  /* Offset outside valid range */
            
            /* Get matchlength */
            unsigned matchLength = token & ML_MASK;
            if (matchLength == ML_MASK) {
                unsigned s;
                do {
                    if (ip >= iend-WILDCOPYLENGTH) return -1;
                    s = *ip++;
                    matchLength += s;
                } while (s==255);
            }
            matchLength += MINMATCH;
            
            /* Copy match */
            if (unlikely(offset < MINMATCH)) {
                /* Offset within the last 4 bytes - LZ4M uses 4-byte granularity */
                op[0] = match[0];
                op[1] = match[1];
                op[2] = match[2];
                op[3] = match[3];
                match += 4; op += 4; matchLength -= 4;  /* Minimum match of 4 bytes */
            }
            
            cpy = op + matchLength;
            if (cpy > oend - WILDCOPYLENGTH) {
                if (cpy > oend) return -1;  /* Output buffer overflow */
                
                /* Copy in 4-byte chunks - LZ4M uses 4-byte granularity */
                while (op < cpy) {
                    *(U32*)op = *(U32*)match;
                    op += 4; match += 4;
                }
            } else {
                /* Use wildcopy for better performance */
                do {
                    *(U32*)op = *(U32*)match;  /* 4-byte granularity */
                    op += 4; match += 4;
                    *(U32*)op = *(U32*)match;
                    op += 4; match += 4;
                } while (op < cpy);
            }
            op = cpy;  /* Correction */
        }
    }
    
    /* End */
    return (int) (((char*)op)-dst);
}

int LZ4M_decompress_safe(const char* source, char* dest, int compressedSize, int maxDecompressedSize)
{
    return LZ4M_decompress_generic(source, dest, compressedSize, maxDecompressedSize);
}

/* Simple API for streaming compression */
LZ4MLIB_API LZ4M_stream_t* LZ4M_createStream(void)
{
    LZ4M_stream_t* lz4s = (LZ4M_stream_t*)ALLOC_AND_ZERO(sizeof(LZ4M_stream_t));
    return lz4s;
}

LZ4MLIB_API int LZ4M_freeStream(LZ4M_stream_t* LZ4M_stream)
{
    if (!LZ4M_stream) return 0;
    FREEMEM(LZ4M_stream);
    return 0;
}

/* Simple API for streaming decompression */
LZ4MLIB_API LZ4M_streamDecode_t* LZ4M_createStreamDecode(void)
{
    LZ4M_streamDecode_t* lz4s = (LZ4M_streamDecode_t*)ALLOC_AND_ZERO(sizeof(LZ4M_streamDecode_t));
    return lz4s;
}

LZ4MLIB_API int LZ4M_freeStreamDecode(LZ4M_streamDecode_t* LZ4M_stream)
{
    if (!LZ4M_stream) return 0;
    FREEMEM(LZ4M_stream);
    return 0;
}