#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>

// Shader compilation and management
// DEPENDS ON: D3D12 device, passed as parameter
class Shader
{
public:
    Shader();
    ~Shader();

    // Compile shader from file
    bool CompileFromFile(
        const wchar_t* filename,
        const char* entryPoint,
        const char* target
    );

    // Get compiled bytecode for PSO creation
    D3D12_SHADER_BYTECODE GetBytecode() const;

    // Check if shader is valid
    bool IsValid() const { return m_shaderBlob.Get() != nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3DBlob> m_shaderBlob;
};