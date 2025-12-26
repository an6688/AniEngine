#pragma once

#include <string>
#include <vector>
#include <chrono>

// Metadata for an imported asset
struct AssetEntry {
    std::string uuid;           // Unique identifier
    std::string name;           // Display name (filename without extension)
    std::string relativePath;   // Path relative to project assets folder
    std::string sourcePath;     // Original import path (for re-import)
    std::string type;           // "model", "texture", "scene", etc.

    // Timestamps
    int64_t importedTime = 0;   // When first imported
    int64_t modifiedTime = 0;   // Last modification

    // Model-specific metadata (optional)
    int meshCount = 0;
    int materialCount = 0;
    int textureCount = 0;
};

// Project configuration
struct Project {
    std::string name;
    std::string rootPath;           // Absolute path to project folder
    std::string version = "1.0";

    // Folders (relative to rootPath)
    std::string assetsFolder = "Assets";
    std::string scenesFolder = "Scenes";

    // Last opened scene
    std::string lastOpenedScene;

    // Asset registry
    std::vector<AssetEntry> assets;

    // Project settings
    struct Settings {
        float defaultCameraDistance = 5.0f;
        float defaultLightIntensity = 2.0f;
        float defaultAmbient = 0.1f;
        int windowWidth = 1280;
        int windowHeight = 720;
    } settings;

    // Timestamps
    int64_t createdTime = 0;
    int64_t lastOpenedTime = 0;

    // Helper methods
    std::string GetAssetsPath() const {
        return rootPath + "/" + assetsFolder;
    }

    std::string GetScenesPath() const {
        return rootPath + "/" + scenesFolder;
    }

    std::string GetProjectFilePath() const {
        return rootPath + "/project.aniproj";
    }

    // Find asset by relative path
    AssetEntry* FindAsset(const std::string& relativePath) {
        for (auto& asset : assets) {
            if (asset.relativePath == relativePath) {
                return &asset;
            }
        }
        return nullptr;
    }

    // Find asset by UUID
    AssetEntry* FindAssetByUUID(const std::string& uuid) {
        for (auto& asset : assets) {
            if (asset.uuid == uuid) {
                return &asset;
            }
        }
        return nullptr;
    }
};

// Recent project entry (for launcher)
struct RecentProject {
    std::string name;
    std::string path;
    int64_t lastOpened = 0;
};