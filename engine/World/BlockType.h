// BlockType.h - String-based block type system with compile-time registration
#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>

enum class BlockRenderType {
    VOXEL,    // Traditional meshed voxel blocks
    OBJ       // GPU instanced OBJ models
};

struct BlockTypeInfo {
    std::string name;
    BlockRenderType renderType;
    std::string assetPath;  // For OBJ blocks, path to the model file
    uint8_t legacyID;       // For compatibility with existing numeric system
    
    BlockTypeInfo(const std::string& blockName, BlockRenderType type, const std::string& asset = "", uint8_t id = 0)
        : name(blockName), renderType(type), assetPath(asset), legacyID(id) {}
};

class BlockTypeRegistry {
public:
    static BlockTypeRegistry& getInstance() {
        static BlockTypeRegistry instance;
        return instance;
    }
    
    // Register a new block type
    void registerBlockType(const std::string& name, BlockRenderType renderType, const std::string& assetPath = "", uint8_t legacyID = 0);
    
    // Get block type info by name
    const BlockTypeInfo* getBlockType(const std::string& name) const;
    
    // Get block type info by legacy ID (for backward compatibility)
    const BlockTypeInfo* getBlockTypeByLegacyID(uint8_t id) const;
    
    // Get legacy ID by block name (for network compatibility)
    uint8_t getLegacyID(const std::string& name) const;
    
    // Check if a block type exists
    bool hasBlockType(const std::string& name) const;
    
    // Get all registered block types
    const std::unordered_map<std::string, BlockTypeInfo>& getAllBlockTypes() const { return m_blockTypes; }

private:
    BlockTypeRegistry();
    std::unordered_map<std::string, BlockTypeInfo> m_blockTypes;
    std::unordered_map<uint8_t, std::string> m_legacyIDMap;
    
    // Initialize default block types
    void initializeDefaultBlocks();
};

// Convenience functions
inline const BlockTypeInfo* getBlockType(const std::string& name) {
    return BlockTypeRegistry::getInstance().getBlockType(name);
}

inline bool isValidBlockType(const std::string& name) {
    return BlockTypeRegistry::getInstance().hasBlockType(name);
}

inline uint8_t getBlockLegacyID(const std::string& name) {
    return BlockTypeRegistry::getInstance().getLegacyID(name);
}
