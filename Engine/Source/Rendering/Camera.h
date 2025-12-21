#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
    Camera();
    ~Camera();

    void Initialize(float aspectRatio);
    void Update(float deltaTime);

    // Camera controls
    void Orbit(float deltaYaw, float deltaPitch);
    void Pan(float deltaX, float deltaY);
    void PanForward(float delta);
    void Zoom(float delta);

    // Frame the camera to view bounds
    void FrameBounds(const glm::vec3& center, float radius);

    // Frame with explicit bounds (min/max)
    void FrameBoundsMinMax(const glm::vec3& boundsMin, const glm::vec3& boundsMax);

    // Getters
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    glm::vec3 GetPosition() const { return m_position; }
    glm::vec3 GetTarget() const { return m_target; }
    float GetDistance() const { return m_distance; }

    // Setters
    void SetAspectRatio(float aspectRatio);
    void SetFOV(float fovDegrees);
    void SetNearFar(float nearPlane, float farPlane);
    void SetTarget(const glm::vec3& target);
    void SetDistance(float distance);

private:
    void UpdatePosition();

private:
    // Camera position is calculated from target + spherical coordinates
    glm::vec3 m_position;
    glm::vec3 m_target;

    // Spherical coordinates for orbit camera
    float m_yaw;        // Horizontal angle (radians)
    float m_pitch;      // Vertical angle (radians)
    float m_distance;   // Distance from target

    // Projection parameters
    float m_fov;        // In radians
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;

    // Constraints
    static constexpr float m_MinPitch = -1.5f;  // ~ -86 degrees
    static constexpr float m_MaxPitch = 1.5f;   // ~ 86 degrees
    static constexpr float m_MinDistance = 0.01f;
};
