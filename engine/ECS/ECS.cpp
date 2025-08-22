// ECS.cpp - Implementation of Entity Component System
#include "ECS.h"

ECSWorld g_ecs;

EntityID ECSWorld::createEntity() {
    return m_nextEntityID++;
}

void ECSWorld::destroyEntity(EntityID entity) {
    // Remove from all component storages
    for (auto& [typeIndex, storage] : m_componentStorages) {
        storage->removeEntity(entity);
    }
}
