#pragma once

struct InputState
{
    int mouseDeltaX = 0;
    int mouseDeltaY = 0;
    int scrollDelta = 0;  // raw wheel units (±120 per notch)
};
