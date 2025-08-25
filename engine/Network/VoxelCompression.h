// VoxelCompression.h - Simple compression for voxel chunk data
#pragma once
#include <cstdint>
#include <vector>

/**
 * Simple Run-Length Encoding compression optimized for voxel data.
 * Since voxel chunks often have large runs of the same block type (air, stone, etc),
 * RLE can achieve good compression ratios with minimal CPU overhead.
 */
class VoxelCompression {
public:
    /**
     * Compress voxel data using Run-Length Encoding
     * @param input - Raw voxel data (32*32*32 = 32768 bytes)
     * @param inputSize - Size of input data 
     * @param output - Vector to store compressed data
     * @return Size of compressed data, or 0 if compression failed
     */
    static uint32_t compressRLE(const uint8_t* input, uint32_t inputSize, std::vector<uint8_t>& output);
    
    /**
     * Decompress RLE-compressed voxel data
     * @param input - Compressed data
     * @param inputSize - Size of compressed data
     * @param output - Buffer to store decompressed data (must be pre-allocated)
     * @param outputSize - Expected size of decompressed data (32768 for full chunk)
     * @return true if decompression succeeded
     */
    static bool decompressRLE(const uint8_t* input, uint32_t inputSize, uint8_t* output, uint32_t outputSize);
    
private:
    // RLE format: [count][value][count][value]...
    // If count >= 128, it's a run of (count-128) identical values
    // If count < 128, it's a literal sequence of count different values
    static constexpr uint8_t RLE_RUN_FLAG = 128;
    static constexpr uint8_t MAX_RUN_LENGTH = 127;
    static constexpr uint8_t MAX_LITERAL_LENGTH = 127;
};
