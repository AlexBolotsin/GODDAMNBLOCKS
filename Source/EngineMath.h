#pragma once

#include <cstring>

struct vec3
{
    float x, y, z;
    vec3() : x(0), y(0), z(0) {}
    vec3(float x, float y, float z) : x(x), y(y), z(z) {}
};

struct vec2
{
    float x, y;
    vec2() : x(0), y(0) {}
    vec2(float x, float y) : x(x), y(y) {}
};

struct vec4
{
    float x, y, z, w;
    vec4() : x(0), y(0), z(0), w(0) {}
    vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};

struct quat
{
    float x, y, z, w;
    quat() : x(0), y(0), z(0), w(1) {}
    quat(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};

struct mat4
{
    float m[16];

    mat4()
    {
        memset(m, 0, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    mat4(const float *data)
    {
        memcpy(m, data, sizeof(m));
    }
};

inline mat4 MatrixIdentity()
{
    return mat4();
}

inline mat4 MatrixTranslation(float x, float y, float z)
{
    mat4 result;
    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;
    return result;
}

inline mat4 MatrixScaling(float x, float y, float z)
{
    mat4 result;
    result.m[0] = x;
    result.m[5] = y;
    result.m[10] = z;
    return result;
}

inline mat4 MatrixRotationQuaternion(const quat &q)
{
    mat4 result;
    float x2 = q.x + q.x;
    float y2 = q.y + q.y;
    float z2 = q.z + q.z;

    float wx2 = q.w * x2;
    float wy2 = q.w * y2;
    float wz2 = q.w * z2;
    float xx2 = q.x * x2;
    float xy2 = q.x * y2;
    float xz2 = q.x * z2;
    float yy2 = q.y * y2;
    float yz2 = q.y * z2;
    float zz2 = q.z * z2;

    result.m[0] = 1.0f - (yy2 + zz2);
    result.m[1] = xy2 + wz2;
    result.m[2] = xz2 - wy2;

    result.m[4] = xy2 - wz2;
    result.m[5] = 1.0f - (xx2 + zz2);
    result.m[6] = yz2 + wx2;

    result.m[8] = xz2 + wy2;
    result.m[9] = yz2 - wx2;
    result.m[10] = 1.0f - (xx2 + yy2);

    return result;
}

inline mat4 MatrixMultiply(const mat4 &a, const mat4 &b)
{
    mat4 result;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                sum += a.m[i * 4 + k] * b.m[k * 4 + j];
            }
            result.m[i * 4 + j] = sum;
        }
    }
    return result;
}

inline vec3 Vec3(float x, float y, float z)
{
    return vec3(x, y, z);
}

inline vec4 Vec4(float x, float y, float z, float w)
{
    return vec4(x, y, z, w);
}

inline quat QuatIdentity()
{
    return quat(0, 0, 0, 1);
}

// Note: QuatRotationAxis is defined in Transform.cpp due to sin/cos requirements
quat QuatRotationAxis(const vec3 &axis, float angle);

// ---------------------------------------------------------------
//  vec3 operators
// ---------------------------------------------------------------

inline vec3 operator+(const vec3 &a, const vec3 &b) { return vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline vec3 operator-(const vec3 &a, const vec3 &b) { return vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline vec3 operator*(const vec3 &v, float s)        { return vec3(v.x * s, v.y * s, v.z * s); }

inline float Vec3Dot(const vec3 &a, const vec3 &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline vec3 Vec3Cross(const vec3 &a, const vec3 &b)
{
    return vec3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

inline vec3 Vec3Normalize(const vec3 &v)
{
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lenSq <= 1e-12f)
        return vec3(0.0f, 0.0f, 0.0f);
    const float invLen = 1.0f / sqrtf(lenSq);
    return vec3(v.x * invLen, v.y * invLen, v.z * invLen);
}

// ---------------------------------------------------------------
//  Camera / scene matrix builders
// ---------------------------------------------------------------

inline mat4 MatrixLookAtRH(const vec3 &eye, const vec3 &target, const vec3 &up)
{
    const vec3 zAxis = Vec3Normalize(eye - target);
    const vec3 xAxis = Vec3Normalize(Vec3Cross(up, zAxis));
    const vec3 yAxis = Vec3Cross(zAxis, xAxis);

    mat4 view;
    view.m[0]  = xAxis.x;  view.m[1]  = yAxis.x;  view.m[2]  = zAxis.x;  view.m[3]  = 0.0f;
    view.m[4]  = xAxis.y;  view.m[5]  = yAxis.y;  view.m[6]  = zAxis.y;  view.m[7]  = 0.0f;
    view.m[8]  = xAxis.z;  view.m[9]  = yAxis.z;  view.m[10] = zAxis.z;  view.m[11] = 0.0f;
    view.m[12] = -Vec3Dot(xAxis, eye);
    view.m[13] = -Vec3Dot(yAxis, eye);
    view.m[14] = -Vec3Dot(zAxis, eye);
    view.m[15] = 1.0f;
    return view;
}

inline mat4 MatrixPerspectiveRH(float fovYRadians, float aspect, float nearZ, float farZ)
{
    mat4 result;
    const float f = 1.0f / tanf(fovYRadians * 0.5f);
    result.m[0]  = f / aspect;
    result.m[5]  = f;
    result.m[10] = farZ / (nearZ - farZ);
    result.m[11] = -1.0f;
    result.m[14] = nearZ * farZ / (nearZ - farZ);
    result.m[15] = 0.0f;
    return result;
}

inline mat4 MatrixShadowProjection(float planeY, const vec3 &rayDir)
{
    mat4 shadow;
    const float safeY = (fabsf(rayDir.y) > 1e-4f) ? rayDir.y : -1e-4f;
    const float kx = rayDir.x / safeY;
    const float kz = rayDir.z / safeY;

    shadow.m[0]  = 1.0f;  shadow.m[4]  = -kx;   shadow.m[8]  = 0.0f;  shadow.m[12] = kx * planeY;
    shadow.m[1]  = 0.0f;  shadow.m[5]  = 0.0f;  shadow.m[9]  = 0.0f;  shadow.m[13] = planeY;
    shadow.m[2]  = 0.0f;  shadow.m[6]  = -kz;   shadow.m[10] = 1.0f;  shadow.m[14] = kz * planeY;
    shadow.m[3]  = 0.0f;  shadow.m[7]  = 0.0f;  shadow.m[11] = 0.0f;  shadow.m[15] = 1.0f;
    return shadow;
}

inline mat4 MatrixBillboard(const vec3 &position, const vec3 &scale, const vec3 &cameraEye)
{
    vec3 toCamera = Vec3Normalize(cameraEye - position);
    if (toCamera.x == 0.0f && toCamera.y == 0.0f && toCamera.z == 0.0f)
        toCamera = vec3(0.0f, 0.0f, 1.0f);

    const vec3 worldUp(0.0f, 1.0f, 0.0f);
    vec3 right = Vec3Cross(worldUp, toCamera);
    if (Vec3Dot(right, right) <= 1e-8f)
        right = vec3(1.0f, 0.0f, 0.0f);
    else
        right = Vec3Normalize(right);

    const vec3 up = Vec3Normalize(Vec3Cross(toCamera, right));

    mat4 world;
    world.m[0]  = right.x * scale.x;    world.m[1]  = right.y * scale.x;    world.m[2]  = right.z * scale.x;    world.m[3]  = 0.0f;
    world.m[4]  = up.x * scale.y;       world.m[5]  = up.y * scale.y;       world.m[6]  = up.z * scale.y;       world.m[7]  = 0.0f;
    world.m[8]  = toCamera.x * scale.z; world.m[9]  = toCamera.y * scale.z; world.m[10] = toCamera.z * scale.z; world.m[11] = 0.0f;
    world.m[12] = position.x;           world.m[13] = position.y;            world.m[14] = position.z;            world.m[15] = 1.0f;
    return world;
}
