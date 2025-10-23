// PeriodicTableUI.cpp - Implementation of periodic table UI
#include "PeriodicTableUI.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <iostream>

// Periodic table layout (first 20 elements with proper positioning)
const PeriodicTableUI::ElementBox PeriodicTableUI::s_elements[] = {
    {Element::H,  1,  1, "H",  "Hydrogen",   1},
    {Element::He, 1, 18, "He", "Helium",     2},
    {Element::Li, 2,  1, "Li", "Lithium",    3},
    {Element::C,  2, 14, "C",  "Carbon",     6},
    {Element::N,  2, 15, "N",  "Nitrogen",   7},
    {Element::O,  2, 16, "O",  "Oxygen",     8},
    {Element::F,  2, 17, "F",  "Fluorine",   9},
    {Element::Ne, 2, 18, "Ne", "Neon",      10},
    {Element::Na, 3,  1, "Na", "Sodium",    11},
    {Element::Mg, 3,  2, "Mg", "Magnesium", 12},
    {Element::Al, 3, 13, "Al", "Aluminum",  13},
    {Element::Si, 3, 14, "Si", "Silicon",   14},
    {Element::P,  3, 15, "P",  "Phosphorus",15},
    {Element::S,  3, 16, "S",  "Sulfur",    16},
    {Element::Cl, 3, 17, "Cl", "Chlorine",  17},
    {Element::K,  4,  1, "K",  "Potassium", 19},
    {Element::Ca, 4,  2, "Ca", "Calcium",   20},
    {Element::Fe, 4,  8, "Fe", "Iron",      26},
    {Element::Cu, 4, 11, "Cu", "Copper",    29},
    {Element::Au, 6, 11, "Au", "Gold",      79},
};

PeriodicTableUI::PeriodicTableUI() 
    : m_isOpen(false)
    , m_hoveredElement(Element::None)
{
}

void PeriodicTableUI::renderElementBox(const ElementBox& box, float x, float y, float size,
                                       Element hoveredElement, int selectedHotbarSlot)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    bool isHovered = (box.element == hoveredElement);
    
    // Use shared color function for consistency with hotbar
    ImU32 boxColor = ElementRecipeSystem::getElementColor(box.element);
    
    // Highlight if hovered
    if (isHovered) {
        boxColor = IM_COL32(255, 220, 100, 255);  // Bright yellow
    }
    
    // Draw box
    drawList->AddRectFilled(
        ImVec2(x, y),
        ImVec2(x + size, y + size),
        boxColor,
        4.0f
    );
    
    // Border
    drawList->AddRect(
        ImVec2(x, y),
        ImVec2(x + size, y + size),
        IM_COL32(80, 80, 80, 255),
        4.0f,
        0,
        2.0f
    );
    
    // Atomic number (top-left, small)
    char numStr[8];
    snprintf(numStr, sizeof(numStr), "%d", box.atomicNumber);
    drawList->AddText(
        ImVec2(x + 4.0f, y + 2.0f),
        IM_COL32(60, 60, 60, 255),
        numStr
    );
    
    // Element symbol (center, large) - scale font with box size
    ImFont* font = ImGui::GetFont();
    float fontSize = size * 0.4f;  // Symbol is 40% of box size
    ImVec2 symbolSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, box.symbol);
    drawList->AddText(
        font,
        fontSize,
        ImVec2(x + (size - symbolSize.x) * 0.5f, y + (size - symbolSize.y) * 0.5f - size * 0.08f),
        IM_COL32(20, 20, 20, 255),
        box.symbol
    );
    
    // Element name (bottom, small) - scale with box size
    ImVec2 nameSize = ImGui::CalcTextSize(box.name);
    drawList->AddText(
        ImVec2(x + (size - nameSize.x) * 0.5f, y + size - size * 0.23f),
        IM_COL32(60, 60, 60, 255),
        box.name
    );
}

bool PeriodicTableUI::render(std::array<Element, 9>& hotbarElements)
{
    if (!m_isOpen) {
        return false;
    }
    
    // Scale with window size (80% width, 70% height, but capped at reasonable sizes)
    ImGuiIO& io = ImGui::GetIO();
    float windowWidth = std::min(io.DisplaySize.x * 0.8f, 1600.0f);
    float windowHeight = std::min(io.DisplaySize.y * 0.7f, 900.0f);
    
    // Center the window
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
    
    ImGui::Begin("Periodic Table - Hover over element and press 1-9 to bind to hotbar", nullptr, 
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    
    // Instructions
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Hover over an element and press 1-9 to assign it to your hotbar");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Press TAB to close");
    ImGui::Separator();
    
    // Get current hotbar slot selection (check keys 1-9)
    int selectedHotbarSlot = -1;
    for (int i = 0; i < 9; ++i) {
        if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_1 + i), false)) {
            selectedHotbarSlot = i;
        }
    }
    
    // Drawing area - scale box size based on window size
    ImVec2 windowPos = ImGui::GetCursorScreenPos();
    float boxSize = std::min(windowWidth / 20.0f, windowHeight / 9.0f);  // Scale to fit 18 columns, ~7 rows
    float spacing = boxSize * 0.1f;  // Spacing is 10% of box size
    float cellSize = boxSize + spacing;
    
    // Reset hovered element
    m_hoveredElement = Element::None;
    
    // Draw periodic table
    for (int i = 0; i < s_elementCount; ++i) {
        const ElementBox& elem = s_elements[i];
        
        float x = windowPos.x + (elem.col - 1) * cellSize;
        float y = windowPos.y + (elem.row - 1) * cellSize;
        
        // Check if mouse is hovering over this element
        ImVec2 mousePos = ImGui::GetMousePos();
        bool isHovered = (mousePos.x >= x && mousePos.x <= x + boxSize &&
                         mousePos.y >= y && mousePos.y <= y + boxSize);
        
        if (isHovered) {
            m_hoveredElement = elem.element;
        }
        
        renderElementBox(elem, x, y, boxSize, m_hoveredElement, selectedHotbarSlot);
    }
    
    // If user pressed a hotbar key while hovering, bind the element
    if (selectedHotbarSlot != -1 && m_hoveredElement != Element::None) {
        hotbarElements[selectedHotbarSlot] = m_hoveredElement;
        std::cout << "Bound " << ElementRecipeSystem::getElementName(m_hoveredElement) 
                  << " (" << ElementRecipeSystem::getElementSymbol(m_hoveredElement) 
                  << ") to hotbar slot " << (selectedHotbarSlot + 1) << std::endl;
    }
    
    // Show current hotbar at bottom
    ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 100.0f);
    ImGui::Separator();
    ImGui::Text("Current Hotbar:");
    ImGui::BeginChild("Hotbar", ImVec2(0, 60), true);
    for (int i = 0; i < 9; ++i) {
        if (i > 0) ImGui::SameLine();
        
        Element elem = hotbarElements[i];
        if (elem != Element::None) {
            ImGui::BeginGroup();
            ImGui::Text("%d", i + 1);
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "%s", 
                             ElementRecipeSystem::getElementSymbol(elem));
            ImGui::EndGroup();
        } else {
            ImGui::BeginGroup();
            ImGui::Text("%d", i + 1);
            ImGui::TextDisabled("---");
            ImGui::EndGroup();
        }
    }
    ImGui::EndChild();
    
    ImGui::End();
    
    return true;  // UI is capturing input
}
