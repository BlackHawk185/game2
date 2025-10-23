// ElementRecipes.h - Element combination system for block crafting
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// Element definitions (periodic table)
enum class Element : uint8_t {
    None = 0,  // Empty/unbound slot
    H = 1,   // Hydrogen
    He = 2,  // Helium
    Li = 3,  // Lithium
    C = 4,   // Carbon
    N = 5,   // Nitrogen
    O = 6,   // Oxygen
    F = 7,   // Fluorine
    Ne = 8,  // Neon
    Na = 9,  // Sodium
    Mg = 10, // Magnesium
    Al = 11, // Aluminum
    Si = 12, // Silicon
    P = 13,  // Phosphorus
    S = 14,  // Sulfur
    Cl = 15, // Chlorine
    K = 16,  // Potassium
    Ca = 17, // Calcium
    Fe = 18, // Iron
    Cu = 19, // Copper
    Au = 20  // Gold
};

// Element queue for player input
struct ElementQueue {
    std::vector<Element> elements;
    
    void addElement(Element elem) { elements.push_back(elem); }
    void clear() { elements.clear(); }
    bool isEmpty() const { return elements.empty(); }
    size_t size() const { return elements.size(); }
    
    // Convert to string for display (e.g., "122" for H₂O)
    std::string toString() const;
    
    // Convert to formula string (e.g., "H₂O")
    std::string toFormula() const;
};

// Block recipe definition
struct BlockRecipe {
    std::unordered_map<Element, int> elements;  // Required element counts (order doesn't matter)
    uint8_t blockID;                             // Resulting block type
    std::string name;                            // Display name
    std::string formula;                         // Chemical formula (e.g., "H2O")
    
    // Helper to create recipe from element list
    static BlockRecipe create(std::initializer_list<Element> elemList, uint8_t block, 
                             const std::string& name, const std::string& formula);
};

// Recipe system
class ElementRecipeSystem {
public:
    static ElementRecipeSystem& getInstance() {
        static ElementRecipeSystem instance;
        return instance;
    }
    
    // Match element queue to a recipe
    const BlockRecipe* matchRecipe(const ElementQueue& queue) const;
    
    // Get element name
    static const char* getElementName(Element elem);
    static const char* getElementSymbol(Element elem);
    
    // Get element color for UI rendering (returns ImU32 color)
    // Shared by periodic table and hotbar for consistency
    static uint32_t getElementColor(Element elem);
    
    // Get all recipes (for UI display)
    const std::vector<BlockRecipe>& getAllRecipes() const { return m_recipes; }
    
private:
    ElementRecipeSystem();
    void initializeRecipes();
    std::string createRecipeKey(const std::unordered_map<Element, int>& elements) const;
    
    std::vector<BlockRecipe> m_recipes;
    std::unordered_map<std::string, const BlockRecipe*> m_recipeMap;  // Fast lookup by element counts
};
