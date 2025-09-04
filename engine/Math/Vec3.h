// Vec3.h - Lightweight 3D vector math for voxel engine
#pragma once

// Prevent Windows min/max macro conflicts
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

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
    static Vec3 componentMin(const Vec3& a, const Vec3& b)
    {
        return {(a.x < b.x) ? a.x : b.x, (a.y < b.y) ? a.y : b.y, (a.z < b.z) ? a.z : b.z};
    }
    static Vec3 componentMax(const Vec3& a, const Vec3& b)
    {
        return {(a.x > b.x) ? a.x : b.x, (a.y > b.y) ? a.y : b.y, (a.z > b.z) ? a.z : b.z};
    }
};
