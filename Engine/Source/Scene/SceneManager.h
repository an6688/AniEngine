#pragma once

#include "Scene.h"
#include <string>
#include <functional>

class RenderDevice;
class TextureManager;
class ModelLoader;

// Manages scene operations: create, load, save, add/remove objects
class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    bool Initialize(RenderDevice* device, TextureManager* textureManager);
    void Shutdown();

    // Scene lifecycle
    void NewScene(const std::string& name = "Untitled");
    bool LoadScene(const std::string& filePath);
    bool SaveScene();
    bool SaveSceneAs(const std::string& filePath);

    // Object management
    bool AddObjectFromFile(const std::string& glTFPath);
    bool AddObjectFromFile(const std::string& glTFPath, const glm::vec3& position);
    void RemoveObject(int index);
    void DuplicateObject(int index);

    // Selection
    void SelectObject(int index);
    void ClearSelection();
    SceneObject* GetSelectedObject();
    int GetSelectedIndex() const;

    // Access
    Scene* GetScene() { return m_scene.get(); }
    const Scene* GetScene() const { return m_scene.get(); }

    bool HasUnsavedChanges() const;
    const std::string& GetSceneName() const;
    const std::string& GetScenePath() const;

private:
    bool LoadModelIntoObject(SceneObject* object, const std::string& glTFPath);
    std::string GenerateUniqueName(const std::string& baseName);

    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<ModelLoader> m_modelLoader;
    RenderDevice* m_device;
    TextureManager* m_textureManager;
    int m_objectCounter;  // For generating unique names
};