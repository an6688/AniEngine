#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Mesh;
struct Material;
class Texture;

// A single object in the scene (an imported model instance)
struct SceneObject {
    std::string name;
    std::string assetPath;  // Path to the source glTF file

    // Transform
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity
    glm::vec3 scale = glm::vec3(1.0f);

    // Computed world matrix (updated when transform changes)
    glm::mat4 worldMatrix = glm::mat4(1.0f);

    // The loaded mesh data for this object
    // Each SceneObject can have multiple mesh instances (from glTF scene graph)
    struct MeshInstance {
        std::shared_ptr<Mesh> mesh;
        glm::mat4 localTransform;  // Transform relative to the SceneObject
    };
    std::vector<MeshInstance> meshInstances;

    // Shared resources from the loaded model
    std::vector<std::shared_ptr<Mesh>> meshes;
    std::vector<std::shared_ptr<Material>> materials;
    std::vector<std::shared_ptr<Texture>> textures;

    // Bounds (local space, for framing)
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);

    // State
    bool isSelected = false;
    bool isVisible = true;

    // Update world matrix from position/rotation/scale
    void UpdateWorldMatrix() {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 r = glm::mat4_cast(rotation);
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
        worldMatrix = t * r * s;
    }

    // Get the full transform for a mesh instance
    glm::mat4 GetMeshWorldMatrix(size_t instanceIndex) const {
        if (instanceIndex >= meshInstances.size()) {
            return worldMatrix;
        }
        return worldMatrix * meshInstances[instanceIndex].localTransform;
    }
};

// Scene lights
struct SceneLight {
    enum class Type { Directional, Point, Spot };

    std::string name;
    Type type = Type::Directional;

    glm::vec3 position = glm::vec3(0.0f, 5.0f, 0.0f);
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 color = glm::vec3(1.0f);

    float intensity = 1.0f;
    float range = 10.0f;          // For point/spot
    float innerConeAngle = 30.0f; // For spot (degrees)
    float outerConeAngle = 45.0f; // For spot (degrees)

    bool castsShadows = false;
    bool isEnabled = true;
};

// The complete scene
struct Scene {
    std::string name = "Untitled";
    std::string filePath;  // Path to saved scene file (empty if not saved)

    std::vector<std::unique_ptr<SceneObject>> objects;
    std::vector<SceneLight> lights;

    // Environment
    glm::vec3 ambientColor = glm::vec3(0.1f);
    std::string skyboxPath;  // Future: HDR environment map

    // Editor state (not saved)
    int selectedObjectIndex = -1;
    bool isDirty = false;  // Has unsaved changes

    // Helpers
    SceneObject* GetSelectedObject() {
        if (selectedObjectIndex >= 0 && selectedObjectIndex < static_cast<int>(objects.size())) {
            return objects[selectedObjectIndex].get();
        }
        return nullptr;
    }

    void ClearSelection() {
        for (auto& obj : objects) {
            obj->isSelected = false;
        }
        selectedObjectIndex = -1;
    }

    void SelectObject(int index) {
        ClearSelection();
        if (index >= 0 && index < static_cast<int>(objects.size())) {
            selectedObjectIndex = index;
            objects[index]->isSelected = true;
        }
    }

    void MarkDirty() {
        isDirty = true;
    }

    // Calculate world bounds encompassing all objects
    void GetWorldBounds(glm::vec3& outMin, glm::vec3& outMax) const {
        if (objects.empty()) {
            outMin = glm::vec3(-1.0f);
            outMax = glm::vec3(1.0f);
            return;
        }

        outMin = glm::vec3(FLT_MAX);
        outMax = glm::vec3(-FLT_MAX);

        for (const auto& obj : objects) {
            if (!obj->isVisible) {
                continue;
            }

            // Transform object bounds to world space (approximate with corners)
            glm::vec3 corners[8] = {
                { obj->boundsMin.x, obj->boundsMin.y, obj->boundsMin.z },
                { obj->boundsMax.x, obj->boundsMin.y, obj->boundsMin.z },
                { obj->boundsMin.x, obj->boundsMax.y, obj->boundsMin.z },
                { obj->boundsMax.x, obj->boundsMax.y, obj->boundsMin.z },
                { obj->boundsMin.x, obj->boundsMin.y, obj->boundsMax.z },
                { obj->boundsMax.x, obj->boundsMin.y, obj->boundsMax.z },
                { obj->boundsMin.x, obj->boundsMax.y, obj->boundsMax.z },
                { obj->boundsMax.x, obj->boundsMax.y, obj->boundsMax.z },
            };

            for (const auto& corner : corners) {
                glm::vec4 worldCorner = obj->worldMatrix * glm::vec4(corner, 1.0f);
                outMin = glm::min(outMin, glm::vec3(worldCorner));
                outMax = glm::max(outMax, glm::vec3(worldCorner));
            }
        }
    }
};