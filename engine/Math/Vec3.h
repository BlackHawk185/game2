// Vec3.h - Lightweight 3D vector math for voxel engine
#pragma once
#include <cmath>

struct Vec3 {
    float x, y, z;
    
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    
    // Basic operations
    Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
    Vec3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
    
    // Vector operations
    float dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
    Vec3 cross(const Vec3& other) const { 
        return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x}; 
    }
    float length() const { return sqrt(x*x + y*y + z*z); }
    Vec3 normalized() const { float l = length(); return l > 0 ? *this * (1.0f/l) : Vec3(); }
    
    // Distance functions
    float distance(const Vec3& other) const {
        return (*this - other).length();
    }
    
    float distanceSquared(const Vec3& other) const {
        return (*this - other).lengthSquared();
    }
    
    float lengthSquared() const { return x*x + y*y + z*z; }
    
    // Array access for OpenGL compatibility  
    float& operator[](int i) { return (&x)[i]; }
    const float& operator[](int i) const { return (&x)[i]; }
    float* data() { return &x; }
    const float* data() const { return &x; }
};

struct Mat4 {
    float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; // Identity by default
    
    float* data() { return m; }
    const float* data() const { return m; }
    
    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up);
    static Mat4 perspective(float fov, float aspect, float near, float far);
    static Mat4 translation(const Vec3& pos);
};
