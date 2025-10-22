// HUD.cpp - Heads-Up Display implementation
#include "HUD.h"
#include "Inventory.h"  // DEPRECATED: Old inventory system
#include "../World/BlockType.h"
#include "../World/ElementRecipes.h"  // NEW: Element-based crafting
#include <imgui.h>
#include <sstream>
#include <iomanip>

HUD::HUD() {
    // Initialize HUD state
}

HUD::~HUD() {
    // Cleanup if needed
}

void HUD::render(float deltaTime) {
    m_timeSinceLastUpdate += deltaTime;
    
    // Render HUD elements
    // renderCrosshair();  // REMOVED: Using block wireframe instead
    renderHealthBar();
    renderCurrentBlock();
    renderFPS();
    
    if (m_showDebugInfo) {
        renderDebugInfo();
    }
    
    if (!m_targetBlock.empty()) {
        renderTargetBlock();
    }
}

void HUD::renderCrosshair() {
    // Simple crosshair at screen center
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    
    ImGui::GetForegroundDrawList()->AddLine(
        ImVec2(center.x - 10, center.y),
        ImVec2(center.x + 10, center.y),
        IM_COL32(255, 255, 255, 200), 2.0f
    );
    ImGui::GetForegroundDrawList()->AddLine(
        ImVec2(center.x, center.y - 10),
        ImVec2(center.x, center.y + 10),
        IM_COL32(255, 255, 255, 200), 2.0f
    );
}

void HUD::renderHealthBar() {
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(200, 60), ImGuiCond_Always);
    ImGui::Begin("Health", nullptr, 
                 ImGuiWindowFlags_NoTitleBar | 
                 ImGuiWindowFlags_NoResize | 
                 ImGuiWindowFlags_NoMove | 
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoBackground);
    
    float healthPercent = m_health / m_maxHealth;
    ImGui::Text("Health");
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
    ImGui::ProgressBar(healthPercent, ImVec2(-1, 20));
    ImGui::PopStyleColor();
    
    ImGui::End();
}

void HUD::renderDebugInfo() {
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(10, 80), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 120), ImGuiCond_Always);
    ImGui::Begin("Debug Info", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoBackground);
    
    ImGui::Text("Position: %.1f, %.1f, %.1f", m_playerX, m_playerY, m_playerZ);
    ImGui::Text("FPS: %.1f", m_fps);
    ImGui::Text("Press F3 to toggle debug info");
    
    ImGui::End();
}

void HUD::renderCurrentBlock() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Bottom center
    ImVec2 pos(io.DisplaySize.x * 0.5f - 100, io.DisplaySize.y - 80);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(200, 60), ImGuiCond_Always);
    ImGui::Begin("Current Block", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoBackground);
    
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(m_currentBlock.c_str()).x) * 0.5f);
    ImGui::Text("%s", m_currentBlock.c_str());
    
    ImGui::End();
}

void HUD::renderTargetBlock() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Center, below crosshair
    ImVec2 pos(io.DisplaySize.x * 0.5f - 100, io.DisplaySize.y * 0.5f + 30);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(200, 40), ImGuiCond_Always);
    ImGui::Begin("Target Block", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoBackground);
    
    // Block name
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(m_targetBlock.c_str()).x) * 0.5f);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "%s", m_targetBlock.c_str());
    
    // Chemical formula (if available)
    if (!m_targetFormula.empty()) {
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(m_targetFormula.c_str()).x) * 0.5f);
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", m_targetFormula.c_str());
    }
    
    ImGui::End();
}

void HUD::renderFPS() {
    if (m_showDebugInfo) return; // Already shown in debug info
    
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 100, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(90, 30), ImGuiCond_Always);
    ImGui::Begin("FPS", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoBackground);
    
    ImGui::Text("FPS: %.0f", m_fps);
    
    ImGui::End();
}

void HUD::setPlayerPosition(float x, float y, float z) {
    m_playerX = x;
    m_playerY = y;
    m_playerZ = z;
}

void HUD::setPlayerHealth(float health, float maxHealth) {
    m_health = health;
    m_maxHealth = maxHealth;
}

void HUD::setCurrentBlock(const std::string& blockName) {
    m_currentBlock = blockName;
}

void HUD::setFPS(float fps) {
    m_fps = fps;
}

void HUD::setTargetBlock(const std::string& blockName, const std::string& formula) {
    m_targetBlock = blockName;
    m_targetFormula = formula;
}

void HUD::clearTargetBlock() {
    m_targetBlock = "";
    m_targetFormula = "";
}

void HUD::renderHotbar(const Inventory* inventory) {
    if (!inventory) {
        return;
    }
    
    ImGuiIO& io = ImGui::GetIO();
    
    // Hotbar dimensions
    const float slotSize = 50.0f;
    const float slotPadding = 4.0f;
    const float totalWidth = (slotSize + slotPadding) * Inventory::HOTBAR_SIZE - slotPadding;
    const float startX = (io.DisplaySize.x - totalWidth) * 0.5f;
    const float startY = io.DisplaySize.y - 80.0f;  // 80px from bottom
    
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    
    // Draw hotbar slots
    for (int i = 0; i < Inventory::HOTBAR_SIZE; ++i) {
        float x = startX + i * (slotSize + slotPadding);
        float y = startY;
        
        // Slot background (darker if selected)
        bool isSelected = (i == inventory->getSelectedSlot());
        ImU32 bgColor = isSelected 
            ? IM_COL32(80, 80, 80, 220)   // Selected: lighter gray
            : IM_COL32(40, 40, 40, 180);  // Normal: darker gray
        
        drawList->AddRectFilled(
            ImVec2(x, y),
            ImVec2(x + slotSize, y + slotSize),
            bgColor,
            4.0f  // Corner rounding
        );
        
        // Slot border (yellow if selected, white otherwise)
        ImU32 borderColor = isSelected 
            ? IM_COL32(255, 220, 0, 255)   // Selected: yellow
            : IM_COL32(150, 150, 150, 200);  // Normal: light gray
        
        drawList->AddRect(
            ImVec2(x, y),
            ImVec2(x + slotSize, y + slotSize),
            borderColor,
            4.0f,   // Corner rounding
            0,
            isSelected ? 3.0f : 2.0f  // Thicker border if selected
        );
        
        // Slot number (1-9)
        char numberStr[2];
        numberStr[0] = '1' + i;
        numberStr[1] = '\0';
        
        ImVec2 numberSize = ImGui::CalcTextSize(numberStr);
        drawList->AddText(
            ImVec2(x + 4.0f, y + 2.0f),
            IM_COL32(200, 200, 200, 255),
            numberStr
        );
        
        // Block name (get from registry)
        uint8_t blockID = inventory->getHotbarSlot(i);
        if (blockID != BlockID::AIR) {
            const char* blockName = BlockTypeRegistry::getInstance().getBlockName(blockID).c_str();
            
            // Draw block name centered in slot
            ImVec2 textSize = ImGui::CalcTextSize(blockName);
            float textX = x + (slotSize - textSize.x) * 0.5f;
            float textY = y + (slotSize - textSize.y) * 0.5f;
            
            // Text shadow for readability
            drawList->AddText(
                ImVec2(textX + 1, textY + 1),
                IM_COL32(0, 0, 0, 200),
                blockName
            );
            
            drawList->AddText(
                ImVec2(textX, textY),
                IM_COL32(255, 255, 255, 255),
                blockName
            );
        }
    }
}

void HUD::renderElementQueue(const ElementQueue& queue, const BlockRecipe* lockedRecipe,
                             const std::array<Element, 9>& hotbarElements) {
    ImGuiIO& io = ImGui::GetIO();
    
    // Hotbar dimensions (9 slots for elements 1-9)
    const float slotSize = 60.0f;
    const float slotPadding = 4.0f;
    const int numSlots = 9;  // Keys 1-9
    const float totalWidth = (slotSize + slotPadding) * numSlots - slotPadding;
    const float startX = (io.DisplaySize.x - totalWidth) * 0.5f;
    const float startY = io.DisplaySize.y - 100.0f;  // 100px from bottom
    
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    
    // Draw hotbar slots (using customizable hotbar)
    for (int i = 0; i < numSlots; ++i) {
        float x = startX + i * (slotSize + slotPadding);
        float y = startY;
        
        Element elem = hotbarElements[i];
        
        // Slot background
        drawList->AddRectFilled(
            ImVec2(x, y),
            ImVec2(x + slotSize, y + slotSize),
            IM_COL32(40, 40, 40, 200),
            4.0f  // Corner rounding
        );
        
        // Slot border
        drawList->AddRect(
            ImVec2(x, y),
            ImVec2(x + slotSize, y + slotSize),
            IM_COL32(150, 150, 150, 200),
            4.0f,
            0,
            2.0f
        );
        
        // Slot number (1-9) in top-left corner
        char numberStr[2];
        numberStr[0] = '1' + i;
        numberStr[1] = '\0';
        drawList->AddText(
            ImVec2(x + 4.0f, y + 2.0f),
            IM_COL32(180, 180, 180, 255),
            numberStr
        );
        
        // Element symbol (large, centered)
        std::string symbol = ElementRecipeSystem::getElementSymbol(elem);
        
        // Draw symbol larger using font size parameter
        ImFont* font = ImGui::GetFont();
        float fontSize = 28.0f;  // Larger font size for symbols
        ImVec2 symbolSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, symbol.c_str());
        
        drawList->AddText(
            font,
            fontSize,
            ImVec2(x + (slotSize - symbolSize.x) * 0.5f, y + (slotSize - symbolSize.y) * 0.5f - 2.0f),
            IM_COL32(220, 220, 220, 255),
            symbol.c_str()
        );
        
        // Element name (small, bottom)
        std::string name = ElementRecipeSystem::getElementName(elem);
        ImVec2 nameSize = ImGui::CalcTextSize(name.c_str());
        drawList->AddText(
            ImVec2(x + (slotSize - nameSize.x) * 0.5f, y + slotSize - 14.0f),
            IM_COL32(150, 150, 150, 255),
            name.c_str()
        );
    }
    
    // Show current queue above hotbar (if not empty)
    if (!queue.isEmpty() || lockedRecipe) {
        const float queueY = startY - 45.0f;
        
        if (lockedRecipe) {
            // Show locked recipe
            std::string recipeText = "Locked: " + lockedRecipe->name + " (" + lockedRecipe->formula + ")";
            ImVec2 textSize = ImGui::CalcTextSize(recipeText.c_str());
            
            drawList->AddRectFilled(
                ImVec2(startX + (totalWidth - textSize.x) * 0.5f - 10.0f, queueY - 5.0f),
                ImVec2(startX + (totalWidth + textSize.x) * 0.5f + 10.0f, queueY + textSize.y + 5.0f),
                IM_COL32(20, 60, 20, 220),
                4.0f
            );
            
            drawList->AddText(
                ImVec2(startX + (totalWidth - textSize.x) * 0.5f, queueY),
                IM_COL32(100, 255, 100, 255),  // Green
                recipeText.c_str()
            );
        } else {
            // Show current queue formula
            std::string formula = queue.toFormula();
            ImVec2 formulaSize = ImGui::CalcTextSize(formula.c_str());
            
            drawList->AddRectFilled(
                ImVec2(startX + (totalWidth - formulaSize.x) * 0.5f - 10.0f, queueY - 5.0f),
                ImVec2(startX + (totalWidth + formulaSize.x) * 0.5f + 10.0f, queueY + formulaSize.y + 5.0f),
                IM_COL32(40, 40, 20, 220),
                4.0f
            );
            
            drawList->AddText(
                ImVec2(startX + (totalWidth - formulaSize.x) * 0.5f, queueY),
                IM_COL32(255, 255, 100, 255),  // Yellow
                formula.c_str()
            );
        }
    }
}
