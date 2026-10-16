#include "Scene/SceneManager.h"
#include "Project/ProjectManager.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs = std::filesystem;
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
 try {
    auto root = fs::current_path() / ("persistence-test-" + std::to_string(GetCurrentProcessId()));
    fs::create_directories(root / "appdata");
    ProjectManager project(false);
    Require(project.CreateProject("original", root.string()), "create project");
    SceneManager scene;
    scene.NewScene();
    scene.GetScene()->lights[0].intensity = 3.5f;
    auto saved = project.GetFullScenePath("avocado");
    Require(scene.SaveSceneAs(saved), "save scene");
    Require(project.GetSceneList().size() == 1, "scene is discoverable");
    project.GetProject()->lastOpenedScene = "avocado";
    Require(project.SaveProject(), "remember scene");
    Require(project.OpenProject(project.GetProject()->GetProjectFilePath()), "reopen project");
    Require(scene.LoadScene(project.GetFullScenePath(project.GetProject()->lastOpenedScene)), "restore last scene");
    Require(scene.GetScene()->lights[0].intensity == 3.5f, "light round trip");
    scene.GetScene()->MarkDirty();
    Require(scene.SaveSceneAs((root / "export.scene").string(), false), "export");
    Require(scene.GetScenePath() == saved && scene.HasUnsavedChanges(), "export preserves identity and dirty state");
    Require(!scene.SaveSceneAs((root / "missing" / "bad.scene").string()), "failed save");
    Require(scene.GetScenePath() == saved && scene.HasUnsavedChanges(), "failed save preserves state");
    std::ofstream(root / "invalid.scene") << "light: bad\n  intensity: invalid\n";
    Require(!scene.LoadScene((root / "invalid.scene").string()), "invalid scene rejected");
    Require(scene.GetScenePath() == saved && scene.GetScene()->lights[0].intensity == 3.5f, "failed load preserves scene");
    auto asset = root / "original" / "Assets" / "model.gltf";
    std::ofstream(asset) << "{}";
    auto object = std::make_unique<SceneObject>();
    object->assetPath = asset.string();
    scene.GetScene()->objects.push_back(std::move(object));
    Require(scene.SaveScene(), "save relative reference");
    std::ifstream input(saved);
    std::string text((std::istreambuf_iterator<char>(input)), {});
    Require(text.find("asset: ../Assets/model.gltf") != std::string::npos, "asset reference is relative to scene");
    input.close();
    scene.GetScene()->objects.clear();
    Require(scene.SaveScene(), "save portable scene");
    fs::copy(root / "original", root / "relocated", fs::copy_options::recursive);
    Require(project.OpenProject((root / "relocated" / "project.aniproj").string()), "open relocated project");
    Require(scene.LoadScene(project.GetFullScenePath(project.GetProject()->lastOpenedScene)), "restore relocated scene");
    std::cout << "PASS: discovery, reopen, relative asset references, relocation, export, and failure handling\n";
    return 0;
 } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
