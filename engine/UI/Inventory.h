// Inventory.h - Player inventory and hotbar management
#pragma once

#include <cstdint>
#include <array>

// Simple inventory system for block placement
class Inventory {
public:
    static constexpr int HOTBAR_SIZE = 9;
    
    Inventory();
    
    // Hotbar management
    void setHotbarSlot(int slot, uint8_t blockID);
    uint8_t getHotbarSlot(int slot) const;
    void selectSlot(int slot);
    int getSelectedSlot() const { return m_selectedSlot; }
    uint8_t getSelectedBlockID() const;
    
    // Block selection shortcuts
    void selectNextBlock();
    void selectPreviousBlock();
    
    // Initialize with common blocks for testing
    void initializeDefaultBlocks();
    
private:
    std::array<uint8_t, HOTBAR_SIZE> m_hotbar;
    int m_selectedSlot = 0;
};
