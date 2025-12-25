#include "SceneManager.h"
#include "Rendering/ModelLoader.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/TextureManager.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"

#include <fstream>
#include <filesystem>
#include <sstream>

// Simple JSON-ish serialization (you could replace with nlohmann/json later)
namespace {

    std::string Vec3ToString(const glm::vec3& v) {
        std::ostringstream ss;
        ss << v.x << "," << v.y << "," << v.z;
        return ss.str();
    }

    std::string QuatToString(const glm::quat& q) {
        std::ostringstream ss;
        ss << q.w << "," << q.x << "," << q.y << "," << q.z;
        return ss.str();
    }

    glm::vec3 StringToVec3(const std::string& s) {
        glm::vec3 v(0.0f);
        sscanf_s(s.c_str(), "%f,%f,%f", &v.x, &v.y, &v.z);
        return v;
    }

    glm::quat StringToQuat(const std::string& s) {
        glm::quat q(1.0f, 0.0f, 0.0f, 0.0f);
        sscanf_s(s.c_str(), "%f,%f,%f,%f", &q.w, &q.x, &q.y, &q.z);
        return q;
    }

    std::string EscapeString(const std::string& s) {
        std::string result;
        for (char c : s) {
            if (c == '\\') {
                result += "\\\\";
            }
            else if (c == '"') {
                result += "\\\"";
            }
            else {
                result += c;
            }
        }
        return result;
    }

} // anonymous namespace

SceneManager::SceneManager()
    : m_device(nullptr)
    , m_textureManager(nullptr)
    , m_objectCounter(0) {
}

SceneManager::~SceneManager() {
    Shutdown();
}

bool SceneManager::Initialize(RenderDevice* device, TextureManager* textureManager) {
    if (!device || !textureManager) {
        return false;
    }

    m_device = device;
    m_textureManager = textureManager;

    m_modelLoader = std::make_unique<ModelLoader>();
    m_modelLoader->SetTextureManager(textureManager);

    NewScene();

    return true;
}

void SceneManager::Shutdown() {
    m_scene.reset();
    m_modelLoader.reset();
    m_device = nullptr;
    m_textureManager = nullptr;
}

void SceneManager::NewScene(const std::string& name) {
    m_scene = std::make_unique<Scene>();
    m_scene->name = name;
    m_scene->filePath.clear();
    m_scene->isDirty = false;
    m_objectCounter = 0;

    // Add a default directional light
    SceneLight defaultLight;
    defaultLight.name = "Directional Light";
    defaultLight.type = SceneLight::Type::Directional;
    defaultLight.direction = glm::normalize(glm::vec3(0.5f, -1.0f, 0.5f));
    defaultLight.intensity = 2.0f;
    m_scene->lights.push_back(defaultLight);
}

bool SceneManager::LoadScene(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        OutputDebugStringA(("Failed to open scene file: " + filePath + "\n").c_str());
        return false;
    }

    NewScene();
    m_scene->filePath = filePath;

    std::string line;
    SceneObject* currentObject = nullptr;

    while (std::getline(file, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) {
            continue;
        }
        line = line.substr(start);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        // Trim value
        start = value.find_first_not_of(" \t");
        if (start != std::string::npos) {
            value = value.substr(start);
        }

        if (key == "scene_name") {
            m_scene->name = value;
        }
        else if (key == "ambient") {
            m_scene->ambientColor = StringToVec3(value);
        }
        else if (key == "object") {
            // New object
            auto obj = std::make_unique<SceneObject>();
            obj->name = value;
            currentObject = obj.get();
            m_scene->objects.push_back(std::move(obj));
        }
        else if (currentObject) {
            if (key == "asset") {
                currentObject->assetPath = value;
                LoadModelIntoObject(currentObject, value);
            }
            else if (key == "position") {
                currentObject->position = StringToVec3(value);
            }
            else if (key == "rotation") {
                currentObject->rotation = StringToQuat(value);
            }
            else if (key == "scale") {
                currentObject->scale = StringToVec3(value);
            }
            else if (key == "visible") {
                currentObject->isVisible = (value == "true" || value == "1");
            }
        }
    }

    // Update all world matrices
    for (auto& obj : m_scene->objects) {
        obj->UpdateWorldMatrix();
    }

    m_scene->isDirty = false;

    // Extract scene name from filename if not set
    if (m_scene->name == "Untitled") {
        std::filesystem::path p(filePath);
        m_scene->name = p.stem().string();
    }

    return true;
}

bool SceneManager::SaveScene() {
    if (m_scene->filePath.empty()) {
        return false;  // Need to call SaveSceneAs
    }
    return SaveSceneAs(m_scene->filePath);
}

bool SceneManager::SaveSceneAs(const std::string& filePath) {
    // Update scene name from filename
    std::filesystem::path p(filePath);
    m_scene->name = p.stem().string();
    m_scene->filePath = filePath;

    std::ofstream file(filePath);
    if (!file.is_open()) {
        OutputDebugStringA(("Failed to save scene file: " + filePath + "\n").c_str());
        return false;
    }

    file << "# AniEngine Scene File\n";
    file << "scene_name: " << m_scene->name << "\n";
    file << "ambient: " << Vec3ToString(m_scene->ambientColor) << "\n";
    file << "\n";

    for (const auto& obj : m_scene->objects) {
        file << "object: " << obj->name << "\n";
        file << "  asset: " << obj->assetPath << "\n";
        file << "  position: " << Vec3ToString(obj->position) << "\n";
        file << "  rotation: " << QuatToString(obj->rotation) << "\n";
        file << "  scale: " << Vec3ToString(obj->scale) << "\n";
        file << "  visible: " << (obj->isVisible ? "true" : "false") << "\n";
        file << "\n";
    }

    // TODO: Save lights

    m_scene->isDirty = false;

    OutputDebugStringA(("Scene saved: " + filePath + "\n").c_str());
    return true;
}

bool SceneManager::AddObjectFromFile(const std::string& glTFPath) {
    return AddObjectFromFile(glTFPath, glm::vec3(0.0f));
}

bool SceneManager::AddObjectFromFile(const std::string& glTFPath, const glm::vec3& position) {
    auto object = std::make_unique<SceneObject>();

    // Generate name from filename
    std::filesystem::path p(glTFPath);
    std::string baseName = p.stem().string();
    object->name = GenerateUniqueName(baseName);
    object->assetPath = glTFPath;
    object->position = position;

    if (!LoadModelIntoObject(object.get(), glTFPath)) {
        return false;
    }

    object->UpdateWorldMatrix();

    m_scene->objects.push_back(std::move(object));
    m_scene->MarkDirty();

    // Select the newly added object
    SelectObject(static_cast<int>(m_scene->objects.size()) - 1);

    return true;
}

void SceneManager::RemoveObject(int index) {
    if (index < 0 || index >= static_cast<int>(m_scene->objects.size())) {
        return;
    }

    m_scene->objects.erase(m_scene->objects.begin() + index);
    m_scene->MarkDirty();

    // Fix selection
    if (m_scene->selectedObjectIndex == index) {
        m_scene->selectedObjectIndex = -1;
    }
    else if (m_scene->selectedObjectIndex > index) {
        m_scene->selectedObjectIndex--;
    }
}

void SceneManager::DuplicateObject(int index) {
    if (index < 0 || index >= static_cast<int>(m_scene->objects.size())) {
        return;
    }

    const auto& source = m_scene->objects[index];

    auto copy = std::make_unique<SceneObject>();
    copy->name = GenerateUniqueName(source->name);
    copy->assetPath = source->assetPath;
    copy->position = source->position + glm::vec3(1.0f, 0.0f, 0.0f);  // Offset slightly
    copy->rotation = source->rotation;
    copy->scale = source->scale;
    copy->boundsMin = source->boundsMin;
    copy->boundsMax = source->boundsMax;
    copy->isVisible = source->isVisible;

    // Share mesh/material/texture resources (they're shared_ptr)
    copy->meshes = source->meshes;
    copy->materials = source->materials;
    copy->textures = source->textures;
    copy->meshInstances = source->meshInstances;

    copy->UpdateWorldMatrix();

    m_scene->objects.push_back(std::move(copy));
    m_scene->MarkDirty();

    SelectObject(static_cast<int>(m_scene->objects.size()) - 1);
}

void SceneManager::SelectObject(int index) {
    m_scene->SelectObject(index);
}

void SceneManager::ClearSelection() {
    m_scene->ClearSelection();
}

SceneObject* SceneManager::GetSelectedObject() {
    return m_scene->GetSelectedObject();
}

int SceneManager::GetSelectedIndex() const {
    return m_scene->selectedObjectIndex;
}

bool SceneManager::HasUnsavedChanges() const {
    return m_scene && m_scene->isDirty;
}

const std::string& SceneManager::GetSceneName() const {
    static std::string empty;
    return m_scene ? m_scene->name : empty;
}

const std::string& SceneManager::GetScenePath() const {
    static std::string empty;
    return m_scene ? m_scene->filePath : empty;
}

bool SceneManager::LoadModelIntoObject(SceneObject* object, const std::string& glTFPath) {
    if (!object || !m_modelLoader) {
        return false;
    }

    // Use the existing ModelLoader to load the glTF
    LoadedModel loadedModel;
    if (!m_modelLoader->LoadGLTF(glTFPath.c_str(), m_device, loadedModel)) {
        OutputDebugStringA(("Failed to load model: " + glTFPath + "\n").c_str());
        return false;
    }

    // Transfer ownership to SceneObject
    object->meshes = std::move(loadedModel.meshes);
    object->materials = std::move(loadedModel.materials);
    object->textures = std::move(loadedModel.textures);

    // Convert mesh instances
    object->meshInstances.clear();
    for (const auto& instance : loadedModel.meshInstances) {
        SceneObject::MeshInstance mi;
        mi.mesh = instance.mesh;
        mi.localTransform = instance.transform;
        object->meshInstances.push_back(mi);
    }

    // Copy bounds
    object->boundsMin = loadedModel.boundsMin;
    object->boundsMax = loadedModel.boundsMax;

    return true;
}

std::string SceneManager::GenerateUniqueName(const std::string& baseName) {
    m_objectCounter++;

    // Check if name already exists
    std::string candidate = baseName;
    int suffix = 1;

    bool exists = true;
    while (exists) {
        exists = false;
        for (const auto& obj : m_scene->objects) {
            if (obj->name == candidate) {
                exists = true;
                candidate = baseName + "_" + std::to_string(suffix);
                suffix++;
                break;
            }
        }
    }

    return candidate;
}