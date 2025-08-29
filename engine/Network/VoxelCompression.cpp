// VoxelCompression.cpp - LZ4 compression for voxel data
#include "VoxelCompression.h"
#include <lz4.h>
#include <algorithm>
#include <iostream>

uint32_t VoxelCompression::compressLZ4(const uint8_t* input, uint32_t inputSize, std::vector<uint8_t>& output) {
    if (!input || inputSize == 0) {
        std::cerr << "LZ4 compression: Invalid input parameters" << std::endl;
        return 0;
    }
    
    // Calculate maximum compressed size
    uint32_t maxCompressedSize = getMaxCompressedSize(inputSize);
    output.resize(maxCompressedSize);
    
    // Perform LZ4 compression
    int compressedSize = LZ4_compress_default(
        reinterpret_cast<const char*>(input),
        reinterpret_cast<char*>(output.data()),
        static_cast<int>(inputSize),
        static_cast<int>(maxCompressedSize)
    );
    
    if (compressedSize <= 0) {
        std::cerr << "LZ4 compression failed" << std::endl;
        output.clear();
        return 0;
    }
    
    // Resize output to actual compressed size
    output.resize(compressedSize);
    
    std::cout << "LZ4 compression: " << inputSize << " -> " << compressedSize 
              << " bytes (" << (100.0f * compressedSize / inputSize) << "%)" << std::endl;
    
    return static_cast<uint32_t>(compressedSize);
}

bool VoxelCompression::decompressLZ4(const uint8_t* input, uint32_t inputSize, uint8_t* output, uint32_t outputSize) {
    if (!input || !output || inputSize == 0 || outputSize == 0) {
        std::cerr << "LZ4 decompression: Invalid input parameters" << std::endl;
        return false;
    }
    
    // Perform LZ4 decompression
    int decompressedSize = LZ4_decompress_safe(
        reinterpret_cast<const char*>(input),
        reinterpret_cast<char*>(output),
        static_cast<int>(inputSize),
        static_cast<int>(outputSize)
    );
    
    if (decompressedSize < 0) {
        std::cerr << "LZ4 decompression failed with error code: " << decompressedSize << std::endl;
        return false;
    }
    
    if (static_cast<uint32_t>(decompressedSize) != outputSize) {
        std::cerr << "LZ4 decompression: Size mismatch. Expected: " << outputSize 
                  << ", Got: " << decompressedSize << std::endl;
        return false;
    }
    
    std::cout << "LZ4 decompression: " << inputSize << " -> " << decompressedSize << " bytes" << std::endl;
    
    return true;
}

uint32_t VoxelCompression::getMaxCompressedSize(uint32_t inputSize) {
    return static_cast<uint32_t>(LZ4_compressBound(static_cast<int>(inputSize)));
}
