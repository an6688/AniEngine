#include "ProjectDialog.h"
#include "ProjectManager.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>   // For OPENFILENAME, GetOpenFileNameA
#include <ShlObj.h>    // For SHBrowseForFolder, SHGetFolderPath

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#include <imgui.h>
#include <cstring>
#include <ctime>

ProjectDialog::ProjectDialog() {
    Reset();
}

ProjectDialog::~ProjectDialog() {
}

void ProjectDialog::Reset() {
    m_currentTab = 0;
    memset(m_projectNameBuffer, 0, sizeof(m_projectNameBuffer));
    memset(m_projectFolderBuffer, 0, sizeof(m_projectFolderBuffer));
    m_selectedPath.clear();
    m_newProjectName.clear();
    m_newProjectFolder.clear();
    m_result = ProjectDialogResult::None;
    m_isOpen = true;

    // Set default folder to Documents
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS, NULL, 0, path))) {
        char narrowPath[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, path, -1, narrowPath, MAX_PATH, NULL, NULL);
        strncpy_s(m_projectFolderBuffer, narrowPath, sizeof(m_projectFolderBuffer) - 1);
    }
}

ProjectDialogResult ProjectDialog::Show(ProjectManager* projectManager) {
    if (!m_isOpen) {
        return m_result;
    }

    // Center the dialog
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_Appearing);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin("AniEngine - Project", &m_isOpen, flags)) {

        // Title
        ImGui::PushFont(nullptr);  // Could use a larger font here
        ImGui::Text("Welcome to AniEngine");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();

        // Tabs
        if (ImGui::BeginTabBar("ProjectTabs")) {
            if (ImGui::BeginTabItem("Recent")) {
                m_currentTab = 0;
                DrawRecentTab(projectManager);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Open")) {
                m_currentTab = 1;
                DrawOpenTab(projectManager);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Create New")) {
                m_currentTab = 2;
                DrawCreateTab(projectManager);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    // Handle close button
    if (!m_isOpen && m_result == ProjectDialogResult::None) {
        m_result = ProjectDialogResult::Cancelled;
    }

    return m_result;
}

void ProjectDialog::DrawRecentTab(ProjectManager* projectManager) {
    ImGui::Spacing();

    auto recentProjects = projectManager->GetRecentProjects();

    if (recentProjects.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No recent projects");
        ImGui::Spacing();
        ImGui::Text("Create a new project or open an existing one.");
    }
    else {
        ImGui::Text("Select a recent project:");
        ImGui::Spacing();

        // Project list
        ImGui::BeginChild("RecentList", ImVec2(0, -60), true);

        for (size_t i = 0; i < recentProjects.size(); i++) {
            const auto& rp = recentProjects[i];

            ImGui::PushID(static_cast<int>(i));

            // Format timestamp
            char timeStr[64] = "";
            time_t time = static_cast<time_t>(rp.lastOpened);
            struct tm tm;
            localtime_s(&tm, &time);
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", &tm);

            // Selectable row
            bool selected = (m_selectedPath == rp.path);
            if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 40))) {
                m_selectedPath = rp.path;

                if (ImGui::IsMouseDoubleClicked(0)) {
                    m_result = ProjectDialogResult::OpenRecent;
                    m_isOpen = false;
                }
            }

            // Draw content on top of selectable
            ImGui::SameLine(10);
            ImGui::BeginGroup();
            ImGui::Text("%s", rp.name.c_str());
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", rp.path.c_str());
            ImGui::EndGroup();

            ImGui::SameLine(ImGui::GetWindowWidth() - 150);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", timeStr);

            ImGui::PopID();
        }

        ImGui::EndChild();

        // Buttons
        ImGui::Spacing();

        bool canOpen = !m_selectedPath.empty();
        if (!canOpen) ImGui::BeginDisabled();
        if (ImGui::Button("Open Selected", ImVec2(120, 0))) {
            m_result = ProjectDialogResult::OpenRecent;
            m_isOpen = false;
        }
        if (!canOpen) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Clear Recent", ImVec2(120, 0))) {
            projectManager->ClearRecentProjects();
        }
    }
}

void ProjectDialog::DrawOpenTab(ProjectManager* projectManager) {
    ImGui::Spacing();
    ImGui::Text("Open an existing project:");
    ImGui::Spacing();

    ImGui::TextWrapped("Browse to a project folder and select the .aniproj file.");
    ImGui::Spacing();
    ImGui::Spacing();

    // Browse button
    if (ImGui::Button("Browse...", ImVec2(120, 40))) {
        BrowseForProject();
    }

    if (!m_selectedPath.empty()) {
        ImGui::Spacing();
        ImGui::Text("Selected:");
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "%s", m_selectedPath.c_str());

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::Button("Open Project", ImVec2(120, 0))) {
            m_result = ProjectDialogResult::OpenExisting;
            m_isOpen = false;
        }
    }
}

void ProjectDialog::DrawCreateTab(ProjectManager* projectManager) {
    ImGui::Spacing();
    ImGui::Text("Create a new project:");
    ImGui::Spacing();

    // Project name
    ImGui::Text("Project Name:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##ProjectName", m_projectNameBuffer, sizeof(m_projectNameBuffer));

    ImGui::Spacing();

    // Location
    ImGui::Text("Location:");
    ImGui::SetNextItemWidth(-80);
    ImGui::InputText("##ProjectFolder", m_projectFolderBuffer, sizeof(m_projectFolderBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Browse", ImVec2(70, 0))) {
        BrowseForFolder();
    }

    ImGui::Spacing();

    // Preview path
    if (strlen(m_projectNameBuffer) > 0 && strlen(m_projectFolderBuffer) > 0) {
        std::string fullPath = std::string(m_projectFolderBuffer) + "/" + m_projectNameBuffer;
        ImGui::Text("Project will be created at:");
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "%s", fullPath.c_str());
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Create button
    bool canCreate = strlen(m_projectNameBuffer) > 0 && strlen(m_projectFolderBuffer) > 0;
    if (!canCreate) ImGui::BeginDisabled();

    if (ImGui::Button("Create Project", ImVec2(120, 0))) {
        m_newProjectName = m_projectNameBuffer;
        m_newProjectFolder = m_projectFolderBuffer;
        m_result = ProjectDialogResult::CreateNew;
        m_isOpen = false;
    }

    if (!canCreate) ImGui::EndDisabled();

    // Help text
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
        "A new folder will be created with the project name.\n"
        "The project will include Assets and Scenes folders.");
}

void ProjectDialog::BrowseForFolder() {
    BROWSEINFOA bi = {};
    bi.hwndOwner = m_hwnd;
    bi.lpszTitle = "Select Project Location";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            strncpy_s(m_projectFolderBuffer, path, sizeof(m_projectFolderBuffer) - 1);
        }
        CoTaskMemFree(pidl);
    }
}

void ProjectDialog::BrowseForProject() {
    OPENFILENAMEA ofn = {};
    char filename[MAX_PATH] = "";

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = "AniEngine Project (*.aniproj)\0*.aniproj\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Open Project";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        m_selectedPath = filename;
    }
}