// BlockType.cpp - Implementation of string-based block type system
#include "BlockType.h"
#include <iostream>

void BlockTypeRegistry::registerBlockType(const std::string& name, BlockRenderType renderType, const std::string& assetPath, uint8_t legacyID) {
    m_blockTypes.emplace(name, BlockTypeInfo(name, renderType, assetPath, legacyID));
    if (legacyID != 0) {
        m_legacyIDMap[legacyID] = name;
    }
    std::cout << "Registered block type: " << name << " (legacy ID: " << (int)legacyID << ")" << std::endl;
}

const BlockTypeInfo* BlockTypeRegistry::getBlockType(const std::string& name) const {
    auto it = m_blockTypes.find(name);
    return (it != m_blockTypes.end()) ? &it->second : nullptr;
}

const BlockTypeInfo* BlockTypeRegistry::getBlockTypeByLegacyID(uint8_t id) const {
    auto it = m_legacyIDMap.find(id);
    if (it != m_legacyIDMap.end()) {
        return getBlockType(it->second);
    }
    return nullptr;
}

bool BlockTypeRegistry::hasBlockType(const std::string& name) const {
    return m_blockTypes.find(name) != m_blockTypes.end();
}

uint8_t BlockTypeRegistry::getLegacyID(const std::string& name) const {
    auto it = m_blockTypes.find(name);
    return (it != m_blockTypes.end()) ? it->second.legacyID : 0;
}

BlockTypeRegistry::BlockTypeRegistry() {
    initializeDefaultBlocks();
}

void BlockTypeRegistry::initializeDefaultBlocks() {
    // Register default block types
    registerBlockType("air", BlockRenderType::VOXEL, "", 0);
    registerBlockType("stone", BlockRenderType::VOXEL, "", 1);
    registerBlockType("dirt", BlockRenderType::VOXEL, "", 2);
    registerBlockType("grass", BlockRenderType::VOXEL, "", 3);
    
    // Example OBJ block types (you can add OBJ files to assets/models/)
    registerBlockType("tree", BlockRenderType::OBJ, "assets/models/tree.obj", 10);
    registerBlockType("lamp", BlockRenderType::OBJ, "assets/models/lamp.obj", 11);
    registerBlockType("rock", BlockRenderType::OBJ, "assets/models/rock.obj", 12);
    
    std::cout << "BlockTypeRegistry initialized with " << m_blockTypes.size() << " block types" << std::endl;
}
