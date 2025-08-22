#pragma once
#include "Math/Vec3.h"

// Forward declarations
class Camera;

// Player class for physics-driven player movement on floating islands
class Player {
public:
    Player();
    
    // Core update methods
    void update(float deltaTime);
    void applyInput(Vec3 inputDirection, bool jump, float deltaTime);
    void updateCameraFromPlayer(Camera* camera);
    
    // Position and movement getters/setters
    Vec3 getPosition() const { return position; }
    void setPosition(const Vec3& newPos) { position = newPos; }
    
    // Base velocity for platform/island movement (NEW)
    const Vec3& getBaseVelocity() const { return baseVelocity; }
    void setBaseVelocity(const Vec3& vel) { baseVelocity = vel; }

private:
    // Core physics update methods
    void updatePhysics(float deltaTime);
    void checkGroundCollision();
    void applyFriction(float deltaTime);
    
    // Position and movement data
    Vec3 position{0.0f, 0.0f, 0.0f}; // Will be set dynamically above island
    Vec3 velocity{0.0f, 0.0f, 0.0f};
    Vec3 acceleration{0.0f, -15.0f, 0.0f}; // Stronger gravity for dramatic falls
    Vec3 baseVelocity{0.0f, 0.0f, 0.0f}; // Platform/island movement velocity
    
    // Ground state
    bool onGround = false;
    bool wasOnGround = false;
    
    // Movement parameters
    float moveSpeed = 16.0f;
    float jumpStrength = 12.0f;
    float airControl = 0.4f; // Reduced air control for realism
    
    // Friction coefficients
    float groundFriction = 0.85f; // Ground friction (0.85 = 15% velocity loss per frame)
    float airFriction = 0.98f;    // Air resistance (2% velocity loss per frame)
    
    // Collision data
    Vec3 collisionSize{0.7f, 1.8f, 0.7f}; // Slightly smaller for tight spaces
};
