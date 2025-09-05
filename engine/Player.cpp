// Player.cpp - Physics-driven player for floating island exploration
#include "Player.h"

#include "Input/Camera.h"
#include "Time/TimeManager.h"
#include "World/IslandChunkSystem.h"
#include "pch.h"

extern IslandChunkSystem g_islandSystem;  // Global system reference

Player::Player()
{
    // No spawn logic here — spawning is handled by the game startup (e.g., main.cpp)
    // Leave `position` at its default (declared in Player.h) so the world/level
    // code can place the player after islands/world are created.
}

void Player::update(float deltaTime)
{
    // Player physics is handled by camera system, not this class
    // This method is kept for future use when implementing walking/climbing
}




