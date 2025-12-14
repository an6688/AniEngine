#include "Timer.h"

Timer::Timer()
    : m_deltaTime(0.0f)
    , m_totalTime(0.0f)
    , m_fps(0.0f)
    , m_frameCount(0)
    , m_fpsTimer(0.0f)
{
    // Get frequency (ticks per second)
    QueryPerformanceFrequency(&m_frequency);

    m_startTime = {};
    m_currentTime = {};
    m_previousTime = {};
}

void Timer::Start()
{
    QueryPerformanceCounter(&m_startTime);
    m_previousTime = m_startTime;
    m_totalTime = 0.0f;
    m_frameCount = 0;
    m_fpsTimer = 0.0f;
}

void Timer::Tick()
{
    // Get current time
    QueryPerformanceCounter(&m_currentTime);

    // Calculate delta time in seconds
    LONGLONG delta = m_currentTime.QuadPart - m_previousTime.QuadPart;
    m_deltaTime = static_cast<float>(delta) / static_cast<float>(m_frequency.QuadPart);

    // Calculate total time
    LONGLONG total = m_currentTime.QuadPart - m_startTime.QuadPart;
    m_totalTime = static_cast<float>(total) / static_cast<float>(m_frequency.QuadPart);

    // Store for next frame
    m_previousTime = m_currentTime;

    // FPS calculation (update every second)
    m_frameCount++;
    m_fpsTimer += m_deltaTime;

    if (m_fpsTimer >= 1.0f)
    {
        m_fps = static_cast<float>(m_frameCount) / m_fpsTimer;
        m_frameCount = 0;
        m_fpsTimer = 0.0f;
    }
}