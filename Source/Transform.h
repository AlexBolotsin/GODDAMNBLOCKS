#pragma once

#include "EngineMath.h"

class Transform
{
public:
    vec3 position = vec3(0.0f, 0.0f, 0.0f);
    quat rotation = QuatIdentity();
    vec3 scale = vec3(1.0f, 1.0f, 1.0f);

    mat4 GetWorldMatrix() const;
    
    void SetPosition(float x, float y, float z);
    void SetPosition(const vec3& p);
    
    void SetRotation(const quat& q);
    void SetScale(float x, float y, float z);
    void SetScale(const vec3& s);
    
    void Translate(float x, float y, float z);
    void Rotate(const quat& q);
};
