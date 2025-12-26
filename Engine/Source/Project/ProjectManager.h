#pragma once

#include "Project.h"
#include <memory>
#include <functional>

class SceneManager;

// Manages project creation, loading, saving, and asset import
class ProjectManager {
public:
    ProjectManager();
    ~ProjectManager();

    // Project lifecycle
    bool CreateProject(const std::string& name, const std::string& folderPath);
    bool OpenProject(const std::string& projectFilePath);
    bool SaveProject();
    void CloseProject();

    // Asset management
    std::string ImportAsset(const std::string& sourceFilePath);  // Returns relative path
    bool RemoveAsset(const std::string& relativePath);
    bool RefreshAsset(const std::string& relativePath);  // Re-import from source

    // Scene helpers
    std::string GetFullAssetPath(const std::string& relativePath) const;
    std::string GetFullScenePath(const std::string& sceneName) const;
    std::vector<std::string> GetSceneList() const;
    std::vector<AssetEntry> GetAssetsByType(const std::string& type) const;

    // Recent projects
    std::vector<RecentProject> GetRecentProjects() const;
    void AddToRecentProjects(const std::string& projectPath);
    void ClearRecentProjects();

    // State
    bool HasOpenProject() const { return m_project != nullptr; }
    Project* GetProject() { return m_project.get(); }
    const Project* GetProject() const { return m_project.get(); }
    const std::string& GetProjectName() const;
    const std::string& GetProjectPath() const;

    // Settings persistence (stored in user's AppData)
    static std::string GetAppDataPath();

private:
    bool CreateProjectFolders();
    bool SaveProjectFile();
    bool LoadProjectFile(const std::string& projectFilePath);

    std::string GenerateUUID();
    std::string DetermineAssetType(const std::string& filePath);
    void ScanAssetsFolder();  // Rebuild asset list from disk

    void LoadRecentProjects();
    void SaveRecentProjects();

    std::unique_ptr<Project> m_project;
    std::vector<RecentProject> m_recentProjects;

    static const int MAX_RECENT_PROJECTS = 10;
};