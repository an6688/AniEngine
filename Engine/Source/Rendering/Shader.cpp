#include "Shader.h"
#include <d3dcompiler.h>
#include <cassert>

#pragma comment(lib, "d3dcompiler.lib")

Shader::Shader()
{
}

Shader::~Shader()
{
}

bool Shader::CompileFromFile(
    const wchar_t* filename,
    const char* entryPoint,
    const char* target)
{
    UINT compileFlags = 0;

#ifdef _DEBUG
    // Enable debug info and skip optimization in debug
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompileFromFile(
        filename,
        nullptr,                            // Defines
        D3D_COMPILE_STANDARD_FILE_INCLUDE,  // Include handler
        entryPoint,
        target,
        compileFlags,
        0,                                  // Effect flags
        &m_shaderBlob,
        &errorBlob
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            // Output error message
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        return false;
    }

    return true;
}

D3D12_SHADER_BYTECODE Shader::GetBytecode() const
{
    D3D12_SHADER_BYTECODE bytecode = {};

    if (m_shaderBlob)
    {
        bytecode.pShaderBytecode = m_shaderBlob->GetBufferPointer();
        bytecode.BytecodeLength = m_shaderBlob->GetBufferSize();
    }

    return bytecode;
}