// ElementRecipes.cpp - Element combination recipes
#include "ElementRecipes.h"
#include "BlockType.h"
#include <sstream>
#include <algorithm>

std::string ElementQueue::toString() const {
    std::string result;
    for (auto elem : elements) {
        result += std::to_string(static_cast<int>(elem));
    }
    return result;
}

std::string ElementQueue::toFormula() const {
    if (elements.empty()) return "";
    
    // Count element occurrences
    std::unordered_map<Element, int> counts;
    for (auto elem : elements) {
        counts[elem]++;
    }
    
    // Build formula string (e.g., H2O)
    // Sort by element for consistency
    std::vector<std::pair<Element, int>> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(), 
              [](const auto& a, const auto& b) { return a.first < b.first; });
    
    std::stringstream ss;
    for (const auto& [elem, count] : sorted) {
        ss << ElementRecipeSystem::getElementSymbol(elem);
        if (count > 1) {
            ss << count;  // Subscript numbers would be nice but ASCII for now
        }
    }
    return ss.str();
}

BlockRecipe BlockRecipe::create(std::initializer_list<Element> elemList, uint8_t block, 
                                const std::string& name, const std::string& formula) {
    BlockRecipe recipe;
    recipe.blockID = block;
    recipe.name = name;
    recipe.formula = formula;
    
    // Count elements (order doesn't matter)
    for (auto elem : elemList) {
        recipe.elements[elem]++;
    }
    
    return recipe;
}

ElementRecipeSystem::ElementRecipeSystem() {
    initializeRecipes();
}

void ElementRecipeSystem::initializeRecipes() {
    m_recipes.clear();
    m_recipeMap.clear();
    
    // Simple recipes (single elements or pure compounds)
    // Order doesn't matter - we count element occurrences
    
    // Pure elements
    m_recipes.push_back(BlockRecipe::create({Element::C}, BlockID::COAL, "Coal", "C"));
    m_recipes.push_back(BlockRecipe::create({Element::Fe}, BlockID::IRON_BLOCK, "Iron Block", "Fe"));
    m_recipes.push_back(BlockRecipe::create({Element::Au}, BlockID::GOLD_BLOCK, "Gold Block", "Au"));
    m_recipes.push_back(BlockRecipe::create({Element::Cu}, BlockID::COPPER_BLOCK, "Copper Block", "Cu"));
    
    // Water - H₂O (2 hydrogen, 1 oxygen)
    m_recipes.push_back(BlockRecipe::create({Element::H, Element::H, Element::O}, BlockID::WATER, "Water", "H2O"));
    
    // Stone - SiO₂ (1 silicon, 2 oxygen)
    m_recipes.push_back(BlockRecipe::create({Element::Si, Element::O, Element::O}, BlockID::STONE, "Stone", "SiO2"));
    
    // Sand - SiO (simplified, 1 silicon, 1 oxygen)
    m_recipes.push_back(BlockRecipe::create({Element::Si, Element::O}, BlockID::SAND, "Sand", "SiO"));
    
    // Salt - NaCl (1 sodium, 1 chlorine)
    m_recipes.push_back(BlockRecipe::create({Element::Na, Element::Cl}, BlockID::SALT_BLOCK, "Salt", "NaCl"));
    
    // Limestone - CaCO₃ (1 calcium, 1 carbon, 3 oxygen)
    m_recipes.push_back(BlockRecipe::create({Element::Ca, Element::C, Element::O, Element::O, Element::O}, 
                                           BlockID::LIMESTONE, "Limestone", "CaCO3"));
    
    // Ice - H₂O (frozen) - same counts as water but different block
    m_recipes.push_back(BlockRecipe::create({Element::H, Element::H, Element::O}, BlockID::ICE, "Ice", "H2O(s)"));
    
    // Diamond - C₄ (4 carbon atoms in crystalline structure)
    m_recipes.push_back(BlockRecipe::create({Element::C, Element::C, Element::C, Element::C}, 
                                           BlockID::DIAMOND_BLOCK, "Diamond", "C4"));
    
    // Build lookup map (sorted element count signature -> recipe)
    for (const auto& recipe : m_recipes) {
        std::string key = createRecipeKey(recipe.elements);
        m_recipeMap[key] = &recipe;
    }
}

std::string ElementRecipeSystem::createRecipeKey(const std::unordered_map<Element, int>& elements) const {
    // Create sorted key: "element_id:count,element_id:count,..."
    std::vector<std::pair<Element, int>> sorted(elements.begin(), elements.end());
    std::sort(sorted.begin(), sorted.end());
    
    std::stringstream ss;
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (i > 0) ss << ",";
        ss << static_cast<int>(sorted[i].first) << ":" << sorted[i].second;
    }
    return ss.str();
}

const BlockRecipe* ElementRecipeSystem::matchRecipe(const ElementQueue& queue) const {
    if (queue.isEmpty()) return nullptr;
    
    // Count elements in queue
    std::unordered_map<Element, int> counts;
    for (auto elem : queue.elements) {
        counts[elem]++;
    }
    
    // Create key and lookup
    std::string key = createRecipeKey(counts);
    auto it = m_recipeMap.find(key);
    if (it != m_recipeMap.end()) {
        return it->second;
    }
    
    return nullptr;
}

const char* ElementRecipeSystem::getElementName(Element elem) {
    switch (elem) {
        case Element::H:  return "Hydrogen";
        case Element::He: return "Helium";
        case Element::Li: return "Lithium";
        case Element::C:  return "Carbon";
        case Element::N:  return "Nitrogen";
        case Element::O:  return "Oxygen";
        case Element::F:  return "Fluorine";
        case Element::Ne: return "Neon";
        case Element::Na: return "Sodium";
        case Element::Mg: return "Magnesium";
        case Element::Al: return "Aluminum";
        case Element::Si: return "Silicon";
        case Element::P:  return "Phosphorus";
        case Element::S:  return "Sulfur";
        case Element::Cl: return "Chlorine";
        case Element::K:  return "Potassium";
        case Element::Ca: return "Calcium";
        case Element::Fe: return "Iron";
        case Element::Cu: return "Copper";
        case Element::Au: return "Gold";
        default: return "Unknown";
    }
}

const char* ElementRecipeSystem::getElementSymbol(Element elem) {
    switch (elem) {
        case Element::H:  return "H";
        case Element::He: return "He";
        case Element::Li: return "Li";
        case Element::C:  return "C";
        case Element::N:  return "N";
        case Element::O:  return "O";
        case Element::F:  return "F";
        case Element::Ne: return "Ne";
        case Element::Na: return "Na";
        case Element::Mg: return "Mg";
        case Element::Al: return "Al";
        case Element::Si: return "Si";
        case Element::P:  return "P";
        case Element::S:  return "S";
        case Element::Cl: return "Cl";
        case Element::K:  return "K";
        case Element::Ca: return "Ca";
        case Element::Fe: return "Fe";
        case Element::Cu: return "Cu";
        case Element::Au: return "Au";
        default: return "?";
    }
}
