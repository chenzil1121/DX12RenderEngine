#pragma once

// 禁用VS中max和min的宏,得放最前面
#define NOMINMAX
#include<Windows.h>

#pragma warning(disable:4201) // nonstandard extension used : nameless struct/union
#pragma warning(disable:4238) // nonstandard extension used : class rvalue used as lvalue
#pragma warning(disable:4239) // A non-const reference may only be bound to an lvalue; assignment operator takes a reference to non-const
#pragma warning(disable:4324) // structure was padded due to __declspec(align())

#include<wrl/client.h>
#include<iostream>
#include<fstream>
#include<comdef.h>
#include<stdint.h>
#include<initguid.h>
#include<d3d12.h>
#include<DirectXColors.h>
#include<dxgi.h>
#include<D3Dcompiler.h>
#include<vector>
#include<unordered_map>
#include<array>
#include<queue>
#include<unordered_set>
#include<string>
#include<sstream>
#include<locale>
#include<codecvt>
#include<memory>
#include<DirectXMath.h>
#include<DirectXPackedVector.h>
#include<DirectXColors.h>
#include<dxgi1_6.h>
#ifdef _DEBUG
#include<dxgidebug.h>
#endif
#include"d3dx12.h"
#include"../ThirdParty/DXC/dxcapi.use.h"

#pragma comment(lib, "d3d12.lib") 
#pragma comment(lib, "dxgi.lib") 
#pragma comment(lib,"d3dcompiler.lib")


#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

namespace Utility
{
    static dxc::DxcDllSupport g_DxcDllHelper;

    uint32_t const CPUDescriptorHeapSize[4]
    {
        8192,  // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        2048,  // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
        1024,  // D3D12_DESCRIPTOR_HEAP_TYPE_RTV
        1024   // D3D12_DESCRIPTOR_HEAP_TYPE_DSV
    };

    uint32_t const GPUDescriptorHeapSize[2]
    {
        16384, // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        1024   // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
    };

    uint32_t const GPUDescriptorHeapDynamicSize[2]
    {
        8192,  // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        1024   // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
    };

    uint32_t const DynamicDescriptorAllocationChunkSize[2]
    {
        256,  // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        32    // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
    };

    uint32_t const DynamicUploadHeapSize = 1 << 20;

    // Align to a certain value of power of 2.
    inline constexpr UINT Align(UINT size, UINT alignment)
    {
        return (size + (alignment - 1)) & ~(alignment - 1);
    }

    inline std::string WstringToString(std::wstring ws)
    {
        //C++17需要自己实现，因为弃用了codecvt类模板
        static std::wstring_convert<std::codecvt_utf8<wchar_t>>conv;
        return conv.to_bytes(ws);
    }

    inline std::unique_ptr<char[]> WcharToChar(const wchar_t* wc)
    {
        size_t len = wcslen(wc) + 1;
        char* c = new char[len];
        size_t converted = 0;
        wcstombs_s(&converted, c, len, wc, _TRUNCATE);
        return std::unique_ptr<char[]>(c);
    }

    inline Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
        const std::wstring& filename,
        const D3D_SHADER_MACRO* defines,
        const std::string& entrypoint,
        const std::string& target)
    {
        UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
        //compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL1 | D3DCOMPILE_ENABLE_STRICTNESS/*| D3DCOMPILE_SKIP_OPTIMIZATION */;
        compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION ;
#endif

        HRESULT hr = S_OK;

        Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> errors;
        hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

        if (errors != nullptr)
            OutputDebugStringA((char*)errors->GetBufferPointer());

        return byteCode;
    }

    inline Microsoft::WRL::ComPtr<IDxcBlob> CompileLibrary(
        const WCHAR* filename,
        const WCHAR* target
    )
    {
        // Initialize the helper
        Microsoft::WRL::ComPtr<IDxcCompiler>pCompiler;
        Microsoft::WRL::ComPtr<IDxcLibrary>pLibrary;
        Microsoft::WRL::ComPtr<IDxcBlobEncoding>pTextBlob; 
        Microsoft::WRL::ComPtr<IDxcIncludeHandler>pIncludeHander;
        Microsoft::WRL::ComPtr<IDxcOperationResult>pResult;
        if (FAILED(g_DxcDllHelper.InitializeForDll(L"../ThirdParty/DXC/dxcompiler.dll", L"../ThirdParty/DXC/dxil.dll", "DxcCreateInstance")))
        {
            OutputDebugStringA("DXC Init Fail\n");
            __debugbreak();
        }
        if (FAILED(g_DxcDllHelper.CreateInstance(CLSID_DxcCompiler, pCompiler.GetAddressOf())))
        {
            OutputDebugStringA("Create IDxcCompiler Fail\n");
            __debugbreak();
        }
        if (FAILED(g_DxcDllHelper.CreateInstance(CLSID_DxcLibrary, pLibrary.GetAddressOf())))
        {
            OutputDebugStringA("Create IDxcLibrary Fail\n");
            __debugbreak();
        }

        // Open and read the file
        std::ifstream shaderFile(filename);
        if (shaderFile.good() == false)
        {
            OutputDebugStringW(std::wstring(L"Can't open file " + std::wstring(filename) + L"\n").c_str());
            __debugbreak();
            return nullptr;
        }
        std::stringstream strStream;
        strStream << shaderFile.rdbuf();
        std::string shader = strStream.str();

        // Create blob from the string
        if(FAILED(pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)shader.c_str(), (uint32_t)shader.size(), 0, pTextBlob.GetAddressOf())))
        {
            OutputDebugStringA("Create IDxcBlobEncoding Fail\n");
            __debugbreak();
        }

        if (FAILED(pLibrary->CreateIncludeHandler(pIncludeHander.GetAddressOf())))
        {
            OutputDebugStringA("Create IDxcIncludeHandler Fail\n");
            __debugbreak();
        }

        // Compile
        if (FAILED(pCompiler->Compile(pTextBlob.Get(), filename, L"", target, nullptr, 0, nullptr, 0, pIncludeHander.Get(), pResult.GetAddressOf())))
        {
            OutputDebugStringA("Compile Fail\n");
        }

        // Verify the result
        HRESULT resultCode;
        pResult->GetStatus(&resultCode);
        if (FAILED(resultCode))
        {
            Microsoft::WRL::ComPtr<IDxcBlobEncoding> pError;
            pResult->GetErrorBuffer(pError.GetAddressOf());

            std::vector<char> infoLog(pError->GetBufferSize() + 1);
            memcpy(infoLog.data(), pError->GetBufferPointer(), pError->GetBufferSize());
            infoLog[pError->GetBufferSize()] = 0;
            std::string log(infoLog.data());

            OutputDebugStringA(std::string("ERROR :" + log + "\n").c_str());
            return nullptr;
        }

        Microsoft::WRL::ComPtr<IDxcBlob> byteCode = nullptr;
        if (FAILED(pResult->GetResult(&byteCode)))
        {
            OutputDebugStringA("Can't GetResult\n");
            __debugbreak();
            return nullptr;
        }
        return byteCode;
    }

#ifdef RELEASE
    inline void Print(const char* msg) { printf("%s", msg); }
    inline void Print(const wchar_t* msg) { wprintf(L"%ws", msg); }
#else
    inline void Print(const char* msg) { OutputDebugStringA(msg); }
    inline void Print(const wchar_t* msg) { OutputDebugString(msg); }
#endif
    //va_start获取可变参数的第一个参数，然后vsprintf_s根据format把可变参数写入buffer
    inline void Printf(const char* format, ...)
    {
        char buffer[256];
        va_list ap;
        va_start(ap, format);
        vsprintf_s(buffer, 256, format, ap);
        va_end(ap);
        Print(buffer);
    }

    inline void Printf(const wchar_t* format, ...)
    {
        wchar_t buffer[256];
        va_list ap;
        va_start(ap, format);
        vswprintf(buffer, 256, format, ap);
        va_end(ap);
        Print(buffer);
    }

#ifndef RELEASE
    inline void PrintSubMessage(const char* format, ...)
    {
        Print("--> ");
        char buffer[256];
        va_list ap;
        va_start(ap, format);
        vsprintf_s(buffer, 256, format, ap);
        va_end(ap);
        Print(buffer);
        Print("\n");
    }
    inline void PrintSubMessage(const wchar_t* format, ...)
    {
        Print("--> ");
        wchar_t buffer[256];
        va_list ap;
        va_start(ap, format);
        vswprintf(buffer, 256, format, ap);
        va_end(ap);
        Print(buffer);
        Print("\n");
    }
    inline void PrintSubMessage(void)
    {
    }
#endif

}

#ifdef ERROR
#undef ERROR
#endif
#ifdef ASSERT
#undef ASSERT
#endif
#ifdef HALT
#undef HALT
#endif

#ifdef RELEASE

#define ASSERT( isTrue, ... ) (void)(isTrue)
#define WARN_ONCE_IF( isTrue, ... ) (void)(isTrue)
#define WARN_ONCE_IF_NOT( isTrue, ... ) (void)(isTrue)
#define ERROR( msg, ... )
#define DEBUGPRINT( msg, ... ) do {} while(0)
#define ASSERT_SUCCEEDED( hr, ... ) (void)(hr)

#else	// !RELEASE

// 把x加上双引号
// 可变参数宏，宏展开会把__VA_ARGS__替换成省略号匹配的所有参数(包括逗号)
// 宏定义中反斜杠是为了换行
// __FILE__是宏展开时候的源文件名称，__LINE__是宏展开时候的整数行号
// __debugbreak自动触发断点
#define STRINGIFY(x) #x
#define STRINGIFY_BUILTIN(x) STRINGIFY(x)
#define ASSERT( isFalse, ... ) \
        if (!(bool)(isFalse)) { \
            Utility::Print("\nAssertion failed in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n"); \
            Utility::PrintSubMessage("\'" #isFalse "\' is false"); \
            Utility::PrintSubMessage(__VA_ARGS__); \
            Utility::Print("\n"); \
            __debugbreak(); \
        }

#define ASSERT_SUCCEEDED( hr, ... ) \
        if (FAILED(hr)) { \
            Utility::Print("\nHRESULT failed in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n"); \
            Utility::PrintSubMessage("hr = 0x%08X", hr); \
            Utility::PrintSubMessage(__VA_ARGS__); \
            Utility::Print("\n"); \
            __debugbreak(); \
        }


#define WARN_ONCE_IF( isTrue, ... ) \
    { \
        static bool s_TriggeredWarning = false; \
        if ((bool)(isTrue) && !s_TriggeredWarning) { \
            s_TriggeredWarning = true; \
            Utility::Print("\nWarning issued in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n"); \
            Utility::PrintSubMessage("\'" #isTrue "\' is true"); \
            Utility::PrintSubMessage(__VA_ARGS__); \
            Utility::Print("\n"); \
        } \
    }

#define WARN_ONCE_IF_NOT( isTrue, ... ) WARN_ONCE_IF(!(isTrue), __VA_ARGS__)

#define ERROR( ... ) \
        Utility::Print("\nError reported in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n"); \
        Utility::PrintSubMessage(__VA_ARGS__); \
        Utility::Print("\n");

#define DEBUGPRINT( msg, ... ) \
    Utility::Printf( msg "\n", ##__VA_ARGS__ );

#endif