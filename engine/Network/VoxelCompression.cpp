// VoxelCompression.cpp - Simple RLE compression for voxel data
#include "VoxelCompression.h"
#include <algorithm>
#include <iostream>

uint32_t VoxelCompression::compressRLE(const uint8_t* input, uint32_t inputSize, std::vector<uint8_t>& output) {
    if (!input || inputSize == 0) {
        return 0;
    }
    
    output.clear();
    output.reserve(inputSize); // Conservative estimate
    
    uint32_t pos = 0;
    
    while (pos < inputSize) {
        uint8_t currentValue = input[pos];
        uint32_t runLength = 1;
        
        // Count consecutive identical values
        while (pos + runLength < inputSize && 
               input[pos + runLength] == currentValue && 
               runLength < MAX_RUN_LENGTH) {
            runLength++;
        }
        
        if (runLength >= 3) {
            // Use run-length encoding for runs of 3 or more
            output.push_back(static_cast<uint8_t>(runLength + RLE_RUN_FLAG));
            output.push_back(currentValue);
        } else {
            // For short runs, collect literals
            uint32_t literalStart = pos;
            uint32_t literalLength = runLength;
            
            // Extend literal run while it's efficient
            while (pos + literalLength < inputSize && literalLength < MAX_LITERAL_LENGTH) {
                // Check if next section would be better as a run
                uint32_t nextPos = pos + literalLength;
                if (nextPos + 2 < inputSize && 
                    input[nextPos] == input[nextPos + 1] && 
                    input[nextPos] == input[nextPos + 2]) {
                    break; // Stop literal run before an efficient RLE run
                }
                literalLength++;
            }
            
            // Output literal sequence
            output.push_back(static_cast<uint8_t>(literalLength));
            for (uint32_t i = 0; i < literalLength; i++) {
                output.push_back(input[pos + i]);
            }
        }
        
        pos += (runLength > 1) ? runLength : 
               (output[output.size() - (output.back() + 1)] < RLE_RUN_FLAG) ? 
               output[output.size() - (output.back() + 1)] : runLength;
    }
    
    return static_cast<uint32_t>(output.size());
}

bool VoxelCompression::decompressRLE(const uint8_t* input, uint32_t inputSize, uint8_t* output, uint32_t outputSize) {
    if (!input || !output || inputSize == 0 || outputSize == 0) {
        return false;
    }
    
    uint32_t inPos = 0;
    uint32_t outPos = 0;
    
    while (inPos < inputSize && outPos < outputSize) {
        if (inPos >= inputSize) {
            std::cerr << "RLE decompression: Unexpected end of input" << std::endl;
            return false;
        }
        
        uint8_t control = input[inPos++];
        
        if (control >= RLE_RUN_FLAG) {
            // Run-length encoded sequence
            uint32_t runLength = control - RLE_RUN_FLAG;
            
            if (inPos >= inputSize) {
                std::cerr << "RLE decompression: Missing run value" << std::endl;
                return false;
            }
            
            uint8_t value = input[inPos++];
            
            if (outPos + runLength > outputSize) {
                std::cerr << "RLE decompression: Output buffer overflow" << std::endl;
                return false;
            }
            
            for (uint32_t i = 0; i < runLength; i++) {
                output[outPos++] = value;
            }
        } else {
            // Literal sequence
            uint32_t literalLength = control;
            
            if (inPos + literalLength > inputSize) {
                std::cerr << "RLE decompression: Not enough input for literal sequence" << std::endl;
                return false;
            }
            
            if (outPos + literalLength > outputSize) {
                std::cerr << "RLE decompression: Output buffer overflow in literal" << std::endl;
                return false;
            }
            
            for (uint32_t i = 0; i < literalLength; i++) {
                output[outPos++] = input[inPos++];
            }
        }
    }
    
    if (outPos != outputSize) {
        std::cerr << "RLE decompression: Output size mismatch. Expected: " << outputSize 
                  << ", Got: " << outPos << std::endl;
        return false;
    }
    
    return true;
}
