/*
 *  LZ4M - Fast LZ compression algorithm with 4-byte granularity
 *  Header File
 *  Based on LZ4 by Yann Collet
 *  Copyright (c) Yann Collet. All rights reserved.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
    - LZ4 homepage : http://www.lz4.org
    - LZ4 source repository : https://github.com/lz4/lz4
*/
#if defined (__cplusplus)
extern "C" {
#endif

#ifndef LZ4M_H_2983827168210
#define LZ4M_H_2983827168210

/* --- Dependency --- */
#include <stddef.h>   /* size_t */


/**
  Introduction

  LZ4M is a modified version of LZ4 that uses 4-byte granularity for scanning and matching.
  This modification improves compression ratio for certain types of data, especially
  when the data has 4-byte alignment patterns.

  The LZ4M compression library provides in-memory compression and decompression functions.
  It gives full buffer control to user.
  Compression can be done in:
    - a single step (described as Simple Functions)
    - a single step, reusing a context (described in Advanced Functions)
    - unbounded multiple steps (described as Streaming compression)

  lz4m.h generates and decodes LZ4M-compressed blocks.
  Decompressing such a compressed block requires additional metadata.
  Exact metadata depends on exact decompression function.
  For the typical case of LZ4M_decompress_safe(),
  metadata includes block's compressed size, and maximum bound of decompressed size.
  Each application is free to encode and pass such metadata in whichever way it wants.

  lz4m.h only handle blocks, it can not generate Frames.
*/

/*^***************************************************************
*  Export parameters
*****************************************************************/
/*
*  LZ4_DLL_EXPORT :
*  Enable exporting of functions when building a Windows DLL
*  LZ4LIB_VISIBILITY :
*  Control library symbols visibility.
*/
#ifndef LZ4LIB_VISIBILITY
#  if defined(__GNUC__) && (__GNUC__ >= 4)
#    define LZ4LIB_VISIBILITY __attribute__ ((visibility ("default")))
#  else
#    define LZ4LIB_VISIBILITY
#  endif
#endif
#if defined(LZ4_DLL_EXPORT) && (LZ4_DLL_EXPORT==1)
#  define LZ4MLIB_API __declspec(dllexport) LZ4LIB_VISIBILITY
#elif defined(LZ4_DLL_IMPORT) && (LZ4_DLL_IMPORT==1)
#  define LZ4MLIB_API __declspec(dllimport) LZ4LIB_VISIBILITY /* It isn't required but allows to generate better code, saving a function pointer load from the IAT and an indirect jump.*/
#else
#  define LZ4MLIB_API LZ4LIB_VISIBILITY
#endif

/*------   Version   ------*/
#define LZ4M_VERSION_MAJOR    1    /* for breaking interface changes  */
#define LZ4M_VERSION_MINOR    0    /* for new (non-breaking) interface capabilities */
#define LZ4M_VERSION_RELEASE  0    /* for tweaks, bug-fixes, or development */

#define LZ4M_VERSION_NUMBER (LZ4M_VERSION_MAJOR *100*100 + LZ4M_VERSION_MINOR *100 + LZ4M_VERSION_RELEASE)

/*-************************************
*  Simple Functions
**************************************/
/*! LZ4M_compress_default() :
    Compresses 'srcSize' bytes from buffer 'src'
    into already allocated 'dst' buffer of size 'dstCapacity'.
    Compression is guaranteed to succeed if 'dstCapacity' >= LZ4M_compressBound(srcSize).
    It also runs faster, so it's a recommended setting.
    If the function cannot compress 'src' into a more limited 'dst' budget,
    compression stops *immediately*, and the function result is zero.
    Note : as a consequence, 'dst' content is not valid.
    Note 2 : This function is protected against buffer overflow scenarios (never writes outside 'dst' buffer, nor read outside 'source' buffer).
        srcSize : max supported value is LZ4_MAX_INPUT_SIZE.
        dstCapacity : size of buffer 'dst' (which must be already allocated)
        return  : the number of bytes written into buffer 'dst' (necessarily <= dstCapacity)
                  or 0 if compression fails */
LZ4MLIB_API int LZ4M_compress_default(const char* src, char* dst, int srcSize, int dstCapacity);

/*! LZ4M_decompress_safe() :
    compressedSize : is the exact complete size of the compressed block.
    dstCapacity : is the size of destination buffer, which must be already allocated.
    return : the number of bytes decompressed into destination buffer (necessarily <= dstCapacity)
             If destination buffer is not large enough, decoding will stop and output an error code (negative value).
             If the source stream is detected malformed, the function will stop decoding and return a negative result.
             This function is protected against malicious data packets.
*/
LZ4MLIB_API int LZ4M_decompress_safe (const char* src, char* dst, int compressedSize, int dstCapacity);


/*-************************************
*  Advanced Functions
**************************************/
#define LZ4M_MAX_INPUT_SIZE        0x7E000000   /* 2 113 929 216 bytes */
#define LZ4M_COMPRESSBOUND(isize)  ((unsigned)(isize) > (unsigned)LZ4M_MAX_INPUT_SIZE ? 0 : (isize) + ((isize)/255) + 16)

/*! LZ4M_compressBound() :
    Provides the maximum size that LZ4 compression may output in a "worst case" scenario (input data not compressible)
    This function is primarily useful for memory allocation purposes (destination buffer size).
    Macro LZ4M_COMPRESSBOUND() is also provided for compilation-time evaluation (stack memory allocation for example).
    Note that LZ4M_compress_default() compresses faster when dstCapacity is >= LZ4M_compressBound(srcSize)
        inputSize  : max supported value is LZ4M_MAX_INPUT_SIZE
        return : maximum output size in a worst case scenario
        or 0, if input size is incorrect (too large or negative)
*/
LZ4MLIB_API int LZ4M_compressBound(int inputSize);

/*! LZ4M_compress_fast() :
    Same as LZ4M_compress_default(), but allows selection of "acceleration" factor.
    The larger the acceleration value, the faster the algorithm, but also the lesser the compression.
    It's a trade-off. It can be fine tuned, with each successive value providing roughly +~3% to speed.
    An acceleration value of "1" is the same as regular LZ4M_compress_default()
    Values <= 0 will be replaced by LZ4M_ACCELERATION_DEFAULT (currently == 1, see lz4m.c).
*/
LZ4MLIB_API int LZ4M_compress_fast (const char* src, char* dst, int srcSize, int dstCapacity, int acceleration);


/*! LZ4M_compress_fast_extState() :
 *  Same as LZ4M_compress_fast(), using an externally allocated memory space for its state.
 *  Use LZ4M_sizeofState() to know how much memory must be allocated,
 *  and allocate it on 8-bytes boundaries (using `malloc()` typically).
 *  Then, provide this buffer as `void* state` to compression function.
 */
LZ4MLIB_API int LZ4M_sizeofState(void);
LZ4MLIB_API int LZ4M_compress_fast_extState (void* state, const char* src, char* dst, int srcSize, int dstCapacity, int acceleration);


/*! LZ4M_compress_destSize() :
 *  Reverse the logic : compresses as much data as possible from 'src' buffer
 *  into already allocated buffer 'dst', of size >= 'targetDstSize'.
 *  This function either compresses the entire 'src' content into 'dst' if it's large enough,
 *  or fills 'dst' buffer completely with as much data as possible from 'src'.
 *  note: acceleration parameter is fixed to "default".
 *
 * *srcSizePtr : will be modified to indicate how many bytes where read from 'src' to fill 'dst'.
 *               New value is necessarily <= input value.
 * @return : Nb bytes written into 'dst' (necessarily <= targetDstSize)
 *           or 0 if compression fails.
*/
LZ4MLIB_API int LZ4M_compress_destSize (const char* src, char* dst, int* srcSizePtr, int targetDstSize);


/*-************************************
*  Streaming Compression Functions
**************************************/
typedef union LZ4M_stream_u LZ4M_stream_t;  /* incomplete type (defined later) */

/*! LZ4M_createStream() and LZ4M_freeStream() :
 *  LZ4M_createStream() will allocate and initialize an `LZ4M_stream_t` structure.
 *  LZ4M_freeStream() releases its memory.
 */
LZ4MLIB_API LZ4M_stream_t* LZ4M_createStream(void);
LZ4MLIB_API int           LZ4M_freeStream (LZ4M_stream_t* streamPtr);

/*! LZ4M_resetStream() :
 *  An LZ4M_stream_t structure can be allocated once and re-used multiple times.
 *  Use this function to start compressing a new stream.
 */
LZ4MLIB_API void LZ4M_resetStream (LZ4M_stream_t* streamPtr);

/*! LZ4M_loadDict() :
 *  Use this function to load a static dictionary into LZ4M_stream_t.
 *  Any previous data will be forgotten, only 'dictionary' will remain in memory.
 *  Loading a size of 0 is allowed, and is the same as reset.
 * @return : dictionary size, in bytes (necessarily <= 64 KB)
 */
LZ4MLIB_API int LZ4M_loadDict (LZ4M_stream_t* streamPtr, const char* dictionary, int dictSize);

/*! LZ4M_compress_fast_continue() :
 *  Compress 'src' content using data from previously compressed blocks, for better compression ratio.
 *  'dst' buffer must be already allocated.
 *  If dstCapacity >= LZ4M_compressBound(srcSize), compression is guaranteed to succeed, and runs faster.
 * @return : size of compressed block or 0 if there is an error (typically, cannot fit into 'dst').
 *  Note 1 : Each invocation to LZ4M_compress_fast_continue() generates a new block.
 *           Each block has precise boundaries.
 *           Each block must be decompressed separately, calling LZ4M_decompress_*() with relevant metadata.
 *           It's not possible to append blocks together and expect a single invocation of LZ4M_decompress_*() to decompress them together.
 *  Note 2 : The previous 64KB of source data is automatically loaded into internal buffer.
 *           You don't have to worry about creating overlapping blocks,
 *           if previous data is not available, or not useful enough, it will be ignored and replaced by incoming data.
 *  Note 3 : If input buffer is not contiguous, you can use LZ4M_saveDict() to ensure continuity.
 *  Note 4 : When input is structured as a double-buffer, each buffer can be freely compressed.
 *  Note 5 : If the block is not compressible, the function will return 0, and you must use other means to transport uncompressed data.
 *           In this case, consider using LZ4M_compress_destSize(), which is specifically designed for this case.
 *  Note 6 : If previous compressed block is not needed anymore, you can save memory by using LZ4M_resetStream()
 */
LZ4MLIB_API int LZ4M_compress_fast_continue (LZ4M_stream_t* streamPtr, const char* src, char* dst, int srcSize, int dstCapacity, int acceleration);

/*! LZ4M_saveDict() :
 *  If last 64KB data cannot be guaranteed to remain available at its current memory location,
 *  save it into a safer place (char* safeBuffer).
 *  This is schematically equivalent to a memcpy() followed by LZ4M_loadDict(),
 *  but is much faster, because LZ4M_saveDict() doesn't need to rebuild tables.
 * @return : saved dictionary size in bytes (necessarily <= maxDictSize), or 0 if error.
 */
LZ4MLIB_API int LZ4M_saveDict (LZ4M_stream_t* streamPtr, char* safeBuffer, int maxDictSize);


/*-**********************************************
*  Streaming Decompression Functions
************************************************/

#define LZ4M_DECODER_RING_BUFFER_SIZE(maxBlockSize) (65536 + 14 + (maxBlockSize))  /* for static allocation; maxBlockSize presumed <= 16 KB */

typedef struct { unsigned char internal[LZ4M_DECODER_RING_BUFFER_SIZE(LZ4M_MAX_INPUT_SIZE)]; } LZ4M_streamDecode_t;
/*
 * LZ4M_streamDecode_t
 * information structure to track an LZ4 stream.
 * init this structure content using LZ4M_setStreamDecode() before first use.
 * note : only use in association with static linking !
 *        this definition is not API/ABI safe,
 *        it may change in a future version !
 */


/*! LZ4M_createStreamDecode() and LZ4M_freeStreamDecode() :
 *  creation / destruction of streaming decompression tracking structure.
 *  A tracking structure can be re-used multiple times.
 */
LZ4MLIB_API LZ4M_streamDecode_t* LZ4M_createStreamDecode(void);
LZ4MLIB_API int                 LZ4M_freeStreamDecode (LZ4M_streamDecode_t* LZ4M_stream);


/*! LZ4M_setStreamDecode() :
 *  An LZ4M_streamDecode_t structure must be initialized at least once.
 *  This is automatically done when invoking LZ4M_createStreamDecode(),
 *  but should be done explicitly when the structure was created directly in user program.
 *
 *  Use LZ4M_setStreamDecode() to properly initialize a newly created structure.
 *  It can also be used to quickly reset an existing structure.
 *
 *  @return : 1 if OK, 0 if error
 */
LZ4MLIB_API int LZ4M_setStreamDecode (LZ4M_streamDecode_t* LZ4M_streamDecode, const char* dictionary, int dictSize);

/*! LZ4M_decompress_safe_continue() :
 *  These decoding functions allow decompression of consecutive blocks in "streaming" mode.
 *  A block is an unsplittable entity, it must be presented entirely to a decompression function.
 *  Decompression functions only accepts one block at a time.
 *  The last 64KB of previously decoded data *must* remain available and unmodified at the memory position where they were decoded.
 *  If less than 64KB of data has been decoded, all the data must be present.
 *
 *  Special : if decompression side sets a ring buffer, it must respect one of the following conditions :
 *  - Decompression buffer size is _at least_ LZ4M_decoderRingBufferSize(maxBlockSize).
 *    maxBlockSize is the maximum size of any single block. It can have any value > 16 bytes.
 *    In which case, encoding and decoding buffers do not need to be synchronized.
 *    Actually, data can be produced by any source compliant with LZ4 format specification, and respecting maxBlockSize.
 *  - Synchronized mode :
 *    Decompression buffer size is _exactly_ the same as compression buffer size,
 *    and follows exactly same update rule (block boundaries at same positions),
 *    and decoding function is provided with exact decompressed size of each block (exception for last block of the stream),
 *    _then_ decoding & encoding ring buffer can have any size, including small ones ( < 64 KB).
 *  - Decompression buffer is larger than encoding buffer, by a minimum of maxBlockSize more bytes.
 *    In which case, encoding and decoding buffers do not need to be synchronized,
 *    and encoding ring buffer can have any size, including small ones ( < 64 KB).
 *
 *  Whenever these conditions are not possible,
 *  save the last 64KB of decoded data into a safe buffer where it can't be modified during decompression,
 *  then indicate where this data is saved using LZ4M_setStreamDecode(), before decompressing next block.
*/
LZ4MLIB_API int LZ4M_decompress_safe_continue (LZ4M_streamDecode_t* LZ4M_streamDecode, const char* src, char* dst, int srcSize, int dstCapacity);


/*! LZ4M_decompress_safe_usingDict() :
 *  These decoding functions work the same as
 *  a combination of LZ4M_setStreamDecode() followed by LZ4M_decompress_safe_continue()
 *  They are stand-alone, and don't need an LZ4M_streamDecode_t structure.
 *  Dictionary is presumed stable : it must remain accessible and unmodified during next decompression.
 * @return : size of regenerated data, or a negative value if decompression failed
 */
LZ4MLIB_API int LZ4M_decompress_safe_usingDict (const char* src, char* dst, int srcSize, int dstCapacity, const char* dictStart, int dictSize);


#endif /* LZ4M_H_2983827168210 */

#if defined (__cplusplus)
}
#endif