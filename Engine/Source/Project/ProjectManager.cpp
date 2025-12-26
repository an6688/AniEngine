#include "ProjectManager.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <random>
#include <chrono>
#include <ShlObj.h>  // For SHGetFolderPathW

namespace fs = std::filesystem;

namespace {
    int64_t GetCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    // Escape string for saving (handle special chars)
    std::string EscapeString(const std::string& s) {
        std::string result;
        for (char c : s) {
            if (c == '\\') result += "\\\\";
            else if (c == '"') result += "\\\"";
            else result += c;
        }
        return result;
    }

    // Unescape string when loading
    std::string UnescapeString(const std::string& s) {
        std::string result;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                if (s[i + 1] == '\\') { result += '\\'; i++; }
                else if (s[i + 1] == '"') { result += '"'; i++; }
                else result += s[i];
            }
            else {
                result += s[i];
            }
        }
        return result;
    }

} // anonymous namespace

ProjectManager::ProjectManager() {
    LoadRecentProjects();
}

ProjectManager::~ProjectManager() {
    SaveRecentProjects();
}

bool ProjectManager::CreateProject(const std::string& name, const std::string& folderPath) {
    // Create project folder
    std::string projectRoot = folderPath + "/" + name;

    try {
        if (fs::exists(projectRoot)) {
            OutputDebugStringA(("Project folder already exists: " + projectRoot + "\n").c_str());
            return false;
        }

        fs::create_directories(projectRoot);
    }
    catch (const fs::filesystem_error& e) {
        OutputDebugStringA(("Failed to create project folder: " + std::string(e.what()) + "\n").c_str());
        return false;
    }

    // Initialize project
    m_project = std::make_unique<Project>();
    m_project->name = name;
    m_project->rootPath = projectRoot;
    m_project->createdTime = GetCurrentTimestamp();
    m_project->lastOpenedTime = m_project->createdTime;

    // Create subfolders
    if (!CreateProjectFolders()) {
        m_project.reset();
        return false;
    }

    // Save project file
    if (!SaveProjectFile()) {
        m_project.reset();
        return false;
    }

    // Add to recent projects
    AddToRecentProjects(m_project->GetProjectFilePath());

    OutputDebugStringA(("Project created: " + projectRoot + "\n").c_str());
    return true;
}

bool ProjectManager::OpenProject(const std::string& projectFilePath) {
    if (!LoadProjectFile(projectFilePath)) {
        return false;
    }

    // Update last opened time
    m_project->lastOpenedTime = GetCurrentTimestamp();
    SaveProjectFile();

    AddToRecentProjects(projectFilePath);

    // Scan assets folder to catch any manually added files
    ScanAssetsFolder();

    OutputDebugStringA(("Project opened: " + m_project->rootPath + "\n").c_str());
    return true;
}

bool ProjectManager::SaveProject() {
    if (!m_project) {
        return false;
    }
    return SaveProjectFile();
}

void ProjectManager::CloseProject() {
    if (m_project) {
        SaveProjectFile();
        m_project.reset();
    }
}

std::string ProjectManager::ImportAsset(const std::string& sourceFilePath) {
    if (!m_project) {
        OutputDebugStringA("No project open\n");
        return "";
    }

    fs::path sourcePath(sourceFilePath);
    if (!fs::exists(sourcePath)) {
        OutputDebugStringA(("Source file not found: " + sourceFilePath + "\n").c_str());
        return "";
    }

    // Determine asset type and destination folder
    std::string assetType = DetermineAssetType(sourceFilePath);
    std::string destFolder = m_project->GetAssetsPath();

    // Create subfolder based on type
    if (assetType == "model") {
        destFolder += "/Models";
    }
    else if (assetType == "texture") {
        destFolder += "/Textures";
    }

    try {
        fs::create_directories(destFolder);
    }
    catch (...) {}

    // Copy file to project
    std::string filename = sourcePath.filename().string();
    std::string destPath = destFolder + "/" + filename;

    // Handle duplicate names by creating a unique subfolder
    std::string stem = sourcePath.stem().string();
    std::string uniqueFolder = destFolder + "/" + stem;
    int counter = 1;
    while (fs::exists(uniqueFolder)) {
        uniqueFolder = destFolder + "/" + stem + "_" + std::to_string(counter);
        counter++;
    }

    try {
        fs::create_directories(uniqueFolder);

        // Copy the main file
        std::string destFile = uniqueFolder + "/" + filename;
        fs::copy_file(sourcePath, destFile, fs::copy_options::overwrite_existing);
        OutputDebugStringA(("Copied: " + sourcePath.string() + " -> " + destFile + "\n").c_str());

        // For glTF files, copy all files in the same directory with same stem
        // to catch .bin files and embedded textures
        if (assetType == "model") {
            fs::path sourceDir = sourcePath.parent_path();
            std::string baseStem = sourcePath.stem().string();

            // Copy .bin file if exists (same name as .gltf)
            fs::path binPath = sourceDir / (baseStem + ".bin");
            if (fs::exists(binPath)) {
                std::string destBin = uniqueFolder + "/" + baseStem + ".bin";
                fs::copy_file(binPath, destBin, fs::copy_options::overwrite_existing);
                OutputDebugStringA(("Copied .bin: " + binPath.string() + "\n").c_str());
            }

            // Copy common texture patterns
            std::vector<std::string> texturePatterns = {
                "_baseColor", "_normal", "_metallicRoughness", "_occlusion", "_emissive",
                "_diffuse", "_albedo", "_roughness", "_metallic", "_ao",
                // Also try without underscore
                "baseColor", "normal", "metallicRoughness", "occlusion", "emissive"
            };

            std::vector<std::string> imageExtensions = { ".png", ".jpg", ".jpeg", ".tga", ".bmp" };

            for (const auto& pattern : texturePatterns) {
                for (const auto& ext : imageExtensions) {
                    fs::path texPath = sourceDir / (baseStem + pattern + ext);
                    if (fs::exists(texPath)) {
                        std::string destTex = uniqueFolder + "/" + texPath.filename().string();
                        fs::copy_file(texPath, destTex, fs::copy_options::skip_existing);
                        OutputDebugStringA(("Copied texture: " + texPath.filename().string() + "\n").c_str());
                    }
                }
            }

            // Also scan for any image files in the source directory that might be referenced
            try {
                for (const auto& entry : fs::directory_iterator(sourceDir)) {
                    if (!entry.is_regular_file()) continue;

                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    // Copy image files
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") {
                        std::string destTex = uniqueFolder + "/" + entry.path().filename().string();
                        if (!fs::exists(destTex)) {
                            fs::copy_file(entry.path(), destTex, fs::copy_options::skip_existing);
                            OutputDebugStringA(("Copied image: " + entry.path().filename().string() + "\n").c_str());
                        }
                    }

                    // Also copy any .bin files in the directory
                    if (ext == ".bin") {
                        std::string destBin = uniqueFolder + "/" + entry.path().filename().string();
                        if (!fs::exists(destBin)) {
                            fs::copy_file(entry.path(), destBin, fs::copy_options::skip_existing);
                            OutputDebugStringA(("Copied bin: " + entry.path().filename().string() + "\n").c_str());
                        }
                    }
                }
            }
            catch (const std::exception& e) {
                OutputDebugStringA(("Warning scanning source dir: " + std::string(e.what()) + "\n").c_str());
            }
        }

        // Create asset entry with path relative to project root
        AssetEntry entry;
        entry.uuid = GenerateUUID();
        entry.name = stem;
        entry.relativePath = fs::relative(destFile, m_project->rootPath).string();
        std::replace(entry.relativePath.begin(), entry.relativePath.end(), '\\', '/');
        entry.sourcePath = sourceFilePath;
        entry.type = assetType;
        entry.importedTime = GetCurrentTimestamp();
        entry.modifiedTime = entry.importedTime;

        m_project->assets.push_back(entry);
        SaveProjectFile();

        OutputDebugStringA(("Asset imported successfully: " + entry.relativePath + "\n").c_str());
        return entry.relativePath;

    }
    catch (const fs::filesystem_error& e) {
        OutputDebugStringA(("Failed to copy asset: " + std::string(e.what()) + "\n").c_str());
        return "";
    }
}

bool ProjectManager::RemoveAsset(const std::string& relativePath) {
    if (!m_project) {
        return false;
    }

    // Find and remove from registry
    auto it = std::find_if(m_project->assets.begin(), m_project->assets.end(),
        [&](const AssetEntry& e) { return e.relativePath == relativePath; });

    if (it == m_project->assets.end()) {
        return false;
    }

    // Delete file from disk
    std::string fullPath = m_project->rootPath + "/" + relativePath;
    try {
        fs::remove(fullPath);
    }
    catch (...) {
        // File might already be gone
    }

    m_project->assets.erase(it);
    SaveProjectFile();

    return true;
}

bool ProjectManager::RefreshAsset(const std::string& relativePath) {
    if (!m_project) {
        return false;
    }

    AssetEntry* entry = m_project->FindAsset(relativePath);
    if (!entry || entry->sourcePath.empty()) {
        return false;
    }

    // Recopy from source
    std::string destPath = m_project->rootPath + "/" + relativePath;
    try {
        fs::copy_file(entry->sourcePath, destPath, fs::copy_options::overwrite_existing);
        entry->modifiedTime = GetCurrentTimestamp();
        SaveProjectFile();
        return true;
    }
    catch (...) {
        return false;
    }
}

std::string ProjectManager::GetFullAssetPath(const std::string& relativePath) const {
    if (!m_project) {
        return relativePath;
    }
    return m_project->rootPath + "/" + relativePath;
}

std::string ProjectManager::GetFullScenePath(const std::string& sceneName) const {
    if (!m_project) {
        return sceneName;
    }
    return m_project->GetScenesPath() + "/" + sceneName + ".scene";
}

std::vector<std::string> ProjectManager::GetSceneList() const {
    std::vector<std::string> scenes;
    if (!m_project) {
        return scenes;
    }

    std::string scenesPath = m_project->GetScenesPath();
    try {
        for (const auto& entry : fs::directory_iterator(scenesPath)) {
            if (entry.path().extension() == ".scene") {
                scenes.push_back(entry.path().stem().string());
            }
        }
    }
    catch (...) {}

    return scenes;
}

std::vector<AssetEntry> ProjectManager::GetAssetsByType(const std::string& type) const {
    std::vector<AssetEntry> result;
    if (!m_project) {
        return result;
    }

    for (const auto& asset : m_project->assets) {
        if (asset.type == type) {
            result.push_back(asset);
        }
    }
    return result;
}

std::vector<RecentProject> ProjectManager::GetRecentProjects() const {
    return m_recentProjects;
}

void ProjectManager::AddToRecentProjects(const std::string& projectPath) {
    // Remove if already exists
    m_recentProjects.erase(
        std::remove_if(m_recentProjects.begin(), m_recentProjects.end(),
            [&](const RecentProject& rp) { return rp.path == projectPath; }),
        m_recentProjects.end()
    );

    // Add to front
    RecentProject rp;
    rp.path = projectPath;
    rp.lastOpened = GetCurrentTimestamp();

    // Extract name from path
    fs::path p(projectPath);
    rp.name = p.parent_path().filename().string();

    m_recentProjects.insert(m_recentProjects.begin(), rp);

    // Trim to max size
    if (m_recentProjects.size() > MAX_RECENT_PROJECTS) {
        m_recentProjects.resize(MAX_RECENT_PROJECTS);
    }

    SaveRecentProjects();
}

void ProjectManager::ClearRecentProjects() {
    m_recentProjects.clear();
    SaveRecentProjects();
}

const std::string& ProjectManager::GetProjectName() const {
    static std::string empty;
    return m_project ? m_project->name : empty;
}

const std::string& ProjectManager::GetProjectPath() const {
    static std::string empty;
    return m_project ? m_project->rootPath : empty;
}

std::string ProjectManager::GetAppDataPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        char narrowPath[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, path, -1, narrowPath, MAX_PATH, NULL, NULL);
        return std::string(narrowPath) + "/AniEngine";
    }
    return "./AniEngine";
}

bool ProjectManager::CreateProjectFolders() {
    if (!m_project) {
        return false;
    }

    try {
        fs::create_directories(m_project->GetAssetsPath());
        fs::create_directories(m_project->GetAssetsPath() + "/Models");
        fs::create_directories(m_project->GetAssetsPath() + "/Textures");
        fs::create_directories(m_project->GetScenesPath());
        return true;
    }
    catch (const fs::filesystem_error& e) {
        OutputDebugStringA(("Failed to create project folders: " + std::string(e.what()) + "\n").c_str());
        return false;
    }
}

bool ProjectManager::SaveProjectFile() {
    if (!m_project) {
        return false;
    }

    std::string projectFile = m_project->GetProjectFilePath();
    std::ofstream file(projectFile);
    if (!file.is_open()) {
        OutputDebugStringA(("Failed to save project file: " + projectFile + "\n").c_str());
        return false;
    }

    // Write project header
    file << "# AniEngine Project File\n";
    file << "version: " << m_project->version << "\n";
    file << "name: " << m_project->name << "\n";
    file << "created: " << m_project->createdTime << "\n";
    file << "lastOpened: " << m_project->lastOpenedTime << "\n";
    file << "lastScene: " << m_project->lastOpenedScene << "\n";
    file << "\n";

    // Write settings
    file << "[settings]\n";
    file << "cameraDistance: " << m_project->settings.defaultCameraDistance << "\n";
    file << "lightIntensity: " << m_project->settings.defaultLightIntensity << "\n";
    file << "ambient: " << m_project->settings.defaultAmbient << "\n";
    file << "windowWidth: " << m_project->settings.windowWidth << "\n";
    file << "windowHeight: " << m_project->settings.windowHeight << "\n";
    file << "\n";

    // Write assets
    file << "[assets]\n";
    for (const auto& asset : m_project->assets) {
        file << "asset: " << asset.uuid << "\n";
        file << "  name: " << asset.name << "\n";
        file << "  path: " << asset.relativePath << "\n";
        file << "  source: " << EscapeString(asset.sourcePath) << "\n";
        file << "  type: " << asset.type << "\n";
        file << "  imported: " << asset.importedTime << "\n";
        file << "  modified: " << asset.modifiedTime << "\n";
        file << "\n";
    }

    return true;
}

bool ProjectManager::LoadProjectFile(const std::string& projectFilePath) {
    std::ifstream file(projectFilePath);
    if (!file.is_open()) {
        OutputDebugStringA(("Failed to open project file: " + projectFilePath + "\n").c_str());
        return false;
    }

    m_project = std::make_unique<Project>();
    m_project->rootPath = fs::path(projectFilePath).parent_path().string();

    std::string line;
    std::string currentSection;
    AssetEntry* currentAsset = nullptr;

    while (std::getline(file, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        if (line.empty() || line[0] == '#') continue;

        // Section headers
        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                currentSection = line.substr(1, end - 1);
            }
            continue;
        }

        // Parse key: value
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        // Trim value
        start = value.find_first_not_of(" \t");
        if (start != std::string::npos) {
            value = value.substr(start);
        }

        // Parse based on section
        if (currentSection.empty()) {
            // Header section
            if (key == "version") m_project->version = value;
            else if (key == "name") m_project->name = value;
            else if (key == "created") m_project->createdTime = std::stoll(value);
            else if (key == "lastOpened") m_project->lastOpenedTime = std::stoll(value);
            else if (key == "lastScene") m_project->lastOpenedScene = value;
        }
        else if (currentSection == "settings") {
            if (key == "cameraDistance") m_project->settings.defaultCameraDistance = std::stof(value);
            else if (key == "lightIntensity") m_project->settings.defaultLightIntensity = std::stof(value);
            else if (key == "ambient") m_project->settings.defaultAmbient = std::stof(value);
            else if (key == "windowWidth") m_project->settings.windowWidth = std::stoi(value);
            else if (key == "windowHeight") m_project->settings.windowHeight = std::stoi(value);
        }
        else if (currentSection == "assets") {
            if (key == "asset") {
                m_project->assets.push_back(AssetEntry());
                currentAsset = &m_project->assets.back();
                currentAsset->uuid = value;
            }
            else if (currentAsset) {
                if (key == "name") currentAsset->name = value;
                else if (key == "path") currentAsset->relativePath = value;
                else if (key == "source") currentAsset->sourcePath = UnescapeString(value);
                else if (key == "type") currentAsset->type = value;
                else if (key == "imported") currentAsset->importedTime = std::stoll(value);
                else if (key == "modified") currentAsset->modifiedTime = std::stoll(value);
            }
        }
    }

    return true;
}

std::string ProjectManager::GenerateUUID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::stringstream ss;
    ss << std::hex << dis(gen) << dis(gen);
    return ss.str().substr(0, 32);
}

std::string ProjectManager::DetermineAssetType(const std::string& filePath) {
    fs::path p(filePath);
    std::string ext = p.extension().string();

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx") {
        return "model";
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr") {
        return "texture";
    }
    if (ext == ".scene") {
        return "scene";
    }
    if (ext == ".hlsl" || ext == ".glsl") {
        return "shader";
    }

    return "other";
}

void ProjectManager::ScanAssetsFolder() {
    if (!m_project) return;

    std::string assetsPath = m_project->GetAssetsPath();

    try {
        for (const auto& entry : fs::recursive_directory_iterator(assetsPath)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string relativePath = fs::relative(entry.path(), m_project->rootPath).string();
            std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

            // Skip if already registered
            if (m_project->FindAsset(relativePath)) {
                continue;
            }

            // Add new asset
            std::string type = DetermineAssetType(entry.path().string());
            if (type == "other") {
                continue;  // Skip unknown types
            }

            AssetEntry newEntry;
            newEntry.uuid = GenerateUUID();
            newEntry.name = entry.path().stem().string();
            newEntry.relativePath = relativePath;
            newEntry.type = type;
            newEntry.importedTime = GetCurrentTimestamp();
            newEntry.modifiedTime = newEntry.importedTime;

            m_project->assets.push_back(newEntry);
        }
    }
    catch (...) {}

    SaveProjectFile();
}

void ProjectManager::LoadRecentProjects() {
    std::string appData = GetAppDataPath();
    std::string recentFile = appData + "/recent_projects.txt";

    std::ifstream file(recentFile);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        // Format: name|path|timestamp
        size_t pos1 = line.find('|');
        size_t pos2 = line.rfind('|');

        if (pos1 != std::string::npos && pos2 != std::string::npos && pos1 != pos2) {
            RecentProject rp;
            rp.name = line.substr(0, pos1);
            rp.path = line.substr(pos1 + 1, pos2 - pos1 - 1);
            rp.lastOpened = std::stoll(line.substr(pos2 + 1));
            m_recentProjects.push_back(rp);
        }
    }
}

void ProjectManager::SaveRecentProjects() {
    std::string appData = GetAppDataPath();

    try {
        fs::create_directories(appData);
    }
    catch (...) {}

    std::string recentFile = appData + "/recent_projects.txt";
    std::ofstream file(recentFile);
    if (!file.is_open()) {
        return;
    }

    for (const auto& rp : m_recentProjects) {
        file << rp.name << "|" << rp.path << "|" << rp.lastOpened << "\n";
    }
}