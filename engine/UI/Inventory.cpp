// Inventory.cpp - Inventory implementation
#include "Inventory.h"
#include "../World/BlockType.h"
#include <algorithm>

Inventory::Inventory() {
    // Initialize hotbar with air (empty)
    m_hotbar.fill(BlockID::AIR);
    m_selectedSlot = 0;
    
    // Set up default blocks for testing
    initializeDefaultBlocks();
}

void Inventory::selectSlot(int slot) {
    // Clamp to valid range
    m_selectedSlot = std::clamp(slot, 0, HOTBAR_SIZE - 1);
}

uint8_t Inventory::getHotbarSlot(int slot) const {
    if (slot < 0 || slot >= HOTBAR_SIZE) {
        return BlockID::AIR;
    }
    return m_hotbar[slot];
}

void Inventory::setHotbarSlot(int slot, uint8_t blockID) {
    if (slot >= 0 && slot < HOTBAR_SIZE) {
        m_hotbar[slot] = blockID;
    }
}

uint8_t Inventory::getSelectedBlockID() const {
    return getHotbarSlot(m_selectedSlot);
}

void Inventory::selectNextBlock() {
    m_selectedSlot = (m_selectedSlot + 1) % HOTBAR_SIZE;
}

void Inventory::selectPreviousBlock() {
    m_selectedSlot = (m_selectedSlot - 1 + HOTBAR_SIZE) % HOTBAR_SIZE;
}

void Inventory::initializeDefaultBlocks() {
    // Set up some useful blocks in the hotbar for testing
    m_hotbar[0] = BlockID::STONE;
    m_hotbar[1] = BlockID::DIRT;
    m_hotbar[2] = BlockID::GRASS;
    m_hotbar[3] = BlockID::DECOR_GRASS;
    m_hotbar[4] = BlockID::TREE;
    m_hotbar[5] = BlockID::LAMP;
    m_hotbar[6] = BlockID::ROCK;
    m_hotbar[7] = BlockID::QUANTUM_FIELD_GENERATOR;
    m_hotbar[8] = BlockID::AIR; // Air = eraser
}
