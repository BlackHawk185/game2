#include "FrustumCuller.h"

#include <cmath>
#include <iostream>

#include "../Input/Camera.h"

// Global frustum culler
FrustumCuller g_frustumCuller;

FrustumCuller::FrustumCuller()
{
    // Removed verbose debug output
}

void Frustum::updateFromCamera(const Camera& camera, float aspect, float fovDegrees)
{
    // Convert FOV to radians
    float fovRad = fovDegrees * 3.14159f / 180.0f;
    float halfFov = fovRad * 0.5f;

    // Calculate frustum dimensions at near and far planes
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    float nearHeight = 2.0f * tanf(halfFov) * nearPlane;
    float nearWidth = nearHeight * aspect;
    float farHeight = 2.0f * tanf(halfFov) * farPlane;
    float farWidth = farHeight * aspect;

    // Get camera vectors (already normalized)
    Vec3 forward = camera.front;
    Vec3 right = camera.right;
    Vec3 up = camera.up;

    // Calculate frustum plane normals and distances
    Vec3 pos = camera.position;

    // Near and far planes
    planes[4] = forward;  // Near plane normal (pointing into frustum)
    distances[4] = forward.dot(pos + forward * nearPlane);

    planes[5] = forward * -1.0f;  // Far plane normal (pointing into frustum)
    distances[5] = (forward * -1.0f).dot(pos + forward * farPlane);

    // Side planes - calculate normals pointing inward
    Vec3 nearCenter = pos + forward * nearPlane;
    Vec3 farCenter = pos + forward * farPlane;

    // Left plane
    Vec3 leftNormal = (up.cross(nearCenter + right * (-nearWidth * 0.5f) - pos)).normalized();
    planes[0] = leftNormal;
    distances[0] = leftNormal.dot(pos);

    // Right plane
    Vec3 rightNormal = ((nearCenter + right * (nearWidth * 0.5f) - pos).cross(up)).normalized();
    planes[1] = rightNormal;
    distances[1] = rightNormal.dot(pos);

    // Top plane
    Vec3 topNormal = (right.cross(nearCenter + up * (nearHeight * 0.5f) - pos)).normalized();
    planes[2] = topNormal;
    distances[2] = topNormal.dot(pos);

    // Bottom plane
    Vec3 bottomNormal = ((nearCenter + up * (-nearHeight * 0.5f) - pos).cross(right)).normalized();
    planes[3] = bottomNormal;
    distances[3] = bottomNormal.dot(pos);
}

bool Frustum::intersectsAABB(const Vec3& center, const Vec3& halfSize) const
{
    // Test AABB against all 6 frustum planes
    for (int i = 0; i < 6; i++)
    {
        Vec3 normal = planes[i];
        float d = distances[i];

        // Calculate the positive vertex (farthest point along plane normal)
        Vec3 positive;
        positive.x = (normal.x >= 0) ? (center.x + halfSize.x) : (center.x - halfSize.x);
        positive.y = (normal.y >= 0) ? (center.y + halfSize.y) : (center.y - halfSize.y);
        positive.z = (normal.z >= 0) ? (center.z + halfSize.z) : (center.z - halfSize.z);

        // If positive vertex is outside this plane, AABB is completely outside frustum
        if (normal.dot(positive) < d)
        {
            return false;
        }
    }

    return true;  // AABB intersects or is inside frustum
}

bool Frustum::intersectsSphere(const Vec3& center, float radius) const
{
    // Test sphere against all 6 frustum planes
    for (int i = 0; i < 6; i++)
    {
        Vec3 normal = planes[i];
        float d = distances[i];

        // Distance from sphere center to plane
        float distance = normal.dot(center) - d;

        // If sphere is completely outside this plane
        if (distance < -radius)
        {
            return false;
        }
    }

    return true;  // Sphere intersects or is inside frustum
}

bool Frustum::culls32x32Chunk(const Vec3& chunkWorldPos) const
{
    // 32x32x32 chunk has center at +16,+16,+16 from world pos
    Vec3 chunkCenter = chunkWorldPos + Vec3(16.0f, 16.0f, 16.0f);
    Vec3 halfSize(16.0f, 16.0f, 16.0f);

    return !intersectsAABB(chunkCenter, halfSize);
}

void Frustum::debugPrint() const
{
    std::cout << "🔍 Frustum Planes:" << std::endl;
    const char* names[] = {"Left", "Right", "Top", "Bottom", "Near", "Far"};
    for (int i = 0; i < 6; i++)
    {
        Vec3 n = planes[i];
        std::cout << "  " << names[i] << ": (" << n.x << ", " << n.y << ", " << n.z
                  << ") d=" << distances[i] << std::endl;
    }
}

void FrustumCuller::updateFromCamera(const Camera& camera, float aspect, float fovDegrees)
{
    if (!m_enabled)
        return;

    m_frustum.updateFromCamera(camera, aspect, fovDegrees);
}

bool FrustumCuller::shouldCullChunk(const Vec3& chunkCenter, float chunkRadius) const
{
    if (!m_enabled)
        return false;

    m_stats.chunksConsidered++;

    // Use sphere test for chunks (faster than AABB)
    bool culled = !m_frustum.intersectsSphere(chunkCenter, chunkRadius);

    if (culled)
    {
        m_stats.chunksCulled++;
    }
    else
    {
        m_stats.chunksRendered++;
    }

    return culled;
}

bool FrustumCuller::shouldCullAABB(const Vec3& center, const Vec3& halfSize) const
{
    if (!m_enabled)
        return false;

    return !m_frustum.intersectsAABB(center, halfSize);
}

bool FrustumCuller::shouldCullSphere(const Vec3& center, float radius) const
{
    if (!m_enabled)
        return false;

    return !m_frustum.intersectsSphere(center, radius);
}

bool FrustumCuller::shouldCullByDistance(const Vec3& chunkCenter, const Vec3& cameraPos) const
{
    float distance = (chunkCenter - cameraPos).length();
    return distance > m_renderDistance;
}
