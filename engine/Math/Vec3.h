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

struct Mat4
{
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};  // Identity by default

    float* data()
    {
        return m;
    }
    const float* data() const
    {
        return m;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up);
    static Mat4 perspective(float fov, float aspect, float znear, float zfar);
    static Mat4 translation(const Vec3& pos);
};
