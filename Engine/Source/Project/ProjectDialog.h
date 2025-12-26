#pragma once

#include <string>
#include <vector>
#include <functional>

class ProjectManager;

struct HWND__;
typedef HWND__* HWND;

// Result of the project dialog
enum class ProjectDialogResult {
    None,           // Dialog still open
    CreateNew,      // User wants to create new project
    OpenExisting,   // User selected existing project
    OpenRecent,     // User selected from recent list
    Cancelled       // User closed dialog
};

// Handles the project selection/creation UI
// Can be shown as a startup dialog or via File menu
class ProjectDialog {
public:
    ProjectDialog();
    ~ProjectDialog();

    // Show the dialog (call each frame until result != None)
    ProjectDialogResult Show(ProjectManager* projectManager);

    // Get results after dialog closes
    const std::string& GetSelectedProjectPath() const { return m_selectedPath; }
    const std::string& GetNewProjectName() const { return m_newProjectName; }
    const std::string& GetNewProjectFolder() const { return m_newProjectFolder; }

    // Reset for next use
    void Reset();

    // Set HWND for file dialogs
    void SetHWND(HWND hwnd) { m_hwnd = hwnd; }

private:
    void DrawCreateTab(ProjectManager* projectManager);
    void DrawOpenTab(ProjectManager* projectManager);
    void DrawRecentTab(ProjectManager* projectManager);

    void BrowseForFolder();
    void BrowseForProject();

    // State
    int m_currentTab = 0;
    char m_projectNameBuffer[256] = "";
    char m_projectFolderBuffer[512] = "";

    std::string m_selectedPath;
    std::string m_newProjectName;
    std::string m_newProjectFolder;

    ProjectDialogResult m_result = ProjectDialogResult::None;
    bool m_isOpen = true;

    HWND m_hwnd = nullptr;
};