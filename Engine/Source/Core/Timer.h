#pragma once

#include <windows.h>

// High-precision timer for game loop timing
// NO DEPENDENCIES - Pure utility class
class Timer
{
public:
    Timer();

    // Start/reset the timer
    void Start();
    
    // Call once per frame to update delta time
    void Tick();
    
    // Get time since last Tick() in seconds
    float GetDeltaTime() const { return m_deltaTime; }
    
    // Get total elapsed time since Start() in seconds
    float GetTotalTime() const { return m_totalTime; }
    
    // Get current frames per second
    float GetFPS() const { return m_fps; }

private:
    LARGE_INTEGER m_frequency;      // Ticks per second
    LARGE_INTEGER m_startTime;      // Time when timer started
    LARGE_INTEGER m_currentTime;    // Current frame time
    LARGE_INTEGER m_previousTime;   // Previous frame time
    
    float m_deltaTime;              // Time between frames (seconds)
    float m_totalTime;              // Total elapsed time (seconds)
    
    // FPS calculation
    float m_fps;
    int m_frameCount;
    float m_fpsTimer;
};