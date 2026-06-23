#include "Transform.h"
#include <cmath>

quat QuatRotationAxis(const vec3& axis, float angle)
{
    const float lengthSq = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
    if (lengthSq <= 1e-12f)
    {
        return QuatIdentity();
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    const float nx = axis.x * invLength;
    const float ny = axis.y * invLength;
    const float nz = axis.z * invLength;

    const float halfAngle = angle * 0.5f;
    const float s = std::sin(halfAngle);
    const float c = std::cos(halfAngle);

    return quat(nx * s, ny * s, nz * s, c);
}

mat4 Transform::GetWorldMatrix() const
{
    mat4 scaleMatrix = MatrixScaling(scale.x, scale.y, scale.z);
    mat4 rotationMatrix = MatrixRotationQuaternion(rotation);
    mat4 translationMatrix = MatrixTranslation(position.x, position.y, position.z);

    mat4 result = MatrixMultiply(scaleMatrix, rotationMatrix);
    result = MatrixMultiply(result, translationMatrix);
    
    return result;
}

void Transform::SetPosition(float x, float y, float z)
{
    position = vec3(x, y, z);
}

void Transform::SetPosition(const vec3& p)
{
    position = p;
}

void Transform::SetRotation(const quat& q)
{
    rotation = q;
}

void Transform::SetScale(float x, float y, float z)
{
    scale = vec3(x, y, z);
}

void Transform::SetScale(const vec3& s)
{
    scale = s;
}

void Transform::Translate(float x, float y, float z)
{
    position.x += x;
    position.y += y;
    position.z += z;
}

void Transform::Rotate(const quat& q)
{
    const quat& a = rotation;
    const quat& b = q;

    rotation = quat(
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);

    const float lengthSq = rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z + rotation.w * rotation.w;
    if (lengthSq > 1e-12f)
    {
        const float invLength = 1.0f / std::sqrt(lengthSq);
        rotation.x *= invLength;
        rotation.y *= invLength;
        rotation.z *= invLength;
        rotation.w *= invLength;
    }
    else
    {
        rotation = QuatIdentity();
    }
}
