// Vec3.h - Lightweight 3D vector math for voxel engine
#pragma once
#include <cmath>
#include <algorithm>

struct Vec3
{
    float x, y, z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    // Basic operations
    Vec3 operator+(const Vec3& other) const
    {
        return {x + other.x, y + other.y, z + other.z};
    }
    Vec3 operator-(const Vec3& other) const
    {
        return {x - other.x, y - other.y, z - other.z};
    }
    Vec3 operator*(float scalar) const
    {
        return {x * scalar, y * scalar, z * scalar};
    }
    Vec3 operator/(float scalar) const
    {
        return {x / scalar, y / scalar, z / scalar};
    }
    Vec3& operator+=(const Vec3& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    Vec3& operator-=(const Vec3& other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    Vec3& operator*=(float scalar)
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }
    Vec3& operator/=(float scalar)
    {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    // Comparison operators for use in std::map
    bool operator<(const Vec3& other) const
    {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }
    
    bool operator==(const Vec3& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
    
    bool operator!=(const Vec3& other) const
    {
        return !(*this == other);
    }

    // Vector operations
    float dot(const Vec3& other) const
    {
        return x * other.x + y * other.y + z * other.z;
    }
    Vec3 cross(const Vec3& other) const
    {
        return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x};
    }
    float length() const
    {
        return static_cast<float>(std::sqrt(x * x + y * y + z * z));
    }
    Vec3 normalized() const
    {
        float l = length();
        return l > 0.0f ? *this * (1.0f / l) : Vec3();
    }

    // Distance functions
    float distance(const Vec3& other) const
    {
        return (*this - other).length();
    }

    float distanceSquared(const Vec3& other) const
    {
        return (*this - other).lengthSquared();
    }

    float lengthSquared() const
    {
        return x * x + y * y + z * z;
    }

    // Component-wise min/max
    static Vec3 min(const Vec3& a, const Vec3& b)
    {
        return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
    }
    static Vec3 max(const Vec3& a, const Vec3& b)
    {
        return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
    }
};

// Hash function for Vec3 to enable use in std::unordered_set and std::unordered_map
namespace std {
    template <>
    struct hash<Vec3> {
        size_t operator()(const Vec3& v) const {
            // Combine hashes using bit shifts and XOR
            size_t h1 = std::hash<float>()(v.x);
            size_t h2 = std::hash<float>()(v.y);
            size_t h3 = std::hash<float>()(v.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}
