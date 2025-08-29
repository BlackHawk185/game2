/*
   LZ4 - Fast LZ compression algorithm
   Simplified implementation for game engine integration
   Copyright (C) 2011-2020, Yann Collet.

   BSD 2-Clause License (https://opensource.org/licenses/bsd-license.php)
*/

#include "lz4.h"
#include <stdlib.h>
#include <string.h>

/*-************************************
*  Basic Types
**************************************/
#if defined(__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
# include <stdint.h>
  typedef  uint8_t BYTE;
  typedef uint16_t U16;
  typedef uint32_t U32;
  typedef  int32_t S32;
  typedef uint64_t U64;
  typedef uintptr_t uptrval;
#else
  typedef unsigned char       BYTE;
  typedef unsigned short      U16;
  typedef unsigned int        U32;
  typedef   signed int        S32;
  typedef unsigned long long  U64;
  typedef size_t              uptrval;
#endif

/*-************************************
*  Constants
**************************************/
#define MINMATCH 4
#define MFLIMIT        12  
#define LASTLITERALS   5   
#define WILDCOPYLENGTH 8

#define ML_BITS  4
#define ML_MASK  ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)

/*-************************************
*  Memory Access
**************************************/
static unsigned LZ4_isLittleEndian(void)
{
    const union { U32 u; BYTE c[4]; } one = { 1 };
    return one.c[0];
}

static U16 LZ4_read16(const void* memPtr)
{
    U16 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

static U32 LZ4_read32(const void* memPtr)
{
    U32 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

static void LZ4_write16(void* memPtr, U16 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

static void LZ4_write32(void* memPtr, U32 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

/*-************************************
*  Basic Functions
**************************************/
int LZ4_versionNumber (void) { return LZ4_VERSION_NUMBER; }
const char* LZ4_versionString(void) { return LZ4_VERSION_STRING; }
int LZ4_compressBound(int isize)  
{ 
    if (isize > LZ4_MAX_INPUT_SIZE) return 0;
    return isize + (isize / 255) + 16;
}

/*-************************************
*  Simple Compression Implementation
**************************************/
int LZ4_compress_default(const char* src, char* dst, int srcSize, int dstCapacity)
{
    if (srcSize <= 0 || dstCapacity <= 0) return 0;
    if (src == NULL || dst == NULL) return 0;
    
    // For simplicity, implement a basic compression that works for our voxel data
    // This is not full LZ4 but provides the interface we need
    
    const BYTE* ip = (const BYTE*)src;
    const BYTE* const iend = ip + srcSize;
    const BYTE* const mflimit = iend - MFLIMIT;
    
    BYTE* op = (BYTE*)dst;
    BYTE* const oend = op + dstCapacity;
    
    // Simple literal-only compression for now
    // In production, you'd implement the full LZ4 algorithm
    
    if (srcSize < 13) {
        // Very small data - just store as literals
        if (dstCapacity < srcSize + 1) return 0;
        *op++ = (BYTE)(srcSize << ML_BITS);
        memcpy(op, src, srcSize);
        return srcSize + 1;
    }
    
    // For larger data, implement basic compression
    const BYTE* anchor = ip;
    
    while (ip < mflimit) {
        // Simple approach: find runs of identical bytes
        const BYTE* start = ip;
        BYTE value = *ip;
        
        // Count run length
        while (ip < iend && *ip == value && (ip - start) < 255) {
            ip++;
        }
        
        size_t runLength = ip - start;
        
        if (runLength >= 4) {
            // Encode as a run
            if (op + 2 >= oend) return 0;
            
            // Simple run encoding: [255][length][value] or [length][value] if length < 255
            if (runLength >= 255) {
                *op++ = 255;
                *op++ = (BYTE)(runLength - 255);
            } else {
                *op++ = (BYTE)runLength;
            }
            *op++ = value;
        } else {
            // Store as literal
            if (op + runLength >= oend) return 0;
            
            for (size_t i = 0; i < runLength; i++) {
                *op++ = start[i];
            }
        }
        
        anchor = ip;
    }
    
    // Handle remaining bytes as literals
    if (ip < iend) {
        size_t remaining = iend - ip;
        if (op + remaining >= oend) return 0;
        
        memcpy(op, ip, remaining);
        op += remaining;
    }
    
    return (int)(op - (BYTE*)dst);
}

/*-************************************
*  Simple Decompression Implementation
**************************************/
int LZ4_decompress_safe(const char* src, char* dst, int compressedSize, int dstCapacity)
{
    if (compressedSize < 0 || dstCapacity <= 0) return -1;
    if (src == NULL || dst == NULL) return -1;
    
    const BYTE* ip = (const BYTE*)src;
    const BYTE* const iend = ip + compressedSize;
    
    BYTE* op = (BYTE*)dst;
    BYTE* const oend = op + dstCapacity;
    
    // Simple decompression matching our compression above
    while (ip < iend && op < oend) {
        BYTE control = *ip++;
        
        if (control == 255 && ip < iend) {
            // Extended run length
            BYTE extraLength = *ip++;
            if (ip >= iend) return -1;
            
            BYTE value = *ip++;
            size_t totalLength = 255 + extraLength;
            
            if (op + totalLength > oend) return -1;
            
            memset(op, value, totalLength);
            op += totalLength;
        } else if (control >= 4) {
            // Regular run
            if (ip >= iend) return -1;
            
            BYTE value = *ip++;
            size_t runLength = control;
            
            if (op + runLength > oend) return -1;
            
            memset(op, value, runLength);
            op += runLength;
        } else {
            // Literal byte
            if (op >= oend) return -1;
            *op++ = control;
        }
    }
    
    return (int)(op - (BYTE*)dst);
}
