// Vec3.cpp - Implementation of lightweight 3D vector math
#include "Vec3.h"
#include <cstring>

Mat4 Mat4::lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = (center - eye).normalized();
    Vec3 s = f.cross(up).normalized();
    Vec3 u = s.cross(f);

    Mat4 result;
    result.m[0] = s.x;
    result.m[1] = u.x;
    result.m[2] = -f.x;
    result.m[3] = 0.0f;
    result.m[4] = s.y;
    result.m[5] = u.y;
    result.m[6] = -f.y;
    result.m[7] = 0.0f;
    result.m[8] = s.z;
    result.m[9] = u.z;
    result.m[10] = -f.z;
    result.m[11] = 0.0f;
    result.m[12] = -s.dot(eye);
    result.m[13] = -u.dot(eye);
    result.m[14] = f.dot(eye);
    result.m[15] = 1.0f;
    
    return result;
}

Mat4 Mat4::perspective(float fov, float aspect, float near, float far) {
    float tanHalfFov = tan(fov / 2.0f);
    
    Mat4 result;
    memset(result.m, 0, sizeof(result.m));
    
    result.m[0] = 1.0f / (aspect * tanHalfFov);
    result.m[5] = 1.0f / tanHalfFov;
    result.m[10] = -(far + near) / (far - near);
    result.m[11] = -1.0f;
    result.m[14] = -(2.0f * far * near) / (far - near);
    
    return result;
}

Mat4 Mat4::translation(const Vec3& pos) {
    Mat4 result;
    result.m[12] = pos.x;
    result.m[13] = pos.y;
    result.m[14] = pos.z;
    return result;
}
