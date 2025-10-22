// PeriodicTableUI.h - Periodic table inventory UI for element hotbar binding
#pragma once

#include "../World/ElementRecipes.h"
#include <array>

class PeriodicTableUI {
public:
    PeriodicTableUI();
    ~PeriodicTableUI() = default;
    
    // Render the periodic table UI (call when Tab is pressed)
    // Returns true if UI is open and capturing input
    bool render(std::array<Element, 9>& hotbarElements);
    
    // Toggle UI visibility
    void toggle() { m_isOpen = !m_isOpen; }
    void setOpen(bool open) { m_isOpen = open; }
    bool isOpen() const { return m_isOpen; }
    
private:
    struct ElementBox {
        Element element;
        int row;      // 1-7 (periods)
        int col;      // 1-18 (groups)
        const char* symbol;
        const char* name;
        int atomicNumber;
    };
    
    void renderElementBox(const ElementBox& box, float x, float y, float size, 
                          Element hoveredElement, int selectedHotbarSlot);
    
    bool m_isOpen = false;
    Element m_hoveredElement = Element::None;
    
    // Periodic table layout (row, col positions for first 20 elements)
    static const ElementBox s_elements[];
    static constexpr int s_elementCount = 20;
};
