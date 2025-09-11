// BlockType.cpp - Implementation of ID-based block type system
#include "BlockType.h"
#include <iostream>

const std::string BlockTypeRegistry::UNKNOWN_BLOCK_NAME = "unknown";

void BlockTypeRegistry::registerBlockType(uint8_t id, const std::string& name, BlockRenderType renderType, const std::string& assetPath) {
    // Ensure we have enough space in the vector
    if (id >= m_blockTypes.size()) {
        m_blockTypes.resize(id + 1, BlockTypeInfo(0, "", BlockRenderType::VOXEL));
    }
    
    m_blockTypes[id] = BlockTypeInfo(id, name, renderType, assetPath);
    std::cout << "Registered block type: " << name << " (ID: " << (int)id << ")" << std::endl;
}

const BlockTypeInfo* BlockTypeRegistry::getBlockType(uint8_t id) const {
    if (id < m_blockTypes.size() && !m_blockTypes[id].name.empty()) {
        return &m_blockTypes[id];
    }
    return nullptr;
}

const std::string& BlockTypeRegistry::getBlockName(uint8_t id) const {
    if (id < m_blockTypes.size() && !m_blockTypes[id].name.empty()) {
        return m_blockTypes[id].name;
    }
    return UNKNOWN_BLOCK_NAME;
}

bool BlockTypeRegistry::hasBlockType(uint8_t id) const {
    return id < m_blockTypes.size() && !m_blockTypes[id].name.empty();
}

BlockTypeRegistry::BlockTypeRegistry() {
    m_blockTypes.reserve(BlockID::MAX_BLOCK_TYPES);
    initializeDefaultBlocks();
}

void BlockTypeRegistry::initializeDefaultBlocks() {
    // Register default block types using the new ID-based system
    registerBlockType(BlockID::AIR, "air", BlockRenderType::VOXEL);
    registerBlockType(BlockID::STONE, "stone", BlockRenderType::VOXEL);
    registerBlockType(BlockID::DIRT, "dirt", BlockRenderType::VOXEL);
    registerBlockType(BlockID::GRASS, "grass", BlockRenderType::VOXEL);
    // Decorative instanced grass from GLB (treated as model, not meshed)
    registerBlockType(BlockID::DECOR_GRASS, "decor_grass", BlockRenderType::OBJ, "assets/models/grass.glb");
    registerBlockType(BlockID::TREE, "tree", BlockRenderType::OBJ, "assets/models/tree.obj");
    registerBlockType(BlockID::LAMP, "lamp", BlockRenderType::OBJ, "assets/models/lamp.obj");
    registerBlockType(BlockID::ROCK, "rock", BlockRenderType::OBJ, "assets/models/rock.obj");
    
    std::cout << "BlockTypeRegistry initialized with " << m_blockTypes.size() << " block types" << std::endl;
}
