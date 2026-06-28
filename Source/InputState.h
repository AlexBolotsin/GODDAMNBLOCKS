#pragma once

#include <cstdint>

struct InputState
{
    int  mouseDeltaX      = 0;
    int  mouseDeltaY      = 0;
    int  mouseAbsX        = 0;     // absolute client-space pixel
    int  mouseAbsY        = 0;
    int  scrollDelta      = 0;
    uint32_t screenW      = 1280;  // client dimensions for NDC conversion
    uint32_t screenH      = 720;
    bool rightMouseHeld   = false; // hold to orbit camera
    bool middleMouseHeld  = false; // hold to pan camera target
    bool leftMouseClick   = false; // edge-triggered: true only on frame button goes down
    bool cinematicToggled = false; // edge-triggered: true only on frame Space is pressed
};
