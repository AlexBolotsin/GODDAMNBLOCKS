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
