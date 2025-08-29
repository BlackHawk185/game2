/*
   Simple LZ4-compatible compression for game engine
   Provides LZ4 interface with basic compression suitable for voxel data
*/

#include "lz4.h"
#include <stdlib.h>
#include <string.h>

/*-************************************
*  Version and bounds
**************************************/
int LZ4_versionNumber(void) { 
    return LZ4_VERSION_NUMBER; 
}

const char* LZ4_versionString(void) { 
    return LZ4_VERSION_STRING; 
}

int LZ4_compressBound(int inputSize) {
    if (inputSize > LZ4_MAX_INPUT_SIZE) return 0;
    return inputSize + (inputSize / 255) + 16;
}

/*-************************************
*  Simple compression implementation
**************************************/
int LZ4_compress_default(const char* src, char* dst, int srcSize, int dstCapacity)
{
    if (!src || !dst || srcSize <= 0 || dstCapacity <= 0) {
        return 0;
    }
    
    // Simple run-length encoding for voxel data
    const unsigned char* input = (const unsigned char*)src;
    unsigned char* output = (unsigned char*)dst;
    
    int inputPos = 0;
    int outputPos = 0;
    
    while (inputPos < srcSize && outputPos < dstCapacity - 2) {
        unsigned char currentByte = input[inputPos];
        int runLength = 1;
        
        // Count consecutive identical bytes
        while (inputPos + runLength < srcSize && 
               input[inputPos + runLength] == currentByte && 
               runLength < 255) {
            runLength++;
        }
        
        if (runLength >= 3) {
            // Store as run: [255][runLength][value] for runs >= 3
            if (outputPos + 3 >= dstCapacity) break;
            output[outputPos++] = 255;  // Run marker
            output[outputPos++] = (unsigned char)runLength;
            output[outputPos++] = currentByte;
            inputPos += runLength;
        } else {
            // Store literal bytes
            if (outputPos + runLength >= dstCapacity) break;
            for (int i = 0; i < runLength; i++) {
                output[outputPos++] = input[inputPos++];
            }
        }
    }
    
    // If we couldn't compress everything, return 0 (compression failed)
    if (inputPos < srcSize) {
        return 0;
    }
    
    return outputPos;
}

/*-************************************
*  Simple decompression implementation
**************************************/
int LZ4_decompress_safe(const char* src, char* dst, int compressedSize, int dstCapacity)
{
    if (!src || !dst || compressedSize <= 0 || dstCapacity <= 0) {
        return -1;
    }
    
    const unsigned char* input = (const unsigned char*)src;
    unsigned char* output = (unsigned char*)dst;
    
    int inputPos = 0;
    int outputPos = 0;
    
    while (inputPos < compressedSize && outputPos < dstCapacity) {
        unsigned char control = input[inputPos++];
        
        if (control == 255 && inputPos + 1 < compressedSize) {
            // Run-length encoded sequence
            unsigned char runLength = input[inputPos++];
            unsigned char value = input[inputPos++];
            
            if (outputPos + runLength > dstCapacity) {
                return -1;  // Output buffer overflow
            }
            
            for (int i = 0; i < runLength; i++) {
                output[outputPos++] = value;
            }
        } else {
            // Literal byte
            if (outputPos >= dstCapacity) {
                return -1;  // Output buffer overflow
            }
            output[outputPos++] = control;
        }
    }
    
    return outputPos;
}
