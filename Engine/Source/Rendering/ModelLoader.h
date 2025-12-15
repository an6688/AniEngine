#pragma once

#include "Mesh.h"
#include <vector>
#include <string>
#include <memory>

// Forward declarations
class RenderDevice;

// Model loader - loads GLTF files and creates Mesh objects
// DEPENDS ON: FastGLTF, RenderDevice
class ModelLoader
{
public:
    ModelLoader();
    ~ModelLoader();

    // Load a GLTF file and create meshes
    // Returns vector of meshes (one per primitive in the GLTF)
    bool LoadGLTF(
        const std::string& filepath,
        RenderDevice* device,
        std::vector<std::unique_ptr<Mesh>>& outMeshes
    );

private:
    // Helper to convert GLTF data to our vertex format
    void ProcessMesh(
        const void* gltfAsset,
        size_t meshIndex,
        size_t primitiveIndex,
        RenderDevice* device,
        std::vector<std::unique_ptr<Mesh>>& outMeshes
    );
};