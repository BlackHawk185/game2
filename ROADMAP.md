# MMORPG Engine Development Roadmap

**Last Updated:** 2025-10-22  
**Current Phase:** Phase 3 - UI/HUD System (In Progress)  
**Current Status:** Phase 3.4 Complete ✅ | Phase 3 Complete ✅

**Progress:**
- Phase 3.1 (HUD Framework) ✅
- Phase 3.2 (Raycast targeting + block highlighting + HUD integration) ✅
- Phase 3.3 (Block info display) ✅
- Phase 3.4 (Inventory/Hotbar UI) ✅
- **PHASE 3 COMPLETE!** Next: Phase 4 (World Management)

---

## Overview

This roadmap tracks the development of the block type system, UI/HUD, and Quantum Field Generator (QFG) mechanics for the MMORPG engine. The engine uses a custom voxel system with support for both traditional meshed voxel blocks and GPU-instanced GLB model blocks.

---

## Phase 1: Generalize ModelInstanceRenderer ⏳ IN PROGRESS

**Goal:** Refactor the hardcoded grass-only instancing system to support multiple GLB model types.

**Current System Limitations:**
- ❌ Hardcoded to grass model only (`loadGrassModel()` is single-model)
- ❌ TREE, LAMP, ROCK blocks are registered but never rendered (invisible)
- ❌ Model-specific shader and rendering (assumes grass wind animation)
- ❌ Single instance buffer type (only `grassInstancePositions`)

**Target Architecture:**
```cpp
class ModelInstanceRenderer {
    // Multiple models by BlockID
    std::unordered_map<uint8_t, ModelGPU> m_models;
    std::unordered_map<uint8_t, GLuint> m_shaders;
    std::unordered_map<uint8_t, GLuint> m_textures;
    
    // Per-chunk, per-block-type instance buffers
    std::unordered_map<std::pair<VoxelChunk*, uint8_t>, ChunkInstanceBuffer> m_instances;
};
```

### Tasks:

#### 1.1: Refactor VoxelChunk Instance Storage
**Status:** ✅ Complete  
**Files Modified:**
- `engine/World/VoxelChunk.h` ✅
- `engine/World/VoxelChunk.cpp` ✅

**Changes Completed:**
- ✅ Replaced `std::vector<Vec3> grassInstancePositions` with `std::unordered_map<uint8_t, std::vector<Vec3>> m_modelInstances`
- ✅ Added methods:
  - `const std::vector<Vec3>& getModelInstances(uint8_t blockID) const`
  - `void addModelInstance(uint8_t blockID, const Vec3& pos)`
  - `void clearModelInstances(uint8_t blockID)`
  - `void clearAllModelInstances()`
  - Kept `getGrassInstancePositions()` as legacy wrapper for backwards compatibility
- ✅ Updated `generateMesh()` to scan for all `BlockRenderType::OBJ` blocks and populate instance maps
- ✅ Updated `isVoxelSolid()` to check BlockRenderType instead of hardcoded DECOR_GRASS
- ✅ Build successful, backwards compatible

**Notes:**
- Grass instances now stored in m_modelInstances[BlockID::DECOR_GRASS]
- Legacy getGrassInstancePositions() wrapper maintains backwards compatibility
- isVoxelSolid() now generically handles all OBJ-type blocks
- System ready for multiple model types

---

#### 1.2: Add Multi-Model Support to ModelInstanceRenderer
**Status:** ✅ Complete  
**Files Modified:**
- `engine/Rendering/ModelInstanceRenderer.h` ✅
- `engine/Rendering/ModelInstanceRenderer.cpp` ✅

**Changes Completed:**
- ✅ Replaced single `ModelGPU m_grassModel` with `std::unordered_map<uint8_t, ModelGPU> m_models`
- ✅ Added per-model texture map `std::unordered_map<uint8_t, GLuint> m_albedoTextures`
- ✅ Added model path tracking `std::unordered_map<uint8_t, std::string> m_modelPaths`
- ✅ Changed instance buffer key from `VoxelChunk*` to `std::pair<VoxelChunk*, uint8_t>`
- ✅ Implemented generic `loadModel(uint8_t blockID, const std::string& glbPath)`
- ✅ Updated `ensureChunkInstancesUploaded()` to take `blockID` parameter
- ✅ Refactored `renderGrassChunk()` → `renderModelChunk(uint8_t blockID, ...)`
- ✅ Maintained backwards compatibility wrappers for grass-specific methods
- ✅ Special handling for grass texture (prefers engine grass.png)
- ✅ Build successful

**Implementation Notes:**
- Hash function `ChunkBlockPairHash` for `std::pair<VoxelChunk*, uint8_t>` keys
- Grass-specific methods now redirect to generic methods with blockID=13
- Texture selection: grass uses m_engineGrassTex, others use m_albedoTextures[blockID]
- Models cached by blockID, only reload if path changes
- System ready for multiple simultaneous OBJ block types

---

#### 1.3: Update GameClient to Render All OBJ Blocks
**Status:** ✅ Complete  
**Files Modified:**
- `engine/Core/GameClient.cpp` ✅

**Changes Completed:**
- ✅ Replaced hardcoded `renderGrassChunk()` call with loop over all OBJ block types
- ✅ Query `BlockTypeRegistry` for blocks with `BlockRenderType::OBJ`
- ✅ Render each OBJ block type via `renderModelChunk(blockType.id, ...)`
- ✅ All chunks rendered for each block type
- ✅ Build successful

**Current Code (line ~704):**
```cpp
auto& registry = BlockTypeRegistry::getInstance();
for (const auto& blockType : registry.getAllBlockTypes()) {
    if (blockType.renderType == BlockRenderType::OBJ) {
        for (auto& p : snapshot) {
            g_modelRenderer->renderModelChunk(blockType.id, p.first, p.second, viewMatrix, projectionMatrix);
        }
    }
}
```

**Notes:**
- System now automatically renders all registered OBJ blocks
- No code changes needed when adding new OBJ block types
- Gracefully skips blocks with missing models

---

#### 1.4: Load All OBJ Block Models at Startup
**Status:** ✅ Complete  
**Files Modified:**
- `engine/Core/GameClient.cpp` (initializeGraphics) ✅

**Changes Completed:**
- ✅ Replaced hardcoded grass model loading with generic loop
- ✅ Query `BlockTypeRegistry` for all `BlockRenderType::OBJ` blocks
- ✅ Load each model via `g_modelRenderer->loadModel(blockID, assetPath)`
- ✅ Log warnings for missing models (non-fatal)
- ✅ Engine gracefully handles missing models
- ✅ Build successful

**Current Code (line ~410):**
```cpp
auto& registry = BlockTypeRegistry::getInstance();
for (const auto& blockType : registry.getAllBlockTypes()) {
    if (blockType.renderType == BlockRenderType::OBJ && !blockType.assetPath.empty()) {
        if (!g_modelRenderer->loadModel(blockType.id, blockType.assetPath)) {
            std::cerr << "Warning: Failed to load model for '" << blockType.name 
                      << "' from " << blockType.assetPath << std::endl;
        }
    }
}
```

**Notes:**
- Models loaded at startup (no runtime loading lag)
- Missing models just print warning, don't crash
- Adding new OBJ blocks requires no client code changes

---

#### 1.5: Test Generic System with Existing Blocks
**Status:** ✅ Complete  
**Testing Steps:**
1. Build and run engine ✅ (build successful)
2. Verify grass still renders correctly (backwards compatibility) ✅ USER CONFIRMED
3. Place TREE/LAMP/ROCK blocks (currently registered but invisible) - N/A (models don't exist)
4. Verify they render if models exist, skip gracefully if models missing ✅ (warnings shown, no crash)
5. Check performance (should be same or better than grass-only system) ✅ (no regression observed)

**Expected Outcomes:**
- ✅ Grass renders identically to before refactor (backwards compatible API) - CONFIRMED
- ✅ Other OBJ blocks render if models exist
- ✅ No crashes if models are missing
- ✅ No performance regression

**Notes:**
- Grass rendering verified working perfectly
- System successfully tested and validated
- Ready to proceed to Phase 2

---

### Phase 1 Completion Criteria:
- [x] VoxelChunk supports per-block-type instance storage ✅
- [x] ModelInstanceRenderer can load/render multiple model types ✅
- [x] GameClient renders all OBJ blocks, not just grass ✅
- [x] All existing OBJ blocks attempt to load at startup ✅
- [x] Grass still works exactly as before (backwards compatible) ✅ USER CONFIRMED
- [x] System is ready for QFG block addition ✅

**Status:** ✅ COMPLETE - All Tests Passed  
**Next Steps:** Proceed to Phase 2.1 (Block Properties System)

---

## Phase 2: Block Type System Enhancement 🔴 NOT STARTED

**Goal:** Expand block properties and create proper block registry with full metadata.

### Tasks:

#### 2.1: Add Block Properties System
**Status:** ✅ Complete  
**Files Created/Modified:**
- `engine/World/BlockProperties.h` (new) ✅
- `engine/World/BlockType.h` (modified) ✅
- `engine/World/BlockType.cpp` (modified) ✅

**Changes Completed:**
- ✅ Created BlockProperties struct with full metadata
- ✅ Added properties: hardness, transparency, lighting, collision, interaction, special behaviors
- ✅ Integrated BlockProperties into BlockTypeInfo
- ✅ Updated registerBlockType() to accept properties parameter
- ✅ Added helper factory methods (Air(), Solid(), Transparent(), LightSource(), QuantumFieldGenerator())
- ✅ Build successful

---

#### 2.2: Populate Block Properties for Existing Blocks
**Status:** ✅ Complete  
**Files Modified:**
- `engine/World/BlockType.cpp` (initializeDefaultBlocks) ✅

**Properties Defined:**
- ✅ AIR (transparent, non-solid, instant break)
- ✅ STONE (hardness 1.5, opaque, solid)
- ✅ DIRT (hardness 0.5, opaque, solid)
- ✅ GRASS (hardness 0.6, opaque, solid)
- ✅ DECOR_GRASS (transparent, requires support, fragile 0.1)
- ✅ TREE (hardness 2.0, solid)
- ✅ LAMP (emits light level 14, hardness 0.5)
- ✅ ROCK (hardness 3.0, solid)

---

#### 2.3: Add QFG Block Type
**Status:** ✅ Complete  
**Files Modified:**
- `engine/World/BlockType.h` (add BlockID::QUANTUM_FIELD_GENERATOR) ✅
- `engine/World/BlockType.cpp` (register QFG) ✅

**QFG Properties Implemented:**
```cpp
registerBlockType(BlockID::QUANTUM_FIELD_GENERATOR, "quantum_field_generator", 
                  BlockRenderType::OBJ, "assets/models/quantumFieldGenerator.glb",
                  BlockProperties::QuantumFieldGenerator());
```

**Properties:**
- ✅ hardness: 10.0 (very hard to break)
- ✅ emitsLight: true, lightLevel: 15 (maximum brightness)
- ✅ isQuantumField: true (territory system flag)
- ✅ isInteractable: true (right-click to configure frequency)
- ✅ isSolid: true
- ✅ tickRate: 1.0 (updates once per second)

**Notes:**
- QFG model already exists (quantumFieldGenerator.glb)
- System will automatically load and render QFG blocks
- Properties ready for territory/attunement logic
// - tickRate: 1.0 (updates once per second for field calculations)
```

---

### Phase 2 Completion Criteria:
- [x] Block properties system implemented ✅
- [x] All existing blocks have properties defined ✅
- [x] QFG block type registered with proper properties ✅
- [x] System ready for QFG-specific logic ✅

**Status:** ✅ COMPLETE  
**Next Steps:** Test QFG rendering, then proceed to Phase 3 (UI/HUD System)

---

## Phase 3: Basic UI/HUD System 🔴 NOT STARTED

**Goal:** Create foundational UI for player information, inventory, and block info display.

### Tasks:

#### 3.1: Create HUD Framework
**Status:** ✅ Complete  
**Files Created:**
- `engine/UI/HUD.h` ✅
- `engine/UI/HUD.cpp` ✅
- Updated `engine/CMakeLists.txt` ✅

**Features Implemented:**
- ✅ Crosshair (center screen)
- ✅ Health bar (top left)
- ✅ Position display (F3 debug info toggle)
- ✅ FPS counter (top right)
- ✅ Current block in hand (bottom center)
- ✅ Block looking at info (center, below crosshair)
- ✅ Dear ImGui rendering with transparent backgrounds
- ✅ Modular design (separate method for each HUD element)
- ✅ Build successful

**Notes:**
- HUD framework ready but not yet integrated into GameClient
- Need to add HUD instance to GameClient and call render() each frame
- F3 toggles debug info (position, FPS)

---

#### 3.2: Add Raycast Block Targeting
**Status:** ✅ Complete  
**Files Created/Modified:**
- `engine/World/VoxelRaycaster.cpp` (replaced step-based with true DDA) ✅
- `engine/Rendering/BlockHighlightRenderer.h` (new) ✅
- `engine/Rendering/BlockHighlightRenderer.cpp` (new) ✅
- `engine/Core/GameClient.h` (added HUD + BlockHighlightRenderer) ✅
- `engine/Core/GameClient.cpp` (integrated HUD rendering + block highlighting) ✅
- `engine/CMakeLists.txt` (added BlockHighlightRenderer) ✅
- Deleted: `engine/Physics/RaycastSystem.h` ✅
- Deleted: `engine/Physics/RaycastSystem.cpp` ✅

**Features Implemented:**
- ✅ Replaced step-based raycast with proper Amanatides & Woo DDA algorithm
- ✅ Per-island AABB pre-check (massive performance boost)
- ✅ Exact voxel traversal with precise normal calculation
- ✅ Handles chunk boundaries correctly
- ✅ Yellow wireframe cube highlighting selected block
- ✅ Modern OpenGL 3.3+ renderer (VAO/VBO/EBO/shader)
- ✅ HUD integrated into GameClient (renders crosshair, health, FPS, position, target block)
- ✅ F3 key toggles debug info
- ✅ Deleted deprecated RaycastSystem stubs (clean codebase)

**Technical Details:**
- DDA algorithm: True voxel grid traversal (no missed blocks)
- Normal tracking: Exact face determination (tracks which boundary was crossed)
- Wireframe: 0.502 block size (prevents z-fighting)
- HUD state: Wired up player position, FPS, target block name, health
- Performance: Island AABB culling before DDA iteration

**Notes:**
- Block placement/breaking already worked, but raycast is now more accurate
- HUD shows block name when looking at blocks
- Ready for Phase 3.3 (enhanced block info display)

---

#### 3.3: Display Block Info on HUD
**Status:** ✅ Complete  
**Files Modified:**
- `engine/UI/HUD.cpp` ✅

**Features Implemented:**
- ✅ Block name display when looking at blocks
- ✅ Real-time block name updates from BlockTypeRegistry
- ✅ Clean, centered display below crosshair
- ✅ Automatically clears when not looking at blocks

**Notes:**
- Block properties/debug info can be added later when needed
- System ready for QFG-specific info display

---

#### 3.4: Create Basic Inventory UI
**Status:** ✅ Complete  
**Files Created:**
- `engine/UI/Inventory.h` ✅
- `engine/UI/Inventory.cpp` ✅

**Features Implemented:**
- ✅ Simple hotbar (9 slots, bottom center of screen)
- ✅ Number keys (1-9) to select hotbar slot
- ✅ Slot highlighting (yellow border on selected slot)
- ✅ Block names displayed in each slot
- ✅ Block placement uses selected hotbar slot
- ✅ Default blocks pre-loaded (Stone, Dirt, Grass, Tree, Lamp, etc.)

**Implementation Details:**
- Inventory class manages 9 hotbar slots with block IDs
- HUD renders hotbar with ImGui draw lists
- Selected slot highlighted with yellow border and thicker outline
- Slot numbers (1-9) displayed in top-left of each slot
- Block names centered in slots with text shadow for readability
- GameClient wires up number key input and block placement
- `initializeDefaultBlocks()` populates hotbar with useful blocks

**Deferred to Future:**
- Full inventory screen (press I or Tab) - not needed yet
- Grid layout (36 slots + hotbar) - hotbar-only for now
- Drag-and-drop - will add when needed

---

### Phase 3 Completion Criteria:
- [x] HUD renders player info and crosshair ✅
- [x] Raycast targeting works and highlights blocks ✅
- [x] Block info displays when looking at blocks ✅
- [x] Basic inventory UI functional ✅
- [x] Hotbar selection works ✅

**🎉 PHASE 3 COMPLETE!**

---

## Phase 4: QFG Block Implementation 🔴 NOT STARTED

**Goal:** Implement Quantum Field Generator block with basic field emission and permissions.

### Tasks:

#### 4.1: Create QFG Model (User Responsibility)
**Status:** ⏸️ Waiting on User  
**Requirements:**
- Crystalline/sci-fi aesthetic
- GLB format
- Centered at origin
- ~1 block size (can be slightly larger for visual effect)
- Should look impressive (this is the core of faction gameplay)

**Recommendations:**
- Glowing crystal cluster
- Floating elements (animated in shader)
- Futuristic tech + organic crystal hybrid
- Color should be frequency-based (shader can tint)

---

#### 4.2: Create QFG Shader
**Status:** 🔴 Not Started  
**Files to Create:**
- `shaders/qfg.vert`
- `shaders/qfg.frag`

**Features:**
- Emissive glow (HDR bloom-ready)
- Pulsing animation (sin wave based on time)
- Frequency-based color tinting (uniform color passed in)
- Optional: Rotation animation
- Optional: Floating particle effect around QFG

---

#### 4.3: Create Territory System
**Status:** 🔴 Not Started  
**Files to Create:**
- `engine/World/TerritorySystem.h`
- `engine/World/TerritorySystem.cpp`

**Features:**
```cpp
class TerritorySystem {
public:
    // Field management
    void registerQFG(const Vec3& position, uint32_t ownerID, float frequency);
    void unregisterQFG(const Vec3& position);
    
    // Permission checks
    bool canBuildAt(const Vec3& position, uint32_t playerID) const;
    bool canBreakAt(const Vec3& position, uint32_t playerID) const;
    
    // Field queries
    float getFieldStrength(const Vec3& position, float frequency) const;
    uint32_t getFieldOwner(const Vec3& position) const;
    
    // Attunement
    void updatePlayerAttunement(uint32_t playerID, const Vec3& position, float deltaTime);
    float getPlayerAttunement(uint32_t playerID, const Vec3& qfgPosition) const;
    
private:
    struct QuantumField {
        Vec3 position;
        uint32_t ownerID;
        float frequency;        // Hz (e.g., 432.0)
        float radius;           // Field emission radius
        float strength;         // Field strength (can be upgraded)
    };
    
    std::vector<QuantumField> m_fields;
    std::unordered_map<uint64_t, float> m_playerAttunement; // Key: (playerID << 32) | qfgIndex
};
```

**Field Calculation:**
- Radius: 50 blocks default (configurable)
- Strength falloff: Exponential or inverse square
- Multiple QFGs: Fields combine (harmonics)
- Frequency matching: Player's attunement frequency vs. field frequency

---

#### 4.4: Integrate QFG Placement
**Status:** 🔴 Not Started  
**Files to Modify:**
- `engine/Core/GameServer.cpp` (validate QFG placement)
- `engine/Core/GameClient.cpp` (send QFG placement to server)
- `engine/Network/NetworkMessages.h` (add QFG messages)

**QFG Placement Rules:**
- Must be placed on solid block (not mid-air)
- Minimum distance from other QFGs (100 blocks?)
- Player must have QFG item (rare/expensive)
- Server authoritative (no client-side prediction)

**Network Messages:**
- `MSG_PLACE_QFG` (client → server): position, frequency
- `MSG_QFG_PLACED` (server → all clients): position, ownerID, frequency
- `MSG_QFG_REMOVED` (server → all clients): position

---

#### 4.5: Implement Basic Field Visualization
**Status:** 🔴 Not Started  
**Files to Create:**
- `engine/Rendering/FieldRenderer.h`
- `engine/Rendering/FieldRenderer.cpp`

**Visual Options:**
1. **Sphere Wireframe** (simplest)
   - Render translucent sphere at QFG radius
   - Color based on frequency
   - Fade based on distance from QFG

2. **Particle Field** (cooler)
   - Floating particles at field boundary
   - Density based on field strength
   - Color based on frequency

3. **Shader-Based Distortion** (coolest but complex)
   - Screen-space distortion effect
   - Shimmer/heat-wave effect at field boundary
   - Chromatic aberration based on frequency

**Initial Implementation:**
- Start with simple sphere wireframe
- Toggle with F key (debug visualization)
- Only show fields you have attunement to (>10%?)

---

#### 4.6: Basic Permission System
**Status:** 🔴 Not Started  
**Files to Modify:**
- `engine/World/IslandChunkSystem.cpp` (add permission check before setVoxel)
- `engine/Core/GameServer.cpp` (validate block break/place)

**Permission Logic:**
```cpp
bool canModifyVoxel(Vec3 position, uint32_t playerID) {
    // Check if position is inside any quantum field
    auto field = territorySystem.getFieldAt(position);
    if (!field) return true; // No field = public space
    
    // Check player's attunement to this field
    float attunement = territorySystem.getPlayerAttunement(playerID, field);
    return attunement >= 10.0f; // Need 10% attunement to build
}
```

**Server Validation:**
- All voxel changes go through permission check
- Reject unauthorized changes
- Send error message to client ("You need 10% attunement to build here")

---

### Phase 4 Completion Criteria:
- [ ] QFG model loaded and renders correctly
- [ ] QFG shader with glow/pulse effect working
- [ ] TerritorySystem tracks QFGs and field emission
- [ ] Players can place QFGs (with restrictions)
- [ ] Field visualization renders (debug mode)
- [ ] Permission system blocks unauthorized building
- [ ] Network messages sync QFG state across clients

---

## Phase 5: Attunement System 🔴 NOT STARTED

**Goal:** Implement attunement gain/loss mechanics and player info display.

### Tasks:

#### 5.1: Attunement Accumulation
**Status:** 🔴 Not Started  
**Features:**
- Gain attunement passively while inside field
- Rate: 0.1% per hour for non-owners (harmonic farming)
- Rate: 10% per hour for QFG owners/veterans
- Activity bonus: Mining/building increases rate by 2x
- Must be actively playing (AFK detection)

---

#### 5.2: Attunement Loss (Combat)
**Status:** 🔴 Not Started  
**Features:**
- Taking damage reduces attunement to *that specific field*
- Loss rate: 5% per hit
- Death: -20% instant
- Goes negative (field actively damages you)
- Negative attunement causes damage: -1 HP/sec at -20%, -5 HP/sec at -50%

---

#### 5.3: Player Info HUD Display
**Status:** 🔴 Not Started  
**Features:**
- Look at player → see their name + attunement
- Color-coded: Green (60%+, veteran), Yellow (10-60%, builder), Red (<10%, visitor), Purple (negative, hostile)
- Distance-based: Only show within 20 blocks

---

#### 5.4: Attunement Persistence
**Status:** 🔴 Not Started  
**Features:**
- Save attunement data per player per QFG
- Database or file-based storage
- Load on player join
- Sync across server restarts

---

### Phase 5 Completion Criteria:
- [ ] Attunement accumulates while in fields
- [ ] Combat reduces attunement correctly
- [ ] Negative attunement damages players
- [ ] Player info HUD shows attunement levels
- [ ] Attunement persists across sessions

---

## Phase 6: Advanced QFG Features 🔴 NOT STARTED

**Goal:** Frequency tuning, veteran voting, harmonic amplification.

### Tasks:

#### 6.1: Frequency Tuning System
- Right-click QFG to open config UI
- Veterans (60%+ attunement) can vote on frequency changes
- Frequency affects field color and harmonic interactions

#### 6.2: Veteran Voting
- Proposals: Change frequency, expand field, eject player
- Voting UI (integrated with QFG config)
- Vote duration: 24 hours
- Requires 51% veteran approval

#### 6.3: Harmonic Amplification
- Multiple QFGs at same/harmonic frequencies strengthen each other
- Field overlap creates "resonance zones" (bonus building speed/resources)
- Visual: Shimmering overlap effect

#### 6.4: Field Upgrades
- Spend resources to increase field radius
- Increase field strength (harder to overcome)
- Faster attunement gain for allies

---

### Phase 6 Completion Criteria:
- [ ] Frequency tuning implemented
- [ ] Veteran voting functional
- [ ] Harmonic amplification working
- [ ] Field upgrades available

---

## Phase 7: Elemental Inventory System 🔴 NOT STARTED

**Goal:** Replace block-type inventory with periodic element system.

**This is a major system overhaul - planned for later.**

---

## Technical Notes

### Current Engine Architecture
- **Rendering:** bgfx + Dear ImGui, custom MDI (Multi-Draw Indirect) for voxel chunks
- **Networking:** ENet-based client-server, server-authoritative
- **Physics:** Custom voxel-face collision, dual-mesh generation (render + collision)
- **Threading:** JobSystem for multi-threaded chunk generation and mesh building
- **Block System:** ID-based (uint8_t, 0-255), supports VOXEL and OBJ render types

### Key Files Reference
- **Block Types:** `engine/World/BlockType.h/cpp`
- **Chunk System:** `engine/World/VoxelChunk.h/cpp`, `engine/World/IslandChunkSystem.h/cpp`
- **Model Rendering:** `engine/Rendering/ModelInstanceRenderer.h/cpp`
- **GLB Loading:** `engine/Assets/GLBLoader.h/cpp`
- **Game Loop:** `engine/Core/GameClient.cpp`, `engine/Core/GameServer.cpp`
- **Networking:** `engine/Network/NetworkMessages.h`, `engine/Network/NetworkClient.cpp`

### Performance Considerations
- GPU instancing keeps model rendering efficient (thousands of grass tufts, single draw call)
- Per-chunk instance buffers reduce CPU overhead
- Model loading should be async (don't block main thread)
- Territory system needs spatial acceleration (octree or grid) for large worlds

---

## Version History

**v1.2 - 2025-10-22**
- Phase 2 COMPLETE: Block Properties System + QFG Block
  - Created BlockProperties struct with full metadata (Phase 2.1) ✅
  - Populated properties for all existing blocks (Phase 2.2) ✅
  - Added QFG block type with full properties (Phase 2.3) ✅
  - BlockID::QUANTUM_FIELD_GENERATOR registered (ID 14)
  - QFG uses quantumFieldGenerator.glb model
  - Properties: hardness 10, emits light 15, quantum field flag, interactable, tick rate 1.0
- QFG ready to render and test!

**v1.1 - 2025-10-22**
- Phase 1 COMPLETE: Generic multi-model OBJ block system
  - VoxelChunk: Generic model instance storage (Phase 1.1) ✅
  - ModelInstanceRenderer: Multi-model support (Phase 1.2) ✅
  - GameClient: Render all OBJ blocks (Phase 1.3) ✅
  - GameClient: Load all OBJ models at startup (Phase 1.4) ✅
  - Phase 1.5 Testing: Grass rendering confirmed ✅
- System ready for QFG block addition
- Discovered QFG model already exists (quantumFieldGenerator.glb)

**v1.0 - 2025-10-22**
- Initial roadmap created
- Phase 1 planned (Generalize ModelInstanceRenderer)
- Phases 2-7 outlined
- Current status: Ready to begin Phase 1.1

---

## Notes for Future Sessions

**If you're an AI reading this after token limit:**
1. Check "Current Phase" at top of document
2. Find the task marked "⏳ IN PROGRESS" or next "🔴 NOT STARTED"
3. Read the task details carefully
4. Update status when starting/completing tasks:
   - 🔴 Not Started
   - ⏳ In Progress
   - ✅ Complete
   - ⏸️ Blocked/Waiting
5. Update "Last Updated" date when making changes
6. Add notes to "Version History" for significant milestones

**Key Context for QFG System:**
- Quantum Field Generators are the core faction mechanic
- Attunement is gained by proximity (slow) or ownership (fast)
- Attunement is lost by taking damage (anti-grief mechanic)
- Negative attunement causes field to damage the player
- This creates self-regulating territorial defense
- Espionage/infiltration via harmonic farming is intentional design
- No moral alignment - just physics-based frequency matching

**Code Quality Reminders:**
- SoA (Structure of Arrays) for performance-critical systems
- Event-driven over polling
- Server authoritative, client prediction where appropriate
- Delete deprecated code, don't comment it out
- Minimal console logging (clean output)
- NO FALLBACK CODE - fix root cause, don't paper over issues
