#pragma once

struct InputState
{
    int  mouseDeltaX      = 0;
    int  mouseDeltaY      = 0;
    int  scrollDelta      = 0;   // raw wheel units (±120 per notch)
    bool cinematicToggled = false; // edge-triggered: true only on the frame Space is pressed
    bool summonMeteors    = false; // edge-triggered: true only on the frame M is pressed
};
