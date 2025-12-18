#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <glm/glm.hpp>

Camera::Camera()
    : m_position(0.0f, 0.0f, 5.0f)
    , m_target(0.0f, 0.0f, 0.0f)
    , m_yaw(0.0f)
    , m_pitch(0.3f)  // Slightly above horizontal
    , m_distance(5.0f)
    , m_fov(glm::radians(45.0f))
    , m_aspectRatio(16.0f / 9.0f)
    , m_nearPlane(0.01f)
    , m_farPlane(1000.0f)
{
}

Camera::~Camera()
{
}

void Camera::Initialize(float aspectRatio)
{
    m_aspectRatio = aspectRatio;
    UpdatePosition();
}

void Camera::Update(float deltaTime)
{
    // Currently nothing to update each frame
    // Could add smooth interpolation here
}

void Camera::Orbit(float deltaYaw, float deltaPitch)
{
    m_yaw += deltaYaw;
    m_pitch += deltaPitch;

    // Clamp pitch to avoid gimbal lock
    m_pitch = std::clamp(m_pitch, m_MinPitch, m_MaxPitch);

    // Wrap yaw
    if (m_yaw > glm::pi<float>() * 2.0f)
        m_yaw -= glm::pi<float>() * 2.0f;
    if (m_yaw < 0.0f)
        m_yaw += glm::pi<float>() * 2.0f;

    UpdatePosition();
}

void Camera::Pan(float deltaX, float deltaY)
{
    // Calculate right and up vectors from current view
    glm::vec3 forward = glm::normalize(m_target - m_position);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    // Scale pan speed by distance (feels more natural)
    float panScale = m_distance * 0.002f;

    glm::vec3 panOffset = right * deltaX * panScale + up * deltaY * panScale;
    m_target += panOffset;

    UpdatePosition();
}

void Camera::PanForward(float delta)
{
    // Move target forward/backward along view direction (on XZ plane)
    glm::vec3 forward = glm::normalize(m_target - m_position);
    forward.y = 0.0f;  // Keep movement horizontal
    if (glm::length(forward) > 0.001f)
    {
        forward = glm::normalize(forward);
    }
    else
    {
        forward = glm::vec3(0.0f, 0.0f, -1.0f);
    }

    float moveScale = m_distance * 0.002f;
    m_target += forward * delta * moveScale;

    UpdatePosition();
}

void Camera::Zoom(float delta)
{
    // Zoom by adjusting distance
    // Use multiplicative zoom for consistent feel at all distances
    float zoomFactor = 1.0f + delta * 0.1f;
    m_distance *= zoomFactor;
    m_distance = std::max(m_distance, m_MinDistance);

    UpdatePosition();
}

void Camera::FrameBounds(const glm::vec3& center, float radius)
{
    // Set target to center of bounds
    m_target = center;

    // Calculate distance needed to fit the object in view
    // Using FOV: distance = radius / tan(fov/2)
    // Add some margin (1.5x) so the object doesn't fill the entire screen
    float halfFovTan = std::tan(m_fov * 0.5f);
    m_distance = (radius * 1.5f) / halfFovTan;

    // Ensure minimum distance
    m_distance = std::max(m_distance, radius * 2.0f);
    m_distance = std::max(m_distance, m_MinDistance);

    // Set a nice viewing angle (slightly above and to the side)
    m_yaw = glm::radians(45.0f);    // 45 degrees to the side
    m_pitch = glm::radians(25.0f);  // 25 degrees above

    // Adjust near/far planes based on object size
    m_nearPlane = std::max(0.01f, radius * 0.01f);
    m_farPlane = std::max(1000.0f, m_distance * 10.0f);

    UpdatePosition();
}

void Camera::FrameBoundsMinMax(const glm::vec3& boundsMin, const glm::vec3& boundsMax)
{
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    float radius = glm::length(boundsMax - boundsMin) * 0.5f;
    FrameBounds(center, radius);
}

void Camera::UpdatePosition()
{
    // Calculate position from spherical coordinates
    // Position = Target + (distance * direction from spherical coords)
    float x = m_distance * std::cos(m_pitch) * std::sin(m_yaw);
    float y = m_distance * std::sin(m_pitch);
    float z = m_distance * std::cos(m_pitch) * std::cos(m_yaw);

    m_position = m_target + glm::vec3(x, y, z);
}

glm::mat4 Camera::GetViewMatrix() const
{
    return glm::lookAt(m_position, m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::GetProjectionMatrix() const
{
    return glm::perspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
}

void Camera::SetAspectRatio(float aspectRatio)
{
    m_aspectRatio = aspectRatio;
}

void Camera::SetFOV(float fovDegrees)
{
    m_fov = glm::radians(fovDegrees);
}

void Camera::SetNearFar(float nearPlane, float farPlane)
{
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}
