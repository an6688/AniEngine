#pragma once

#include <glm/glm.hpp>

// Orbit camera - rotates around a target point
// DEPENDS ON: GLM only
class Camera
{
public:
    Camera();
    ~Camera();

    // Initialize camera with aspect ratio
    void Initialize(float aspectRatio);

    // Update camera (call once per frame)
    void Update(float deltaTime);

    // Camera controls
    void Orbit(float deltaYaw, float deltaPitch);  // Rotate around target
    void Pan(float deltaX, float deltaY);          // Move target point
    void Zoom(float delta);                        // Move closer/farther

    // Set target point (what the camera looks at)
    void SetTarget(const glm::vec3& target) { m_target = target; }

    // Auto-frame to fit a bounding sphere
    void FrameBounds(const glm::vec3& center, float radius);

    // Get matrices for rendering
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    glm::mat4 GetViewProjectionMatrix() const;

    // Get camera properties
    glm::vec3 GetPosition() const;
    glm::vec3 GetTarget() const { return m_target; }
    float GetDistance() const { return m_distance; }

private:
    void UpdateViewMatrix();

private:
    // Orbit parameters (spherical coordinates)
    float m_yaw;           // Rotation around Y axis (radians)
    float m_pitch;         // Rotation around X axis (radians)
    float m_distance;      // Distance from target

    // Target point (what we're looking at)
    glm::vec3 m_target;

    // Projection parameters
    float m_fov;           // Field of view (radians)
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;

    // Cached matrices
    glm::mat4 m_viewMatrix;
    glm::mat4 m_projectionMatrix;
    bool m_viewDirty;      // View needs recalculation
    bool m_projDirty;      // Projection needs recalculation

    // Movement speeds
    float m_orbitSpeed;
    float m_panSpeed;
    float m_zoomSpeed;
};