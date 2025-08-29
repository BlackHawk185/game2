#include <iostream>
#include <vector>
#include <cstring>
#include "../libs/lz4/lz4.h"
#include "../engine/Network/VoxelCompression.h"

int main() {
    std::cout << "Testing LZ4 compression integration..." << std::endl;
    
    // Test data similar to voxel chunk
    const uint32_t CHUNK_SIZE = 32 * 32 * 32;
    std::vector<uint8_t> originalData(CHUNK_SIZE);
    
    // Fill with pattern similar to voxel data (mostly air with some blocks)
    for (uint32_t i = 0; i < CHUNK_SIZE; i++) {
        if (i < CHUNK_SIZE / 4) {
            originalData[i] = 1; // Stone
        } else if (i < CHUNK_SIZE / 2) {
            originalData[i] = 2; // Dirt
        } else {
            originalData[i] = 0; // Air
        }
    }
    
    std::cout << "Original data size: " << originalData.size() << " bytes" << std::endl;
    
    // Test compression
    std::vector<uint8_t> compressedData;
    uint32_t compressedSize = VoxelCompression::compressLZ4(
        originalData.data(), 
        originalData.size(), 
        compressedData
    );
    
    if (compressedSize > 0) {
        std::cout << "Compression successful!" << std::endl;
        std::cout << "Compressed size: " << compressedSize << " bytes" << std::endl;
        std::cout << "Compression ratio: " << (100.0f * compressedSize / originalData.size()) << "%" << std::endl;
        
        // Test decompression
        std::vector<uint8_t> decompressedData(CHUNK_SIZE);
        bool success = VoxelCompression::decompressLZ4(
            compressedData.data(),
            compressedSize,
            decompressedData.data(),
            CHUNK_SIZE
        );
        
        if (success) {
            std::cout << "Decompression successful!" << std::endl;
            
            // Verify data integrity
            if (memcmp(originalData.data(), decompressedData.data(), CHUNK_SIZE) == 0) {
                std::cout << "✅ Data integrity verified - compression/decompression working correctly!" << std::endl;
            } else {
                std::cout << "❌ Data integrity check failed!" << std::endl;
                return 1;
            }
        } else {
            std::cout << "❌ Decompression failed!" << std::endl;
            return 1;
        }
    } else {
        std::cout << "❌ Compression failed!" << std::endl;
        return 1;
    }
    
    // Test LZ4 bounds function
    int maxSize = LZ4_compressBound(CHUNK_SIZE);
    std::cout << "LZ4_compressBound for " << CHUNK_SIZE << " bytes: " << maxSize << " bytes" << std::endl;
    
    std::cout << "\n🎉 All LZ4 compression tests passed!" << std::endl;
    return 0;
}
