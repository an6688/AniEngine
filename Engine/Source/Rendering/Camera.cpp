#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <glm/glm.hpp>

Camera::Camera()
    : m_yaw(0.0f)
    , m_pitch(glm::radians(30.0f))  // Start 30 degrees above horizon
    , m_distance(5.0f)
    , m_target(0.0f, 0.0f, 0.0f)
    , m_fov(glm::radians(60.0f))
    , m_aspectRatio(16.0f / 9.0f)
    , m_nearPlane(0.01f)
    , m_farPlane(1000.0f)
    , m_viewMatrix(1.0f)
    , m_projectionMatrix(1.0f)
    , m_viewDirty(true)
    , m_projDirty(true)
    , m_orbitSpeed(1.0f)
    , m_panSpeed(1.0f)
    , m_zoomSpeed(1.0f)
{
}

Camera::~Camera()
{
}

void Camera::Initialize(float aspectRatio)
{
    m_aspectRatio = aspectRatio;
    m_projDirty = true;
}

void Camera::Update(float deltaTime)
{
    (void)deltaTime;  // May use for smooth interpolation later

    if (m_viewDirty)
    {
        UpdateViewMatrix();
        m_viewDirty = false;
    }

    if (m_projDirty)
    {
        m_projectionMatrix = glm::perspective(
            m_fov,
            m_aspectRatio,
            m_nearPlane,
            m_farPlane
        );
        m_projDirty = false;
    }
}

void Camera::Orbit(float deltaYaw, float deltaPitch)
{
    m_yaw += deltaYaw * m_orbitSpeed;
    m_pitch += deltaPitch * m_orbitSpeed;

    // Clamp pitch to prevent gimbal lock
    const float maxPitch = glm::radians(89.0f);
    m_pitch = glm::clamp(m_pitch, -maxPitch, maxPitch);

    m_viewDirty = true;
}

void Camera::Pan(float deltaX, float deltaY)
{
    // Get camera right and up vectors
    glm::vec3 position = GetPosition();
    glm::vec3 forward = glm::normalize(m_target - position);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::cross(right, forward);

    // Move target (and camera follows since distance stays same)
    float panScale = m_distance * m_panSpeed * 0.001f;
    m_target += right * deltaX * panScale;
    m_target += up * deltaY * panScale;

    m_viewDirty = true;
}

void Camera::PanForward(float amount)
{
    glm::vec3 position = GetPosition();
    glm::vec3 forward = glm::normalize(m_target - position);

    // Constrain to ground plane
    forward.y = 0.0f;
    if (glm::dot(forward, forward) < 0.0001f) return;

    forward = glm::normalize(forward);

    m_target += forward * amount;
    m_viewDirty = true;
}

void Camera::Zoom(float delta)
{
    m_distance -= delta * m_zoomSpeed;

    // Clamp distance to reasonable values
    m_distance = glm::clamp(m_distance, 0.1f, 1000.0f);

    m_viewDirty = true;
}

void Camera::FrameBounds(const glm::vec3& center, float radius)
{
    m_target = center;

    // Calculate distance to fit sphere in view
    // Distance = radius / sin(fov/2)
    float halfFov = m_fov * 0.5f;
    m_distance = (radius * 1.5f) / glm::sin(halfFov);

    // Clamp to reasonable range
    m_distance = glm::clamp(m_distance, 0.5f, 1000.0f);

    m_viewDirty = true;
}

glm::mat4 Camera::GetViewMatrix() const
{
    return m_viewMatrix;
}

glm::mat4 Camera::GetProjectionMatrix() const
{
    return m_projectionMatrix;
}

glm::mat4 Camera::GetViewProjectionMatrix() const
{
    return m_projectionMatrix * m_viewMatrix;
}

glm::vec3 Camera::GetPosition() const
{
    // Calculate position from spherical coordinates
    float x = m_distance * glm::cos(m_pitch) * glm::sin(m_yaw);
    float y = m_distance * glm::sin(m_pitch);
    float z = m_distance * glm::cos(m_pitch) * glm::cos(m_yaw);

    return m_target + glm::vec3(x, y, z);
}

void Camera::UpdateViewMatrix()
{
    glm::vec3 position = GetPosition();
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    m_viewMatrix = glm::lookAt(position, m_target, up);
}