#pragma once
#include "EngineMath.h"

struct Camera
{
    vec3  eye    = vec3(0.0f, 2.0f,  5.0f);
    vec3  target = vec3(0.0f, 0.0f,  0.0f);
    float fovY   = 1.0471976f;  // ~60 degrees
    float nearZ  = 0.1f;
    float farZ   = 100.0f;
};
