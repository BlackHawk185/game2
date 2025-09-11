// BlockType.h - ID-based block type system
#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Block type IDs - simple, efficient, and network-friendly
namespace BlockID {
    constexpr uint8_t AIR = 0;
    constexpr uint8_t STONE = 1;
    constexpr uint8_t DIRT = 2;
    constexpr uint8_t GRASS = 3;
    // Decorative grass tuft model (instanced, non-voxel meshing)
    constexpr uint8_t DECOR_GRASS = 13;
    constexpr uint8_t TREE = 10;
    constexpr uint8_t LAMP = 11;
    constexpr uint8_t ROCK = 12;
    
    constexpr uint8_t MAX_BLOCK_TYPES = 255;
}

enum class BlockRenderType {
    VOXEL,    // Traditional meshed voxel blocks
    OBJ       // GPU instanced OBJ models
};

struct BlockTypeInfo {
    uint8_t id;
    std::string name;           // For debugging/display only
    BlockRenderType renderType;
    std::string assetPath;      // For OBJ blocks, path to the model file
    
    BlockTypeInfo(uint8_t blockID, const std::string& blockName, BlockRenderType type, const std::string& asset = "")
        : id(blockID), name(blockName), renderType(type), assetPath(asset) {}
};

class BlockTypeRegistry {
public:
    static BlockTypeRegistry& getInstance() {
        static BlockTypeRegistry instance;
        return instance;
    }
    
    // Register a new block type
    void registerBlockType(uint8_t id, const std::string& name, BlockRenderType renderType, const std::string& assetPath = "");
    
    // Get block type info by ID (primary method)
    const BlockTypeInfo* getBlockType(uint8_t id) const;
    
    // Get block name for debugging (should rarely be used)
    const std::string& getBlockName(uint8_t id) const;
    
    // Check if a block type exists
    bool hasBlockType(uint8_t id) const;
    
    // Get all registered block types
    const std::vector<BlockTypeInfo>& getAllBlockTypes() const { return m_blockTypes; }

private:
    BlockTypeRegistry();
    void initializeDefaultBlocks();
    
    std::vector<BlockTypeInfo> m_blockTypes;  // Simple array indexed by block ID
    static const std::string UNKNOWN_BLOCK_NAME;
};


