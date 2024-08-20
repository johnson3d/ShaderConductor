/*
 * ShaderConductor
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <ShaderConductor/ShaderConductor.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <tuple>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Unknwnbase.h>
#include <Windows.h>
#ifdef __MINGW32__
#include <_mingw.h>
#endif
#else
#include <clocale>
#endif

#ifdef __MINGW32__
namespace
{
    constexpr uint8_t NybbleFromHex(char c)
    {
        return ((c >= '0' && c <= '9')
                    ? (c - '0')
                    : ((c >= 'a' && c <= 'f') ? (c - 'a' + 10) : ((c >= 'A' && c <= 'F') ? (c - 'A' + 10) : /* Should be an error */ -1)));
    }

    constexpr uint8_t ByteFromHexstr(const char str[2])
    {
        return (NybbleFromHex(str[0]) << 4) | NybbleFromHex(str[1]);
    }

    constexpr uint32_t GetGuidData1FromString(const char str[37])
    {
        return (static_cast<uint32_t>(ByteFromHexstr(str)) << 24) | (static_cast<uint32_t>(ByteFromHexstr(str + 2)) << 16) |
               (static_cast<uint32_t>(ByteFromHexstr(str + 4)) << 8) | ByteFromHexstr(str + 6);
    }

    constexpr uint16_t GetGuidData2FromString(const char str[37])
    {
        return (static_cast<uint16_t>(ByteFromHexstr(str + 9)) << 8) | ByteFromHexstr(str + 11);
    }

    constexpr uint16_t GetGuidData3FromString(const char str[37])
    {
        return (static_cast<uint16_t>(ByteFromHexstr(str + 14)) << 8) | ByteFromHexstr(str + 16);
    }

    constexpr uint8_t GetGuidData4FromString(const char str[37], uint32_t index)
    {
        return ByteFromHexstr(str + 19 + index * 2 + (index >= 2 ? 1 : 0));
    }
} // namespace

#define CROSS_PLATFORM_UUIDOF(type, spec)                                                                                                  \
    class type;                                                                                                                            \
    __CRT_UUID_DECL(type, GetGuidData1FromString(spec), GetGuidData2FromString(spec), GetGuidData3FromString(spec),                        \
                    GetGuidData4FromString(spec, 0), GetGuidData4FromString(spec, 1), GetGuidData4FromString(spec, 2),                     \
                    GetGuidData4FromString(spec, 3), GetGuidData4FromString(spec, 4), GetGuidData4FromString(spec, 5),                     \
                    GetGuidData4FromString(spec, 6), GetGuidData4FromString(spec, 7))
#endif
//#include <dxc/WinAdapter.h>
#ifdef __MINGW32__
#define _Maybenull_
#endif
#include <dxc/dxcapi.h>

#include <spirv-tools/libspirv.h>
#include <spirv.hpp>
#include <spirv_cross.hpp>
#include <spirv_cross_util.hpp>
#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>

#ifndef _WIN32
#define interface struct
#endif
#include <directx/d3d12shader.h>
#ifndef _WIN32
#undef interface
#endif

#ifdef __MINGW32__
CROSS_PLATFORM_UUIDOF(ID3D12LibraryReflection, "8E349D19-54DB-4A56-9DC9-119D87BDB804")
CROSS_PLATFORM_UUIDOF(ID3D12ShaderReflection, "E913C351-783D-48CA-A1D1-4F306284AD56")
#endif

#include "ComPtr.hpp"
#include "ErrorHandling.hpp"

using namespace ShaderConductor;

namespace
{
#ifndef _WIN32
#ifdef __APPLE__
    constexpr const char* Utf8Locale = "en_US.UTF-8";
#else
    constexpr const char* Utf8Locale = "en_US.utf8";
#endif
#endif

    std::wstring Utf8ToWide(const char* utf8Str)
    {
        std::wstring wideStr;

        const size_t utf8Len = (utf8Str == nullptr) ? 0 : std::strlen(utf8Str);
        if (utf8Len > 0)
        {
#ifdef _WIN32
            const int wideLen = ::MultiByteToWideChar(CP_UTF8, 0, utf8Str, static_cast<int>(utf8Len), nullptr, 0);
            if (wideLen > 0)
            {
                wideStr.resize(wideLen);
                ::MultiByteToWideChar(CP_UTF8, 0, utf8Str, static_cast<int>(utf8Len), wideStr.data(), wideLen);
            }
#else
            char* const locale = std::setlocale(LC_CTYPE, Utf8Locale);

            const size_t wideLen = ::mbstowcs(nullptr, utf8Str, 0);
            if (wideLen > 0)
            {
                wideStr.resize(wideLen);
                ::mbstowcs(wideStr.data(), utf8Str, wideLen);
            }

            std::setlocale(LC_CTYPE, locale);
#endif
        }

        return wideStr;
    }

    std::string WideToUtf8(const wchar_t* wideStr)
    {
        std::string utf8Str;

        const size_t wideLen = (wideStr == nullptr) ? 0 : std::wcslen(wideStr);
        if (wideLen > 0)
        {
#ifdef _WIN32
            const int utf8Len = ::WideCharToMultiByte(CP_UTF8, 0, wideStr, static_cast<int>(wideLen), nullptr, 0, nullptr, nullptr);
            if (utf8Len > 0)
            {
                utf8Str.resize(utf8Len);
                ::WideCharToMultiByte(CP_UTF8, 0, wideStr, static_cast<int>(wideLen), utf8Str.data(), static_cast<int>(utf8Str.size()),
                                      nullptr, nullptr);
            }
#else
            char* const locale = std::setlocale(LC_CTYPE, Utf8Locale);

            const size_t utf8Len = ::wcstombs(nullptr, wideStr, 0);
            if (utf8Len > 0)
            {
                utf8Str.resize(utf8Len);
                ::wcstombs(utf8Str.data(), wideStr, utf8Len);
            }

            std::setlocale(LC_CTYPE, locale);
#endif
        }

        return utf8Str;
    }

    std::filesystem::path DllSelfPath()
    {
#if defined(_WIN32)
        HMODULE dllModule;
        ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCSTR>(DllSelfPath), &dllModule);

        char rawDllPath[MAX_PATH];
        ::GetModuleFileNameA(dllModule, rawDllPath, sizeof(rawDllPath));
        const std::filesystem::path dllPath(rawDllPath);
#else
        Dl_info dlInfo;
        ::dladdr(reinterpret_cast<const void*>(DllSelfPath), &dlInfo);
        const std::filesystem::path dllPath(dlInfo.dli_fname);
#endif

        return std::filesystem::absolute(dllPath);
    }

    class Dxcompiler
    {
    public:
        ~Dxcompiler() noexcept
        {
            this->Destroy();
        }

        static Dxcompiler& Instance()
        {
            static Dxcompiler instance;
            return instance;
        }

        static void DllDetaching(bool detaching) noexcept
        {
            m_dllDetaching = detaching;
        }

        IDxcUtils& Utils() const noexcept
        {
            return *m_utils;
        }

        IDxcCompiler3& Compiler() const noexcept
        {
            return *m_compiler;
        }

        ComPtr<IDxcLinker> CreateLinker() const
        {
            ComPtr<IDxcLinker> linker;
            const HRESULT hr = m_createInstanceFunc(CLSID_DxcLinker, __uuidof(IDxcLinker), linker.PutVoid());
            if (FAILED(hr))
            {
                linker = nullptr;
            }
            return linker;
        }

        bool LinkerSupport() const noexcept
        {
            return m_linkerSupport;
        }

        void Destroy() noexcept
        {
            if (m_dxcompilerDll)
            {
                m_compiler = nullptr;
                m_utils = nullptr;

                m_createInstanceFunc = nullptr;

#ifdef _WIN32
                ::FreeLibrary(m_dxcompilerDll);
#else
                ::dlclose(m_dxcompilerDll);
#endif

                m_dxcompilerDll = nullptr;
            }
        }

        void Terminate() noexcept
        {
            if (m_dxcompilerDll)
            {
                m_compiler.Detach();
                m_utils.Detach();

                m_createInstanceFunc = nullptr;

                m_dxcompilerDll = nullptr;
            }
        }

    private:
        Dxcompiler()
        {
            if (m_dllDetaching)
            {
                return;
            }

#ifdef _WIN32
            const char* dllName = "dxcompiler.dll";
#elif __APPLE__
            const char* dllName = "libdxcompiler.dylib";
#else
            const char* dllName = "libdxcompiler.so";
#endif
            const char* functionName = "DxcCreateInstance";

            const std::filesystem::path dllPath = DllSelfPath().parent_path() / dllName;
#ifdef _WIN32
            m_dxcompilerDll = ::LoadLibraryW(dllPath.wstring().c_str());
#else
            m_dxcompilerDll = ::dlopen(dllPath.string().c_str(), RTLD_LAZY);
#endif

            if (m_dxcompilerDll != nullptr)
            {
#ifdef _WIN32
                m_createInstanceFunc =
                    reinterpret_cast<DxcCreateInstanceProc>(reinterpret_cast<void*>(::GetProcAddress(m_dxcompilerDll, functionName)));
#else
                m_createInstanceFunc = reinterpret_cast<DxcCreateInstanceProc>(::dlsym(m_dxcompilerDll, functionName));
#endif

                if (m_createInstanceFunc != nullptr)
                {
                    TIFHR(m_createInstanceFunc(CLSID_DxcUtils, __uuidof(IDxcUtils), m_utils.PutVoid()));
                    TIFHR(m_createInstanceFunc(CLSID_DxcCompiler, __uuidof(IDxcCompiler3), m_compiler.PutVoid()));
                }
                else
                {
                    this->Destroy();

                    throw std::runtime_error(std::string("COULDN'T get ") + functionName + " from dxcompiler.");
                }
            }
            else
            {
                throw std::runtime_error("COULDN'T load dxcompiler.");
            }

            m_linkerSupport = (CreateLinker() != nullptr);
        }

    private:
        static bool m_dllDetaching;

        HMODULE m_dxcompilerDll = nullptr;
        DxcCreateInstanceProc m_createInstanceFunc = nullptr;

        ComPtr<IDxcUtils> m_utils;
        ComPtr<IDxcCompiler3> m_compiler;

        bool m_linkerSupport = false;
    };

    bool Dxcompiler::m_dllDetaching = false;

    class ScIncludeHandler : public IDxcIncludeHandler
    {
    public:
        explicit ScIncludeHandler(std::function<Blob(const char* includeName)> loadCallback) noexcept
            : m_loadCallback(std::move(loadCallback))
        {
        }

        virtual ~ScIncludeHandler() noexcept = default;

        HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR fileName, IDxcBlob** includeSource) override
        {
            if ((fileName[0] == L'.') && (fileName[1] == L'/'))
            {
                fileName += 2;
            }

            Blob source;
            try
            {
                const std::string utf8FileName = WideToUtf8(fileName);
                source = m_loadCallback(utf8FileName.c_str());
            }
            catch (...)
            {
                return E_FAIL;
            }

            *includeSource = nullptr;
            return Dxcompiler::Instance().Utils().CreateBlob(source.Data(), source.Size(), CP_UTF8,
                                                             reinterpret_cast<IDxcBlobEncoding**>(includeSource));
        }

        ULONG STDMETHODCALLTYPE AddRef() override
        {
            ++m_ref;
            return m_ref;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            --m_ref;
            ULONG result = m_ref;
            if (result == 0)
            {
                delete this;
            }
            return result;
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
        {
            if (IsEqualIID(iid, __uuidof(IDxcIncludeHandler)))
            {
                *object = dynamic_cast<IDxcIncludeHandler*>(this);
                this->AddRef();
                return S_OK;
            }
            else if (IsEqualIID(iid, __uuidof(IUnknown)))
            {
                *object = dynamic_cast<IUnknown*>(this);
                this->AddRef();
                return S_OK;
            }
            else
            {
                return E_NOINTERFACE;
            }
        }

    private:
        std::function<Blob(const char* includeName)> m_loadCallback;

        std::atomic<ULONG> m_ref = 0;
    };

    Blob DefaultLoadCallback(const char* includeName)
    {
        std::vector<char> ret;
        std::ifstream includeFile(includeName, std::ios_base::in);
        if (includeFile)
        {
            includeFile.seekg(0, std::ios::end);
            ret.resize(static_cast<size_t>(includeFile.tellg()));
            includeFile.seekg(0, std::ios::beg);
            includeFile.read(ret.data(), ret.size());
            ret.resize(static_cast<size_t>(includeFile.gcount()));
        }
        else
        {
            throw std::runtime_error(std::string("COULDN'T load included file ") + includeName + ".");
        }
        return Blob(ret.data(), static_cast<uint32_t>(ret.size()));
    }

    void AppendError(Compiler::ResultDesc& result, const std::string& msg)
    {
        std::string errorMsg;
        if (result.errorWarningMsg.Size() != 0)
        {
            errorMsg.assign(reinterpret_cast<const char*>(result.errorWarningMsg.Data()), result.errorWarningMsg.Size());
        }
        if (!errorMsg.empty())
        {
            errorMsg += "\n";
        }
        errorMsg += msg;
        result.errorWarningMsg.Reset(errorMsg.data(), static_cast<uint32_t>(errorMsg.size()));
        result.hasError = true;
    }

    std::wstring ShaderProfileName(ShaderStage stage, Compiler::ShaderModel shaderModel, bool asModule)
    {
        std::wstring shaderProfile;
        if (asModule)
        {
            shaderProfile = L"lib";
            if (shaderModel.major_ver == 6)
            {
                shaderModel.minor_ver = std::max<uint8_t>(shaderModel.minor_ver, 3);
            }
        }
        else
        {
            switch (stage)
            {
            case ShaderStage::VertexShader:
                shaderProfile = L"vs";
                break;

            case ShaderStage::PixelShader:
                shaderProfile = L"ps";
                break;

            case ShaderStage::GeometryShader:
                shaderProfile = L"gs";
                break;

            case ShaderStage::HullShader:
                shaderProfile = L"hs";
                break;

            case ShaderStage::DomainShader:
                shaderProfile = L"ds";
                break;

            case ShaderStage::ComputeShader:
                shaderProfile = L"cs";
                break;

            default:
                SC_UNREACHABLE("Invalid shader stage.");
            }
        }

        shaderProfile.push_back(L'_');
        shaderProfile.push_back(L'0' + shaderModel.major_ver);
        shaderProfile.push_back(L'_');
        shaderProfile.push_back(L'0' + shaderModel.minor_ver);

        return shaderProfile;
    }

    Reflection MakeDxilReflection(IDxcBlob* dxilBlob, bool asModule);
    Reflection MakeSpirVReflection(const spirv_cross::Compiler& compiler);

    void ConvertDxcResult(Compiler::ResultDesc& result, IDxcResult* dxcResult, ShadingLanguage targetLanguage, bool asModule,
                          bool needReflection)
    {
        HRESULT status;
        TIFHR(dxcResult->GetStatus(&status));

        result.target.Reset();
        result.errorWarningMsg.Reset();

        if (dxcResult->HasOutput(DXC_OUT_ERRORS))
        {
            ComPtr<IDxcBlobUtf8> errors;
            TIFHR(dxcResult->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8), errors.PutVoid(), nullptr));
            if (errors != nullptr)
            {
                result.errorWarningMsg.Reset(errors->GetBufferPointer(), static_cast<uint32_t>(errors->GetBufferSize()));
                errors.Reset();
            }
        }

        result.hasError = true;
        if (SUCCEEDED(status))
        {
            DXC_OUT_KIND outKind = DXC_OUT_NONE;
            uint32_t sizeAdjust = 0;
            if (dxcResult->HasOutput(DXC_OUT_OBJECT))
            {
                outKind = DXC_OUT_OBJECT;
            }
            else if (dxcResult->HasOutput(DXC_OUT_DISASSEMBLY))
            {
                outKind = DXC_OUT_DISASSEMBLY;
                sizeAdjust = 1; // Remove end trailing \0
            }

            if (outKind != DXC_OUT_NONE)
            {
                ComPtr<IDxcBlob> resultBlob;
                TIFHR(dxcResult->GetOutput(outKind, __uuidof(IDxcBlob), resultBlob.PutVoid(), nullptr));
                if (resultBlob != nullptr)
                {
                    result.target.Reset(resultBlob->GetBufferPointer(), static_cast<uint32_t>(resultBlob->GetBufferSize() - sizeAdjust));
                    result.hasError = false;
                }
            }

            if (needReflection)
            {
                if (targetLanguage == ShadingLanguage::Dxil)
                {
                    // Gather reflection information only for ShadingLanguage::Dxil. SPIR-V reflection is gathered when cross-compiling.

                    ComPtr<IDxcBlob> reflectionBlob;
                    if (dxcResult->HasOutput(DXC_OUT_REFLECTION))
                    {
                        TIFHR(dxcResult->GetOutput(DXC_OUT_REFLECTION, __uuidof(IDxcBlob), reflectionBlob.PutVoid(), nullptr));
                    }
                    else
                    {
                        TIFHR(dxcResult->GetResult(reflectionBlob.Put()));
                    }

                    if (reflectionBlob != nullptr)
                    {
                        result.reflection = MakeDxilReflection(reflectionBlob.Get(), asModule);
                    }
                }
            }
        }
    }

    Compiler::ResultDesc CompileToBinary(const Compiler::SourceDesc& source, const Compiler::Options& options,
                                         const Compiler::TargetDesc& target)
    {
        assert((target.language == ShadingLanguage::Dxil) || (target.language == ShadingLanguage::SpirV));
        if (target.asModule && (target.language != ShadingLanguage::Dxil))
        {
            // Check https://github.com/microsoft/DirectXShaderCompiler/issues/2633 for details
            SC_UNREACHABLE("Spir-V module is not supported.");
        }

        DxcBuffer sourceBuf;
        sourceBuf.Ptr = source.source;
        sourceBuf.Size = std::strlen(source.source);
        sourceBuf.Encoding = CP_UTF8;
        TIFFALSE(sourceBuf.Size >= 4);

        std::vector<std::wstring> dxcArgStrings;

        for (size_t i = 0; i < source.numDefines; ++i)
        {
            const auto& define = source.defines[i];

            std::wstring defineStr = L"-D " + Utf8ToWide(define.name);
            if (define.value != nullptr)
            {
                defineStr += L"=" + Utf8ToWide(define.value);
            }

            dxcArgStrings.emplace_back(std::move(defineStr));
        }

        dxcArgStrings.push_back(L"-E " + Utf8ToWide(source.entryPoint));
        dxcArgStrings.push_back(L"-T " + ShaderProfileName(source.stage, options.shaderModel, target.asModule));

        // modify by johnson
        if (source.hlslExtVersion != nullptr)
            dxcArgStrings.push_back(L"-HV " + Utf8ToWide(source.hlslExtVersion));
        if (source.dxcArgString != nullptr)
            dxcArgStrings.push_back(Utf8ToWide(source.dxcArgString));
        // end modify
        
        // HLSL matrices are translated into SPIR-V OpTypeMatrixs in a transposed manner,
        // See also https://antiagainst.github.io/post/hlsl-for-vulkan-matrices/
        if (options.packMatricesInRowMajor)
        {
            dxcArgStrings.push_back(L"-Zpr");
        }
        else
        {
            dxcArgStrings.push_back(L"-Zpc");
        }

        if (options.enable16bitTypes)
        {
            if (options.shaderModel >= Compiler::ShaderModel{6, 2})
            {
                dxcArgStrings.push_back(L"-enable-16bit-types");
            }
            else
            {
                throw std::runtime_error("16-bit types requires shader model 6.2 or up.");
            }
        }

        if (options.enableDebugInfo)
        {
            dxcArgStrings.push_back(L"-Zi");
        }

        if (options.disableOptimizations)
        {
            dxcArgStrings.push_back(L"-Od");
        }
        else
        {
            if (options.optimizationLevel < 4)
            {
                dxcArgStrings.push_back(std::wstring(L"-O") + static_cast<wchar_t>(L'0' + options.optimizationLevel));
            }
            else
            {
                SC_UNREACHABLE("Invalid optimization level.");
            }
        }

        if (options.shiftAllCBuffersBindings > 0)
        {
            dxcArgStrings.push_back(L"-fvk-b-shift");
            dxcArgStrings.push_back(std::to_wstring(options.shiftAllCBuffersBindings));
            dxcArgStrings.push_back(L"all");
        }

        if (options.shiftAllUABuffersBindings > 0)
        {
            dxcArgStrings.push_back(L"-fvk-u-shift");
            dxcArgStrings.push_back(std::to_wstring(options.shiftAllUABuffersBindings));
            dxcArgStrings.push_back(L"all");
        }

        if (options.shiftAllSamplersBindings > 0)
        {
            dxcArgStrings.push_back(L"-fvk-s-shift");
            dxcArgStrings.push_back(std::to_wstring(options.shiftAllSamplersBindings));
            dxcArgStrings.push_back(L"all");
        }

        if (options.shiftAllTexturesBindings > 0)
        {
            dxcArgStrings.push_back(L"-fvk-t-shift");
            dxcArgStrings.push_back(std::to_wstring(options.shiftAllTexturesBindings));
            dxcArgStrings.push_back(L"all");
        }

        if (target.language == ShadingLanguage::SpirV)
        {
            dxcArgStrings.push_back(L"-spirv");
            //modify by johnson:DecorationHlslSemanticGOOGLE will enable SPV_GOOGLE_hlsl_functionality1...
            //dxcArgStrings.push_back(L"-fspv-reflect");
            //end modify

            if ((target.version != nullptr) && (std::strlen(target.version) >= 2))
            {
                const std::wstring versionWide = Utf8ToWide(target.version);

                std::wstring arg = L"-fspv-target-env=universal";
                arg += versionWide[0];
                arg += L'.';
                arg += &versionWide[1];
                dxcArgStrings.emplace_back(std::move(arg));
            }
        }

        if (target.asModule)
        {
            dxcArgStrings.push_back(L"-default-linkage external");
        }

        if (options.stripReflection && target.language != ShadingLanguage::SpirV)
        {
            // Reflection info has its own blob. No need to keep it in the DXIL blob.
            dxcArgStrings.push_back(L"-Qstrip_reflect");
        }

        dxcArgStrings.emplace_back(Utf8ToWide(source.fileName));

        std::vector<const wchar_t*> dxcArgs;
        dxcArgs.reserve(dxcArgStrings.size());
        for (const auto& arg : dxcArgStrings)
        {
            dxcArgs.push_back(arg.c_str());
        }

        ComPtr<IDxcIncludeHandler> includeHandler(new ScIncludeHandler(source.loadIncludeCallback));
        ComPtr<IDxcResult> compileResult;
        TIFHR(Dxcompiler::Instance().Compiler().Compile(&sourceBuf, dxcArgs.data(), static_cast<UINT32>(dxcArgs.size()),
                                                        includeHandler.Get(), __uuidof(IDxcResult), compileResult.PutVoid()));

        Compiler::ResultDesc ret{};
        ConvertDxcResult(ret, compileResult.Get(), target.language, target.asModule, options.needReflection);

        return ret;
    }

    Compiler::ResultDesc CrossCompile(const Compiler::ResultDesc& binaryResult, const Compiler::SourceDesc& source,
                                      const Compiler::Options& options, const Compiler::TargetDesc& target)
    {
        assert((target.language != ShadingLanguage::Dxil) && (target.language != ShadingLanguage::SpirV));
        assert((binaryResult.target.Size() & (sizeof(uint32_t) - 1)) == 0);

        Compiler::ResultDesc ret;

        ret.errorWarningMsg = binaryResult.errorWarningMsg;
        ret.isText = true;

        uint32_t intVersion = 0;
        if (target.version != nullptr)
        {
            intVersion = std::stoi(target.version);
        }

        const uint32_t* spirvIr = reinterpret_cast<const uint32_t*>(binaryResult.target.Data());
        const size_t spirvSize = binaryResult.target.Size() / sizeof(uint32_t);

        std::unique_ptr<spirv_cross::CompilerGLSL> compiler;
        bool combinedImageSamplers = false;
        bool buildDummySampler = false;

        switch (target.language)
        {
        case ShadingLanguage::Hlsl:
            if ((source.stage == ShaderStage::GeometryShader) || (source.stage == ShaderStage::HullShader) ||
                (source.stage == ShaderStage::DomainShader))
            {
                // Check https://github.com/KhronosGroup/SPIRV-Cross/issues/904, https://github.com/KhronosGroup/SPIRV-Cross/issues/905, and
                // https://github.com/KhronosGroup/SPIRV-Cross/issues/906 for details
                AppendError(ret, "GS, HS, and DS has not been supported yet.");
                return ret;
            }
            if ((source.stage == ShaderStage::GeometryShader) && (intVersion < 40))
            {
                AppendError(ret, "HLSL shader model earlier than 4.0 doesn't have GS or CS.");
                return ret;
            }
            if ((source.stage == ShaderStage::ComputeShader) && (intVersion < 50))
            {
                AppendError(ret, "CS in HLSL shader model earlier than 5.0 is not supported.");
                return ret;
            }
            if (((source.stage == ShaderStage::HullShader) || (source.stage == ShaderStage::DomainShader)) && (intVersion < 50))
            {
                AppendError(ret, "HLSL shader model earlier than 5.0 doesn't have HS or DS.");
                return ret;
            }
            compiler = std::make_unique<spirv_cross::CompilerHLSL>(spirvIr, spirvSize);
            break;

        case ShadingLanguage::Glsl:
        case ShadingLanguage::Essl:
            compiler = std::make_unique<spirv_cross::CompilerGLSL>(spirvIr, spirvSize);
            combinedImageSamplers = true;
            buildDummySampler = true;

            // Legacy GLSL fixups
            if (intVersion <= 300)
            {
                auto vars = compiler->get_active_interface_variables();
                for (auto& var : vars)
                {
                    auto varClass = compiler->get_storage_class(var);

                    // Make VS out and PS in variable names match
                    if ((source.stage == ShaderStage::VertexShader) && (varClass == spv::StorageClass::StorageClassOutput))
                    {
                        auto name = compiler->get_name(var);
                        if ((name.find("out_var_") == 0) || (name.find("out.var.") == 0))
                        {
                            name.replace(0, 8, "varying_");
                            compiler->set_name(var, name);
                        }
                    }
                    else if ((source.stage == ShaderStage::PixelShader) && (varClass == spv::StorageClass::StorageClassInput))
                    {
                        auto name = compiler->get_name(var);
                        if ((name.find("in_var_") == 0) || (name.find("in.var.") == 0))
                        {
                            name.replace(0, 7, "varying_");
                            compiler->set_name(var, name);
                        }
                    }
                }
            }
            break;

        case ShadingLanguage::Msl_macOS:
        case ShadingLanguage::Msl_iOS:
            if (source.stage == ShaderStage::GeometryShader)
            {
                AppendError(ret, "MSL doesn't have GS.");
                return ret;
            }
            compiler = std::make_unique<spirv_cross::CompilerMSL>(spirvIr, spirvSize);
            break;

        default:
            SC_UNREACHABLE("Invalid target language.");
        }

        spv::ExecutionModel model;
        switch (source.stage)
        {
        case ShaderStage::VertexShader:
            model = spv::ExecutionModelVertex;
            break;

        case ShaderStage::HullShader:
            model = spv::ExecutionModelTessellationControl;
            break;

        case ShaderStage::DomainShader:
            model = spv::ExecutionModelTessellationEvaluation;
            break;

        case ShaderStage::GeometryShader:
            model = spv::ExecutionModelGeometry;
            break;

        case ShaderStage::PixelShader:
            model = spv::ExecutionModelFragment;
            break;

        case ShaderStage::ComputeShader:
            model = spv::ExecutionModelGLCompute;
            break;

        default:
            SC_UNREACHABLE("Invalid shader stage.");
        }
        compiler->set_entry_point(source.entryPoint, model);

        spirv_cross::CompilerGLSL::Options opts = compiler->get_common_options();
        if (target.version != nullptr)
        {
            opts.version = intVersion;
        }
        opts.es = (target.language == ShadingLanguage::Essl);
        opts.force_temporary = false;
        opts.separate_shader_objects = true;
        opts.flatten_multidimensional_arrays = false;
        opts.enable_420pack_extension =
            (target.language == ShadingLanguage::Glsl) && ((target.version == nullptr) || (opts.version >= 420));
        opts.vulkan_semantics = false;
        opts.vertex.fixup_clipspace = false;
        opts.vertex.flip_vert_y = false;
        opts.vertex.support_nonzero_base_instance = true;
        compiler->set_common_options(opts);

        if (target.language == ShadingLanguage::Hlsl)
        {
            auto* hlslCompiler = static_cast<spirv_cross::CompilerHLSL*>(compiler.get());
            auto hlslOpts = hlslCompiler->get_hlsl_options();
            if (target.version != nullptr)
            {
                if (opts.version < 30)
                {
                    AppendError(ret, "HLSL shader model earlier than 3.0 is not supported.");
                    return ret;
                }
                hlslOpts.shader_model = opts.version;
            }

            if (hlslOpts.shader_model <= 30)
            {
                combinedImageSamplers = true;
                buildDummySampler = true;
            }

            hlslCompiler->set_hlsl_options(hlslOpts);
        }
        else if ((target.language == ShadingLanguage::Msl_macOS) || (target.language == ShadingLanguage::Msl_iOS))
        {
            auto* mslCompiler = static_cast<spirv_cross::CompilerMSL*>(compiler.get());
            auto mslOpts = mslCompiler->get_msl_options();
            if (target.version != nullptr)
            {
                mslOpts.msl_version = opts.version;
            }
            mslOpts.swizzle_texture_samples = false;
            mslOpts.use_framebuffer_fetch_subpasses = true;
            mslOpts.platform = (target.language == ShadingLanguage::Msl_iOS) ? spirv_cross::CompilerMSL::Options::iOS
                                                                             : spirv_cross::CompilerMSL::Options::macOS;

            mslCompiler->set_msl_options(mslOpts);

            const auto& resources = mslCompiler->get_shader_resources();

            uint32_t textureBinding = 0;
            for (const auto& image : resources.separate_images)
            {
                mslCompiler->set_decoration(image.id, spv::DecorationBinding, textureBinding);
                ++textureBinding;
            }

            uint32_t samplerBinding = 0;
            for (const auto& sampler : resources.separate_samplers)
            {
                mslCompiler->set_decoration(sampler.id, spv::DecorationBinding, samplerBinding);
                ++samplerBinding;
            }
        }

        if (buildDummySampler)
        {
            const uint32_t sampler = compiler->build_dummy_sampler_for_combined_images();
            if (sampler != 0)
            {
                compiler->set_decoration(sampler, spv::DecorationDescriptorSet, 0);
                compiler->set_decoration(sampler, spv::DecorationBinding, 0);
            }
        }

        if (combinedImageSamplers)
        {
            compiler->build_combined_image_samplers();

            if (options.inheritCombinedSamplerBindings)
            {
                spirv_cross_util::inherit_combined_sampler_bindings(*compiler);
            }

            for (auto& remap : compiler->get_combined_image_samplers())
            {
                compiler->set_name(remap.combined_id,
                                   "SPIRV_Cross_Combined" + compiler->get_name(remap.image_id) + compiler->get_name(remap.sampler_id));
            }
        }

        if (target.language == ShadingLanguage::Hlsl)
        {
            auto* hlslCompiler = static_cast<spirv_cross::CompilerHLSL*>(compiler.get());
            const uint32_t newBuiltin = hlslCompiler->remap_num_workgroups_builtin();
            if (newBuiltin)
            {
                compiler->set_decoration(newBuiltin, spv::DecorationDescriptorSet, 0);
                compiler->set_decoration(newBuiltin, spv::DecorationBinding, 0);
            }
        }

        if ((target.language == ShadingLanguage::Msl_iOS) || (target.language == ShadingLanguage::Essl))
        {
            const spirv_cross::ShaderResources resources = compiler->get_shader_resources();
            for (const auto& resource : resources.subpass_inputs)
            {
                const uint32_t binding = compiler->get_decoration(resource.id, spv::DecorationBinding);
                compiler->remap_ext_framebuffer_fetch(binding, binding, true);
            }
        }

        try
        {
            const std::string targetStr = compiler->compile();
            ret.target.Reset(targetStr.data(), static_cast<uint32_t>(targetStr.size()));
            ret.hasError = false;
            if (options.needReflection)
            {
                ret.reflection = MakeSpirVReflection(*compiler);
            }
        }
        catch (spirv_cross::CompilerError& error)
        {
            const char* errorMsg = error.what();
            ret.errorWarningMsg.Reset(errorMsg, static_cast<uint32_t>(std::strlen(errorMsg)));
            ret.hasError = true;
        }

        return ret;
    }

    Compiler::ResultDesc ConvertBinary(const Compiler::ResultDesc& binaryResult, const Compiler::SourceDesc& source,
                                       const Compiler::Options& options, const Compiler::TargetDesc& target)
    {
        if (!binaryResult.hasError)
        {
            if (target.asModule)
            {
                return binaryResult;
            }
            else
            {
                switch (target.language)
                {
                case ShadingLanguage::Dxil:
                case ShadingLanguage::SpirV:
                    return binaryResult;

                case ShadingLanguage::Hlsl:
                case ShadingLanguage::Glsl:
                case ShadingLanguage::Essl:
                case ShadingLanguage::Msl_macOS:
                case ShadingLanguage::Msl_iOS:
                    return CrossCompile(binaryResult, source, options, target);

                default:
                    SC_UNREACHABLE("Invalid shading language.");
                }
            }
        }
        else
        {
            return binaryResult;
        }
    }

    template <typename T>
    void ExtractDxilResourceDesc(std::vector<Reflection::ResourceDesc>& resourceDescs,
                                 std::vector<Reflection::ConstantBuffer>& constantBuffers, T* d3d12Reflection, uint32_t resourceIndex);
} // namespace

namespace ShaderConductor
{
    class Blob::BlobImpl
    {
    public:
        BlobImpl(const void* data, uint32_t size) noexcept
            : m_data(reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + size)
        {
        }

        const void* Data() const noexcept
        {
            return m_data.data();
        }

        uint32_t Size() const noexcept
        {
            return static_cast<uint32_t>(m_data.size());
        }

    private:
        std::vector<uint8_t> m_data;
    };

    Blob::Blob() noexcept = default;

    Blob::Blob(const void* data, uint32_t size)
    {
        this->Reset(data, size);
    }

    Blob::Blob(const Blob& other)
    {
        this->Reset(other.Data(), other.Size());
    }

    Blob::Blob(Blob&& other) noexcept : m_impl(std::move(other.m_impl))
    {
        other.m_impl = nullptr;
    }

    Blob::~Blob() noexcept
    {
        delete m_impl;
    }

    Blob& Blob::operator=(const Blob& other)
    {
        if (this != &other)
        {
            this->Reset(other.Data(), other.Size());
        }
        return *this;
    }

    Blob& Blob::operator=(Blob&& other) noexcept
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = std::move(other.m_impl);
            other.m_impl = nullptr;
        }
        return *this;
    }

    void Blob::Reset() noexcept
    {
        delete m_impl;
        m_impl = nullptr;
    }

    void Blob::Reset(const void* data, uint32_t size)
    {
        this->Reset();
        if ((data != nullptr) && (size > 0))
        {
            m_impl = new BlobImpl(data, size);
        }
    }

    const void* Blob::Data() const noexcept
    {
        return m_impl ? m_impl->Data() : nullptr;
    }

    uint32_t Blob::Size() const noexcept
    {
        return m_impl ? m_impl->Size() : 0;
    }


    class Reflection::VariableType::VariableTypeImpl
    {
    public:
        explicit VariableTypeImpl(ID3D12ShaderReflectionType* d3d12Type)
        {
            D3D12_SHADER_TYPE_DESC d3d12ShaderTypeDesc;
            TIFHR(d3d12Type->GetDesc(&d3d12ShaderTypeDesc));

            if (d3d12ShaderTypeDesc.Name)
            {
                m_name = d3d12ShaderTypeDesc.Name;
            }

            switch (d3d12ShaderTypeDesc.Type)
            {
            case D3D_SVT_BOOL:
                m_type = DataType::Bool;
                break;
            case D3D_SVT_INT:
                m_type = DataType::Int;
                break;
            case D3D_SVT_UINT:
                m_type = DataType::Uint;
                break;
            case D3D_SVT_FLOAT:
                m_type = DataType::Float;
                break;

            case D3D_SVT_MIN16FLOAT:
                m_type = DataType::Half;
                break;
            case D3D_SVT_MIN16INT:
                m_type = DataType::Int16;
                break;
            case D3D_SVT_MIN16UINT:
                m_type = DataType::Uint16;
                break;

            case D3D_SVT_VOID:
                if (d3d12ShaderTypeDesc.Class == D3D_SVC_STRUCT)
                {
                    m_type = DataType::Struct;

                    for (uint32_t memberIndex = 0; memberIndex < d3d12ShaderTypeDesc.Members; ++memberIndex)
                    {
                        VariableDesc member{};

                        const char* memberName = d3d12Type->GetMemberTypeName(memberIndex);
                        std::strncpy(member.name, memberName, sizeof(member.name));
                        member.name[sizeof(member.name) - 1] = '\0';

                        ID3D12ShaderReflectionType* d3d12MemberType = d3d12Type->GetMemberTypeByIndex(memberIndex);
                        member.type = Make(d3d12MemberType);

                        D3D12_SHADER_TYPE_DESC d3d12MemberTypeDesc;
                        TIFHR(d3d12MemberType->GetDesc(&d3d12MemberTypeDesc));

                        member.offset = d3d12MemberTypeDesc.Offset;
                        if (d3d12MemberTypeDesc.Elements == 0)
                        {
                            member.size = 0;
                        }
                        else
                        {
                            member.size = (member.type.Elements() - 1) * member.type.ElementStride();
                        }
                        member.size += member.type.Rows() * member.type.Columns() * 4;

                        m_members.emplace_back(std::move(member));
                    }
                }
                else
                {
                    m_type = DataType::Void;
                }
                break;

            default:
                SC_UNREACHABLE("Unsupported variable type.");
            }

            m_rows = d3d12ShaderTypeDesc.Rows;
            m_columns = d3d12ShaderTypeDesc.Columns;
            m_elements = d3d12ShaderTypeDesc.Elements;

            if (m_elements > 0)
            {
                m_elementStride = m_rows * 16;
            }
            else
            {
                m_elementStride = 0;
            }
        }

        VariableTypeImpl(const spirv_cross::Compiler& compiler, const spirv_cross::SPIRType& spirvParentReflectionType,
                         uint32_t variableIndex, const spirv_cross::SPIRType& spirvReflectionType)
        {
            switch (spirvReflectionType.basetype)
            {
            case spirv_cross::SPIRType::Boolean:
                m_type = DataType::Bool;
                m_name = "bool";
                break;
            case spirv_cross::SPIRType::Int:
                m_type = DataType::Int;
                m_name = "int";
                break;
            case spirv_cross::SPIRType::UInt:
                m_type = DataType::Uint;
                m_name = "uint";
                break;
            case spirv_cross::SPIRType::Float:
                m_type = DataType::Float;
                m_name = "float";
                break;

            case spirv_cross::SPIRType::Half:
                m_type = DataType::Half;
                m_name = "half";
                break;
            case spirv_cross::SPIRType::Short:
                m_type = DataType::Int16;
                m_name = "int16_t";
                break;
            case spirv_cross::SPIRType::UShort:
                m_type = DataType::Uint16;
                m_name = "uint16_t";
                break;

            case spirv_cross::SPIRType::Struct:
                m_type = DataType::Struct;
                m_name = compiler.get_name(spirvReflectionType.self);

                for (uint32_t memberIndex = 0; memberIndex < spirvReflectionType.member_types.size(); ++memberIndex)
                {
                    VariableDesc member{};

                    const std::string& memberName = compiler.get_member_name(spirvReflectionType.self, memberIndex);
                    std::strncpy(member.name, memberName.c_str(), sizeof(member.name));
                    member.name[sizeof(member.name) - 1] = '\0';

                    member.type =
                        Make(compiler, spirvReflectionType, memberIndex, compiler.get_type(spirvReflectionType.member_types[memberIndex]));

                    member.offset = compiler.type_struct_member_offset(spirvReflectionType, memberIndex);
                    member.size = static_cast<uint32_t>(compiler.get_declared_struct_member_size(spirvReflectionType, memberIndex));

                    m_members.emplace_back(std::move(member));
                }
                break;

            case spirv_cross::SPIRType::Void:
                m_type = DataType::Void;
                m_name = "void";
                break;

            default:
                SC_UNREACHABLE("Unsupported variable type.");
            }

            if (spirvReflectionType.columns == 1)
            {
                if (spirvReflectionType.vecsize > 1)
                {
                    m_name += std::to_string(spirvReflectionType.vecsize);
                }
            }
            else
            {
                m_name += std::to_string(spirvReflectionType.columns) + 'x' + std::to_string(spirvReflectionType.vecsize);
            }

            m_rows = spirvReflectionType.columns;
            m_columns = spirvReflectionType.vecsize;
            if (compiler.has_member_decoration(spirvParentReflectionType.self, variableIndex, spv::DecorationColMajor))
            {
                std::swap(m_rows, m_columns);
            }
            if (spirvReflectionType.array.empty())
            {
                m_elements = 0;
            }
            else
            {
                m_elements = spirvReflectionType.array[0];
            }

            if (!spirvReflectionType.array.empty())
            {
                m_elementStride = compiler.type_struct_member_array_stride(spirvParentReflectionType, variableIndex);
            }
            else
            {
                m_elementStride = 0;
            }
        }

        const char* Name() const noexcept
        {
            return m_name.c_str();
        }

        DataType Type() const noexcept
        {
            return m_type;
        }

        uint32_t Rows() const noexcept
        {
            return m_rows;
        }

        uint32_t Columns() const noexcept
        {
            return m_columns;
        }

        uint32_t Elements() const noexcept
        {
            return m_elements;
        }

        uint32_t ElementStride() const noexcept
        {
            return m_elementStride;
        }

        uint32_t NumMembers() const noexcept
        {
            return static_cast<uint32_t>(m_members.size());
        }

        const VariableDesc* MemberByIndex(uint32_t index) const noexcept
        {
            if (index < m_members.size())
            {
                return &m_members[index];
            }

            return nullptr;
        }

        const VariableDesc* MemberByName(const char* name) const noexcept
        {
            for (const auto& member : m_members)
            {
                if (std::strcmp(member.name, name) == 0)
                {
                    return &member;
                }
            }

            return nullptr;
        }

        static VariableType Make(ID3D12ShaderReflectionType* d3d12ReflectionType)
        {
            VariableType ret;
            ret.m_impl = new VariableTypeImpl(d3d12ReflectionType);
            return ret;
        }

        static VariableType Make(const spirv_cross::Compiler& compiler, const spirv_cross::SPIRType& spirvParentReflectionType,
                                 uint32_t variableIndex, const spirv_cross::SPIRType& spirvReflectionType)
        {
            VariableType ret;
            ret.m_impl = new VariableTypeImpl(compiler, spirvParentReflectionType, variableIndex, spirvReflectionType);
            return ret;
        }

    private:
        std::string m_name;
        DataType m_type;
        uint32_t m_rows;
        uint32_t m_columns;
        uint32_t m_elements;
        uint32_t m_elementStride;

        std::vector<VariableDesc> m_members;
    };

    Reflection::VariableType::VariableType() noexcept = default;

    Reflection::VariableType::VariableType(const VariableType& other) : m_impl(other.m_impl ? new VariableTypeImpl(*other.m_impl) : nullptr)
    {
    }

    Reflection::VariableType::VariableType(VariableType&& other) noexcept : m_impl(std::move(other.m_impl))
    {
        other.m_impl = nullptr;
    }

    Reflection::VariableType::~VariableType() noexcept
    {
        delete m_impl;
    }

    Reflection::VariableType& Reflection::VariableType::operator=(const VariableType& other)
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = nullptr;

            if (other.m_impl)
            {
                m_impl = new VariableTypeImpl(*other.m_impl);
            }
        }
        return *this;
    }

    Reflection::VariableType& Reflection::VariableType::operator=(VariableType&& other) noexcept
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = std::move(other.m_impl);
            other.m_impl = nullptr;
        }
        return *this;
    }

    bool Reflection::VariableType::Valid() const noexcept
    {
        return m_impl != nullptr;
    }

    const char* Reflection::VariableType::Name() const noexcept
    {
        assert(Valid());
        return m_impl->Name();
    }

    Reflection::VariableType::DataType Reflection::VariableType::Type() const noexcept
    {
        assert(Valid());
        return m_impl->Type();
    }

    uint32_t Reflection::VariableType::Rows() const noexcept
    {
        assert(Valid());
        return m_impl->Rows();
    }

    uint32_t Reflection::VariableType::Columns() const noexcept
    {
        assert(Valid());
        return m_impl->Columns();
    }

    uint32_t Reflection::VariableType::Elements() const noexcept
    {
        assert(Valid());
        return m_impl->Elements();
    }

    uint32_t Reflection::VariableType::ElementStride() const noexcept
    {
        assert(Valid());
        return m_impl->ElementStride();
    }

    uint32_t Reflection::VariableType::NumMembers() const noexcept
    {
        assert(Valid());
        return m_impl->NumMembers();
    }

    const Reflection::VariableDesc* Reflection::VariableType::MemberByIndex(uint32_t index) const noexcept
    {
        assert(Valid());
        return m_impl->MemberByIndex(index);
    }

    const Reflection::VariableDesc* Reflection::VariableType::MemberByName(const char* name) const noexcept
    {
        assert(Valid());
        return m_impl->MemberByName(name);
    }


    class Reflection::ConstantBuffer::ConstantBufferImpl
    {
    public:
        explicit ConstantBufferImpl(ID3D12ShaderReflectionConstantBuffer* constantBuffer)
        {
            D3D12_SHADER_BUFFER_DESC bufferDesc;
            TIFHR(constantBuffer->GetDesc(&bufferDesc));

            m_name = bufferDesc.Name;

            for (uint32_t variableIndex = 0; variableIndex < bufferDesc.Variables; ++variableIndex)
            {
                ID3D12ShaderReflectionVariable* variable = constantBuffer->GetVariableByIndex(variableIndex);
                D3D12_SHADER_VARIABLE_DESC d3d12VariableDesc;
                TIFHR(variable->GetDesc(&d3d12VariableDesc));

                VariableDesc variableDesc{};

                std::strncpy(variableDesc.name, d3d12VariableDesc.Name, sizeof(variableDesc.name));
                variableDesc.name[sizeof(variableDesc.name) - 1] = '\0';

                variableDesc.type = VariableType::VariableTypeImpl::Make(variable->GetType());

                variableDesc.offset = d3d12VariableDesc.StartOffset;
                variableDesc.size = d3d12VariableDesc.Size;

                m_variableDescs.emplace_back(std::move(variableDesc));
            }

            m_size = bufferDesc.Size;
        }

        ConstantBufferImpl(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& resource)
        {
            const auto& cbufferType = compiler.get_type(resource.type_id);

            m_name = compiler.get_name(resource.id);

            for (uint32_t variableIndex = 0; variableIndex < cbufferType.member_types.size(); ++variableIndex)
            {
                VariableDesc variableDesc{};

                const std::string& varName = compiler.get_member_name(cbufferType.self, variableIndex);
                std::strncpy(variableDesc.name, varName.c_str(), sizeof(variableDesc.name));
                variableDesc.name[sizeof(variableDesc.name) - 1] = '\0';

                variableDesc.type = VariableType::VariableTypeImpl::Make(compiler, cbufferType, variableIndex,
                                                                         compiler.get_type(cbufferType.member_types[variableIndex]));

                variableDesc.offset = compiler.type_struct_member_offset(cbufferType, variableIndex);
                variableDesc.size = static_cast<uint32_t>(compiler.get_declared_struct_member_size(cbufferType, variableIndex));

                m_variableDescs.emplace_back(std::move(variableDesc));
            }

            m_size = static_cast<uint32_t>(compiler.get_declared_struct_size(cbufferType));
        }

        const char* Name() const noexcept
        {
            return m_name.c_str();
        }

        uint32_t Size() const noexcept
        {
            return m_size;
        }

        uint32_t NumVariables() const noexcept
        {
            return static_cast<uint32_t>(m_variableDescs.size());
        }

        const VariableDesc* VariableByIndex(uint32_t index) const noexcept
        {
            if (index < m_variableDescs.size())
            {
                return &m_variableDescs[index];
            }

            return nullptr;
        }

        const VariableDesc* VariableByName(const char* name) const noexcept
        {
            for (const auto& variableDesc : m_variableDescs)
            {
                if (std::strcmp(variableDesc.name, name) == 0)
                {
                    return &variableDesc;
                }
            }

            return nullptr;
        }

    private:
        std::string m_name;
        uint32_t m_size;
        std::vector<VariableDesc> m_variableDescs;
    };

    Reflection::ConstantBuffer::ConstantBuffer() noexcept = default;

    Reflection::ConstantBuffer::ConstantBuffer(const ConstantBuffer& other)
        : m_impl(other.m_impl ? new ConstantBufferImpl(*other.m_impl) : nullptr)
    {
    }

    Reflection::ConstantBuffer::ConstantBuffer(ConstantBuffer&& other) noexcept : m_impl(std::move(other.m_impl))
    {
        other.m_impl = nullptr;
    }

    Reflection::ConstantBuffer::~ConstantBuffer() noexcept
    {
        delete m_impl;
    }

    Reflection::ConstantBuffer& Reflection::ConstantBuffer::operator=(const ConstantBuffer& other)
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = nullptr;

            if (other.m_impl)
            {
                m_impl = new ConstantBufferImpl(*other.m_impl);
            }
        }
        return *this;
    }

    Reflection::ConstantBuffer& Reflection::ConstantBuffer::operator=(ConstantBuffer&& other) noexcept
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = std::move(other.m_impl);
            other.m_impl = nullptr;
        }
        return *this;
    }

    bool Reflection::ConstantBuffer::Valid() const noexcept
    {
        return m_impl != nullptr;
    }

    const char* Reflection::ConstantBuffer::Name() const noexcept
    {
        assert(Valid());
        return m_impl->Name();
    }

    uint32_t Reflection::ConstantBuffer::Size() const noexcept
    {
        assert(Valid());
        return m_impl->Size();
    }

    uint32_t Reflection::ConstantBuffer::NumVariables() const noexcept
    {
        assert(Valid());
        return m_impl->NumVariables();
    }

    const Reflection::VariableDesc* Reflection::ConstantBuffer::VariableByIndex(uint32_t index) const noexcept
    {
        assert(Valid());
        return m_impl->VariableByIndex(index);
    }

    const Reflection::VariableDesc* Reflection::ConstantBuffer::VariableByName(const char* name) const noexcept
    {
        assert(Valid());
        return m_impl->VariableByName(name);
    }


    class Reflection::Function::FunctionImpl
    {
    public:
        explicit FunctionImpl(ID3D12FunctionReflection* d3d12FuncReflection)
        {
            D3D12_FUNCTION_DESC d3d12FuncDesc;
            TIFHR(d3d12FuncReflection->GetDesc(&d3d12FuncDesc));

            m_name = d3d12FuncDesc.Name;

            for (uint32_t resourceIndex = 0; resourceIndex < d3d12FuncDesc.BoundResources; ++resourceIndex)
            {
                ExtractDxilResourceDesc(m_resourceDescs, m_constantBuffers, d3d12FuncReflection, resourceIndex);
            }
        }

        const char* Name() const noexcept
        {
            return m_name.c_str();
        }

        uint32_t NumResources() const noexcept
        {
            return static_cast<uint32_t>(m_resourceDescs.size());
        }

        const ResourceDesc* ResourceByIndex(uint32_t index) const noexcept
        {
            if (index < m_resourceDescs.size())
            {
                return &m_resourceDescs[index];
            }

            return nullptr;
        }

        const ResourceDesc* ResourceByName(const char* name) const noexcept
        {
            for (const auto& resourceDesc : m_resourceDescs)
            {
                if (std::strcmp(resourceDesc.name, name) == 0)
                {
                    return &resourceDesc;
                }
            }

            return nullptr;
        }

        uint32_t NumConstantBuffers() const noexcept
        {
            return static_cast<uint32_t>(m_resourceDescs.size());
        }

        const ConstantBuffer* ConstantBufferByIndex(uint32_t index) const noexcept
        {
            if (index < m_constantBuffers.size())
            {
                return &m_constantBuffers[index];
            }

            return nullptr;
        }

        const ConstantBuffer* ConstantBufferByName(const char* name) const noexcept
        {
            for (const auto& cbuffer : m_constantBuffers)
            {
                if (std::strcmp(cbuffer.Name(), name) == 0)
                {
                    return &cbuffer;
                }
            }

            return nullptr;
        }

        static Function Make(ID3D12FunctionReflection* d3d12FuncReflection)
        {
            Function ret;
            ret.m_impl = new FunctionImpl(d3d12FuncReflection);
            return ret;
        }

    private:
        std::string m_name;

        std::vector<ResourceDesc> m_resourceDescs;
        std::vector<ConstantBuffer> m_constantBuffers;
    };

    Reflection::Function::Function() noexcept = default;

    Reflection::Function::Function(const Function& other) : m_impl(other.m_impl ? new FunctionImpl(*other.m_impl) : nullptr)
    {
    }

    Reflection::Function::Function(Function&& other) noexcept : m_impl(std::move(other.m_impl))
    {
        other.m_impl = nullptr;
    }

    Reflection::Function::~Function() noexcept
    {
        delete m_impl;
    }

    Reflection::Function& Reflection::Function::operator=(const Function& other)
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = nullptr;

            if (other.m_impl)
            {
                m_impl = new FunctionImpl(*other.m_impl);
            }
        }
        return *this;
    }

    Reflection::Function& Reflection::Function::operator=(Function&& other) noexcept
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = std::move(other.m_impl);
            other.m_impl = nullptr;
        }
        return *this;
    }

    bool Reflection::Function::Valid() const noexcept
    {
        return m_impl != nullptr;
    }

    const char* Reflection::Function::Name() const noexcept
    {
        assert(Valid());
        return m_impl->Name();
    }

    uint32_t Reflection::Function::NumResources() const noexcept
    {
        assert(Valid());
        return m_impl->NumResources();
    }

    const Reflection::ResourceDesc* Reflection::Function::ResourceByIndex(uint32_t index) const noexcept
    {
        assert(Valid());
        return m_impl->ResourceByIndex(index);
    }

    const Reflection::ResourceDesc* Reflection::Function::ResourceByName(const char* name) const noexcept
    {
        assert(Valid());
        return m_impl->ResourceByName(name);
    }

    uint32_t Reflection::Function::NumConstantBuffers() const noexcept
    {
        assert(Valid());
        return m_impl->NumConstantBuffers();
    }

    const Reflection::ConstantBuffer* Reflection::Function::ConstantBufferByIndex(uint32_t index) const noexcept
    {
        assert(Valid());
        return m_impl->ConstantBufferByIndex(index);
    }

    const Reflection::ConstantBuffer* Reflection::Function::ConstantBufferByName(const char* name) const noexcept
    {
        assert(Valid());
        return m_impl->ConstantBufferByName(name);
    }


    class Reflection::ReflectionImpl
    {
    public:
        ReflectionImpl(IDxcBlob* dxilBlob, bool asModule)
        {
            auto& utils = Dxcompiler::Instance().Utils();

            DxcBuffer reflectionBuffer;
            reflectionBuffer.Ptr = dxilBlob->GetBufferPointer();
            reflectionBuffer.Size = dxilBlob->GetBufferSize();
            reflectionBuffer.Encoding = DXC_CP_ACP;

            if (asModule)
            {
                ComPtr<ID3D12LibraryReflection> libReflection;
                TIFHR(utils.CreateReflection(&reflectionBuffer, __uuidof(ID3D12LibraryReflection), libReflection.PutVoid()));

                D3D12_LIBRARY_DESC d3d12LibDesc;
                TIFHR(libReflection->GetDesc(&d3d12LibDesc));

                for (uint32_t funcIndex = 0; funcIndex < d3d12LibDesc.FunctionCount; ++funcIndex)
                {
                    ID3D12FunctionReflection* d3d12FuncReflection = libReflection->GetFunctionByIndex(funcIndex);
                    m_funcs.emplace_back(Function::FunctionImpl::Make(d3d12FuncReflection));
                }
            }
            else
            {
                ComPtr<ID3D12ShaderReflection> shaderReflection;
                TIFHR(utils.CreateReflection(&reflectionBuffer, __uuidof(ID3D12ShaderReflection), shaderReflection.PutVoid()));

                // modify by johnson3d
                m_shaderReflection = shaderReflection;
                // end modify

                D3D12_SHADER_DESC shaderDesc;
                TIFHR(shaderReflection->GetDesc(&shaderDesc));

                for (uint32_t resourceIndex = 0; resourceIndex < shaderDesc.BoundResources; ++resourceIndex)
                {
                    ExtractDxilResourceDesc(m_resourceDescs, m_constantBuffers, shaderReflection.Get(), resourceIndex);
                }

                for (uint32_t inputParamIndex = 0; inputParamIndex < shaderDesc.InputParameters; ++inputParamIndex)
                {
                    SignatureParameterDesc paramDesc{};

                    D3D12_SIGNATURE_PARAMETER_DESC signatureParamDesc;
                    shaderReflection->GetInputParameterDesc(inputParamIndex, &signatureParamDesc);

                    std::strncpy(paramDesc.semantic, signatureParamDesc.SemanticName, sizeof(paramDesc.semantic));
                    paramDesc.semantic[sizeof(paramDesc.semantic) - 1] = '\0';
                    paramDesc.semanticIndex = signatureParamDesc.SemanticIndex;
                    paramDesc.location = signatureParamDesc.Register;
                    switch (signatureParamDesc.ComponentType)
                    {
                    case D3D_REGISTER_COMPONENT_UINT32:
                        paramDesc.componentType = VariableType::DataType::Uint;
                        break;
                    case D3D_REGISTER_COMPONENT_SINT32:
                        paramDesc.componentType = VariableType::DataType::Int;
                        break;
                    case D3D_REGISTER_COMPONENT_FLOAT32:
                        paramDesc.componentType = VariableType::DataType::Float;
                        break;

                    default:
                        SC_UNREACHABLE("Unsupported input component type.");
                    }
                    paramDesc.mask = static_cast<ComponentMask>(signatureParamDesc.Mask);

                    m_inputParams.emplace_back(std::move(paramDesc));
                }

                for (uint32_t outputParamIndex = 0; outputParamIndex < shaderDesc.OutputParameters; ++outputParamIndex)
                {
                    SignatureParameterDesc paramDesc{};

                    D3D12_SIGNATURE_PARAMETER_DESC signatureParamDesc;
                    shaderReflection->GetOutputParameterDesc(outputParamIndex, &signatureParamDesc);

                    std::strncpy(paramDesc.semantic, signatureParamDesc.SemanticName, sizeof(paramDesc.semantic));
                    paramDesc.semantic[sizeof(paramDesc.semantic) - 1] = '\0';
                    paramDesc.semanticIndex = signatureParamDesc.SemanticIndex;
                    paramDesc.location = signatureParamDesc.Register;
                    switch (signatureParamDesc.ComponentType)
                    {
                    case D3D_REGISTER_COMPONENT_UINT32:
                        paramDesc.componentType = VariableType::DataType::Uint;
                        break;
                    case D3D_REGISTER_COMPONENT_SINT32:
                        paramDesc.componentType = VariableType::DataType::Int;
                        break;
                    case D3D_REGISTER_COMPONENT_FLOAT32:
                        paramDesc.componentType = VariableType::DataType::Float;
                        break;

                    default:
                        SC_UNREACHABLE("Unsupported output component type.");
                    }
                    paramDesc.mask = static_cast<ComponentMask>(signatureParamDesc.Mask);

                    m_outputParams.emplace_back(std::move(paramDesc));
                }

                switch (shaderDesc.InputPrimitive)
                {
                case D3D_PRIMITIVE_UNDEFINED:
                    m_gsHSInputPrimitive = PrimitiveTopology::Undefined;
                    break;
                case D3D_PRIMITIVE_POINT:
                    m_gsHSInputPrimitive = PrimitiveTopology::Points;
                    break;
                case D3D_PRIMITIVE_LINE:
                    m_gsHSInputPrimitive = PrimitiveTopology::Lines;
                    break;
                case D3D_PRIMITIVE_TRIANGLE:
                    m_gsHSInputPrimitive = PrimitiveTopology::Triangles;
                    break;
                case D3D_PRIMITIVE_LINE_ADJ:
                    m_gsHSInputPrimitive = PrimitiveTopology::LinesAdj;
                    break;
                case D3D_PRIMITIVE_TRIANGLE_ADJ:
                    m_gsHSInputPrimitive = PrimitiveTopology::TrianglesAdj;
                    break;
                case D3D_PRIMITIVE_1_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_1_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_2_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_2_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_3_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_3_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_4_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_4_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_5_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_5_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_6_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_6_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_7_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_7_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_8_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_8_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_9_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_9_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_10_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_10_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_11_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_11_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_12_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_12_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_13_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_13_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_14_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_14_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_15_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_15_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_16_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_16_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_17_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_17_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_18_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_18_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_19_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_19_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_20_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_20_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_21_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_21_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_22_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_22_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_23_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_23_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_24_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_24_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_25_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_25_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_26_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_26_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_27_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_27_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_28_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_28_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_29_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_29_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_30_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_30_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_31_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_31_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_32_CONTROL_POINT_PATCH:
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_32_CtrlPoint;
                    break;

                default:
                    SC_UNREACHABLE("Unsupported input primitive type.");
                }

                switch (shaderDesc.GSOutputTopology)
                {
                case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED:
                    m_gsOutputTopology = PrimitiveTopology::Undefined;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
                    m_gsOutputTopology = PrimitiveTopology::Points;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
                    m_gsOutputTopology = PrimitiveTopology::Lines;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
                    m_gsOutputTopology = PrimitiveTopology::LineStrip;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
                    m_gsOutputTopology = PrimitiveTopology::Triangles;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
                    m_gsOutputTopology = PrimitiveTopology::TriangleStrip;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
                    m_gsOutputTopology = PrimitiveTopology::LinesAdj;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
                    m_gsOutputTopology = PrimitiveTopology::LineStripAdj;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
                    m_gsOutputTopology = PrimitiveTopology::TrianglesAdj;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
                    m_gsOutputTopology = PrimitiveTopology::TriangleStripAdj;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_1_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_2_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_3_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_4_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_5_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_6_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_7_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_8_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_9_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_10_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_11_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_12_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_13_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_14_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_15_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_16_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_17_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_18_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_19_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_20_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_21_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_22_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_23_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_24_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_25_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_26_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_27_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_28_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_29_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_30_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_31_CtrlPoint;
                    break;
                case D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST:
                    m_gsOutputTopology = PrimitiveTopology::Patches_32_CtrlPoint;
                    break;

                default:
                    SC_UNREACHABLE("Unsupported output topoloty type.");
                }

                m_gsMaxNumOutputVertices = shaderDesc.GSMaxOutputVertexCount;
                m_gsNumInstances = shaderDesc.cGSInstanceCount;

                switch (shaderDesc.HSOutputPrimitive)
                {
                case D3D_TESSELLATOR_OUTPUT_UNDEFINED:
                    m_hsOutputPrimitive = TessellatorOutputPrimitive::Undefined;
                    break;
                case D3D_TESSELLATOR_OUTPUT_POINT:
                    m_hsOutputPrimitive = TessellatorOutputPrimitive::Point;
                    break;
                case D3D_TESSELLATOR_OUTPUT_LINE:
                    m_hsOutputPrimitive = TessellatorOutputPrimitive::Line;
                    break;
                case D3D_TESSELLATOR_OUTPUT_TRIANGLE_CW:
                    m_hsOutputPrimitive = TessellatorOutputPrimitive::TriangleCW;
                    break;
                case D3D_TESSELLATOR_OUTPUT_TRIANGLE_CCW:
                    m_hsOutputPrimitive = TessellatorOutputPrimitive::TriangleCCW;
                    break;

                default:
                    SC_UNREACHABLE("Unsupported output primitive type.");
                }

                switch (shaderDesc.HSPartitioning)
                {
                case D3D_TESSELLATOR_PARTITIONING_UNDEFINED:
                    m_hsPartitioning = TessellatorPartitioning::Undefined;
                    break;
                case D3D_TESSELLATOR_PARTITIONING_INTEGER:
                    m_hsPartitioning = TessellatorPartitioning::Integer;
                    break;
                case D3D_TESSELLATOR_PARTITIONING_POW2:
                    m_hsPartitioning = TessellatorPartitioning::Pow2;
                    break;
                case D3D_TESSELLATOR_PARTITIONING_FRACTIONAL_ODD:
                    m_hsPartitioning = TessellatorPartitioning::FractionalOdd;
                    break;
                case D3D_TESSELLATOR_PARTITIONING_FRACTIONAL_EVEN:
                    m_hsPartitioning = TessellatorPartitioning::FractionalEven;
                    break;

                default:
                    SC_UNREACHABLE("Unsupported partitioning type.");
                }

                switch (shaderDesc.TessellatorDomain)
                {
                case D3D_TESSELLATOR_DOMAIN_UNDEFINED:
                    m_hSDSTessellatorDomain = TessellatorDomain::Undefined;
                    break;
                case D3D_TESSELLATOR_DOMAIN_ISOLINE:
                    m_hSDSTessellatorDomain = TessellatorDomain::Line;
                    break;
                case D3D_TESSELLATOR_DOMAIN_TRI:
                    m_hSDSTessellatorDomain = TessellatorDomain::Triangle;
                    break;
                case D3D_TESSELLATOR_DOMAIN_QUAD:
                    m_hSDSTessellatorDomain = TessellatorDomain::Quad;
                    break;

                default:
                    SC_UNREACHABLE("Unsupported tessellator domain type.");
                }

                for (uint32_t patchConstantParamIndex = 0; patchConstantParamIndex < shaderDesc.PatchConstantParameters;
                     ++patchConstantParamIndex)
                {
                    SignatureParameterDesc paramDesc{};

                    D3D12_SIGNATURE_PARAMETER_DESC signatureParamDesc;
                    shaderReflection->GetPatchConstantParameterDesc(patchConstantParamIndex, &signatureParamDesc);

                    std::strncpy(paramDesc.semantic, signatureParamDesc.SemanticName, sizeof(paramDesc.semantic));
                    paramDesc.semantic[sizeof(paramDesc.semantic) - 1] = '\0';
                    paramDesc.semanticIndex = signatureParamDesc.SemanticIndex;
                    paramDesc.location = signatureParamDesc.Register;
                    switch (signatureParamDesc.ComponentType)
                    {
                    case D3D_REGISTER_COMPONENT_UINT32:
                        paramDesc.componentType = VariableType::DataType::Uint;
                        break;
                    case D3D_REGISTER_COMPONENT_SINT32:
                        paramDesc.componentType = VariableType::DataType::Int;
                        break;
                    case D3D_REGISTER_COMPONENT_FLOAT32:
                        paramDesc.componentType = VariableType::DataType::Float;
                        break;

                    default:
                        SC_UNREACHABLE("Unsupported patch constant component type.");
                    }
                    paramDesc.mask = static_cast<ComponentMask>(signatureParamDesc.Mask);

                    m_hsDSPatchConstantParams.emplace_back(std::move(paramDesc));
                }

                m_hsDSNumCtrlPoints = shaderDesc.cControlPoints;

                shaderReflection->GetThreadGroupSize(&m_csBlockSizeX, &m_csBlockSizeY, &m_csBlockSizeZ);
            }
        }
		
		// modify by johnson3d
        ComPtr<ID3D12ShaderReflection> m_shaderReflection;
        // end modify
        explicit ReflectionImpl(const spirv_cross::Compiler& compiler)
        {
            const auto& modes = compiler.get_execution_mode_bitset();
            switch (compiler.get_execution_model())
            {
            case spv::ExecutionModelTessellationControl:
                if (modes.get(spv::ExecutionModeOutputVertices))
                {
                    m_hsDSNumCtrlPoints = compiler.get_execution_mode_argument(spv::ExecutionModeOutputVertices, 0);
                    switch (m_hsDSNumCtrlPoints)
                    {
                    case 2:
                        m_hSDSTessellatorDomain = TessellatorDomain::Line;
                        break;
                    case 3:
                        m_hSDSTessellatorDomain = TessellatorDomain::Triangle;
                        break;
                    case 4:
                        m_hSDSTessellatorDomain = TessellatorDomain::Quad;
                        break;

                    default:
                        break;
                    }
                }

                if (modes.get(spv::ExecutionModeInputPoints))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_1_CtrlPoint;
                }
                else if (modes.get(spv::ExecutionModeInputLines))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_2_CtrlPoint;
                }
                else if (modes.get(spv::ExecutionModeTriangles))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_3_CtrlPoint;
                }

                if (modes.get(spv::ExecutionModeVertexOrderCw))
                {
                    m_hsOutputPrimitive = TessellatorOutputPrimitive::TriangleCW;
                }
                else if (modes.get(spv::ExecutionModeVertexOrderCcw))
                {
                    m_hsOutputPrimitive = TessellatorOutputPrimitive::TriangleCCW;
                }

                if (modes.get(spv::ExecutionModeSpacingEqual))
                {
                    m_hsPartitioning = TessellatorPartitioning::Integer;
                }
                else if (modes.get(spv::ExecutionModeSpacingFractionalOdd))
                {
                    m_hsPartitioning = TessellatorPartitioning::FractionalOdd;
                }
                else if (modes.get(spv::ExecutionModeSpacingFractionalOdd))
                {
                    m_hsPartitioning = TessellatorPartitioning::FractionalEven;
                }
                break;

            case spv::ExecutionModelTessellationEvaluation:
                if (modes.get(spv::ExecutionModeIsolines))
                {
                    m_hSDSTessellatorDomain = TessellatorDomain::Line;
                    m_hsDSNumCtrlPoints = 2;
                }
                else if (modes.get(spv::ExecutionModeTriangles))
                {
                    m_hSDSTessellatorDomain = TessellatorDomain::Triangle;
                    m_hsDSNumCtrlPoints = 3;
                }
                else if (modes.get(spv::ExecutionModeQuads))
                {
                    m_hSDSTessellatorDomain = TessellatorDomain::Quad;
                    m_hsDSNumCtrlPoints = 4;
                }
                break;

            case spv::ExecutionModelGeometry:
                if (modes.get(spv::ExecutionModeOutputVertices))
                {
                    m_gsMaxNumOutputVertices = compiler.get_execution_mode_argument(spv::ExecutionModeOutputVertices, 0);
                }

                if (modes.get(spv::ExecutionModeInvocations))
                {
                    m_gsNumInstances = compiler.get_execution_mode_argument(spv::ExecutionModeInvocations, 0);
                }

                if (modes.get(spv::ExecutionModeInputPoints))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::Points;
                }
                else if (modes.get(spv::ExecutionModeInputLines))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::Lines;
                }
                else if (modes.get(spv::ExecutionModeTriangles))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::Triangles;
                }
                else if (modes.get(spv::ExecutionModeInputLinesAdjacency))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::LinesAdj;
                }
                else if (modes.get(spv::ExecutionModeInputTrianglesAdjacency))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::TrianglesAdj;
                }

                if (modes.get(spv::ExecutionModeOutputPoints))
                {
                    m_gsOutputTopology = PrimitiveTopology::Points;
                }
                else if (modes.get(spv::ExecutionModeOutputLineStrip))
                {
                    m_gsOutputTopology = PrimitiveTopology::LineStrip;
                }
                else if (modes.get(spv::ExecutionModeOutputTriangleStrip))
                {
                    m_gsOutputTopology = PrimitiveTopology::TriangleStrip;
                }
                break;

            case spv::ExecutionModelGLCompute:
            {
                spirv_cross::SpecializationConstant spec_x, spec_y, spec_z;
                compiler.get_work_group_size_specialization_constants(spec_x, spec_y, spec_z);

                m_csBlockSizeX = spec_x.id != spirv_cross::ID(0) ? spec_x.constant_id
                                                                 : compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0);
                m_csBlockSizeY = spec_y.id != spirv_cross::ID(0) ? spec_y.constant_id
                                                                 : compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1);
                m_csBlockSizeZ = spec_z.id != spirv_cross::ID(0) ? spec_z.constant_id
                                                                 : compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2);
                break;
            }

            default:
                break;
            }

            const spirv_cross::ShaderResources resources = compiler.get_shader_resources();

            for (const auto& resource : resources.uniform_buffers)
            {
                ResourceDesc reflectionDesc{};
                this->ExtractReflection(reflectionDesc, compiler, resource.id);
                reflectionDesc.type = ShaderResourceType::ConstantBuffer;

                m_resourceDescs.emplace_back(std::move(reflectionDesc));

                m_constantBuffers.emplace_back(Make(compiler, resource));
            }

            for (const auto& resource : resources.storage_buffers)
            {
                ResourceDesc reflectionDesc{};
                this->ExtractReflection(reflectionDesc, compiler, resource.id);

                const spirv_cross::Bitset& typeFlags = compiler.get_decoration_bitset(compiler.get_type(resource.type_id).self);
                const auto& type = compiler.get_type(resource.type_id);

                const bool ssboBlock = type.storage == spv::StorageClassStorageBuffer ||
                                       (type.storage == spv::StorageClassUniform && typeFlags.get(spv::DecorationBufferBlock));
                if (ssboBlock)
                {
                    spirv_cross::Bitset buffer_flags = compiler.get_buffer_block_flags(resource.id);
                    if (buffer_flags.get(spv::DecorationNonWritable))
                    {
                        reflectionDesc.type = ShaderResourceType::ShaderResourceView;
                    }
                    else
                    {
                        reflectionDesc.type = ShaderResourceType::UnorderedAccessView;
                    }
                }
                else
                {
                    reflectionDesc.type = ShaderResourceType::ShaderResourceView;
                }

                m_resourceDescs.emplace_back(std::move(reflectionDesc));
            }

            for (const auto& resource : resources.storage_images)
            {
                ResourceDesc reflectionDesc{};
                this->ExtractReflection(reflectionDesc, compiler, resource.id);
                reflectionDesc.type = ShaderResourceType::UnorderedAccessView;

                m_resourceDescs.emplace_back(std::move(reflectionDesc));
            }

            for (const auto& resource : resources.separate_images)
            {
                ResourceDesc reflectionDesc{};
                this->ExtractReflection(reflectionDesc, compiler, resource.id);
                reflectionDesc.type = ShaderResourceType::Texture;

                m_resourceDescs.emplace_back(std::move(reflectionDesc));
            }

            for (const auto& resource : resources.separate_samplers)
            {
                ResourceDesc reflectionDesc{};
                this->ExtractReflection(reflectionDesc, compiler, resource.id);
                reflectionDesc.type = ShaderResourceType::Sampler;

                m_resourceDescs.emplace_back(std::move(reflectionDesc));
            }

            uint32_t combinedBinding = 0;
            for (const auto& resource : resources.sampled_images)
            {
                ResourceDesc reflectionDesc{};
                this->ExtractReflection(reflectionDesc, compiler, resource.id);
                reflectionDesc.bindPoint = combinedBinding;
                reflectionDesc.type = ShaderResourceType::Texture;

                m_resourceDescs.emplace_back(std::move(reflectionDesc));
                ++combinedBinding;
            }

            for (const auto& inputParam : resources.builtin_inputs)
            {
                if (IsNonTrivialBuiltIn(inputParam.builtin))
                {
                    SignatureParameterDesc paramDesc = this->ExtractParameter(compiler, inputParam.resource);

                    const auto& type = compiler.get_type(inputParam.resource.type_id);
                    switch (inputParam.builtin)
                    {
                    case spv::BuiltInTessLevelInner:
                    case spv::BuiltInTessLevelOuter:
                        uint32_t numParams;
                        if (inputParam.builtin == spv::BuiltInTessLevelOuter)
                        {
                            numParams = std::min(m_hsDSNumCtrlPoints, type.array[0]);
                            paramDesc.mask = ComponentMask::W;
                        }
                        else
                        {
                            numParams = std::min(1U, type.array[0]);
                            paramDesc.mask = ComponentMask::X;
                            paramDesc.location = m_hsDSNumCtrlPoints;
                        }
                        for (uint32_t patchConstantParamIndex = 0; patchConstantParamIndex < numParams; ++patchConstantParamIndex)
                        {
                            paramDesc.semanticIndex = patchConstantParamIndex;
                            if (inputParam.builtin == spv::BuiltInTessLevelOuter)
                            {
                                paramDesc.location = patchConstantParamIndex;
                            }
                            m_hsDSPatchConstantParams.push_back(paramDesc);
                        }
                        break;

                    default:
                        m_inputParams.emplace_back(std::move(paramDesc));
                        break;
                    }
                }
            }

            for (const auto& inputParam : resources.stage_inputs)
            {
                SignatureParameterDesc paramDesc = this->ExtractParameter(compiler, inputParam);

                if (compiler.get_decoration(inputParam.id, spv::DecorationPatch))
                {
                    m_hsDSPatchConstantParams.emplace_back(std::move(paramDesc));
                }
                else
                {
                    m_inputParams.emplace_back(std::move(paramDesc));
                }
            }

            for (const auto& outputParam : resources.builtin_outputs)
            {
                if (IsNonTrivialBuiltIn(outputParam.builtin))
                {
                    SignatureParameterDesc paramDesc = this->ExtractParameter(compiler, outputParam.resource);

                    const auto& type = compiler.get_type(outputParam.resource.type_id);
                    switch (outputParam.builtin)
                    {
                    case spv::BuiltInTessLevelInner:
                    case spv::BuiltInTessLevelOuter:
                        uint32_t numParams;
                        if (outputParam.builtin == spv::BuiltInTessLevelOuter)
                        {
                            numParams = std::min(m_hsDSNumCtrlPoints, type.array[0]);
                            paramDesc.mask = ComponentMask::W;
                        }
                        else
                        {
                            numParams = std::min(1U, type.array[0]);
                            paramDesc.mask = ComponentMask::X;
                            paramDesc.location = m_hsDSNumCtrlPoints;
                        }
                        for (uint32_t patchConstantParamIndex = 0; patchConstantParamIndex < numParams; ++patchConstantParamIndex)
                        {
                            paramDesc.semanticIndex = patchConstantParamIndex;
                            if (outputParam.builtin == spv::BuiltInTessLevelOuter)
                            {
                                paramDesc.location = patchConstantParamIndex;
                            }
                            m_hsDSPatchConstantParams.push_back(paramDesc);
                        }
                        break;

                    default:
                        m_outputParams.emplace_back(std::move(paramDesc));
                        break;
                    }
                }
            }

            for (const auto& outputParam : resources.stage_outputs)
            {
                SignatureParameterDesc paramDesc = this->ExtractParameter(compiler, outputParam);
                m_outputParams.emplace_back(std::move(paramDesc));
            }
        }

        uint32_t NumResources() const noexcept
        {
            return static_cast<uint32_t>(m_resourceDescs.size());
        }

        const ResourceDesc* ResourceByIndex(uint32_t index) const noexcept
        {
            if (index < m_resourceDescs.size())
            {
                return &m_resourceDescs[index];
            }

            return nullptr;
        }

        const ResourceDesc* ResourceByName(const char* name) const noexcept
        {
            for (const auto& resourceDesc : m_resourceDescs)
            {
                if (std::strcmp(resourceDesc.name, name) == 0)
                {
                    return &resourceDesc;
                }
            }

            return nullptr;
        }

        uint32_t NumConstantBuffers() const noexcept
        {
            return static_cast<uint32_t>(m_constantBuffers.size());
        }

        const ConstantBuffer* ConstantBufferByIndex(uint32_t index) const noexcept
        {
            if (index < m_resourceDescs.size())
            {
                return &m_constantBuffers[index];
            }

            return nullptr;
        }

        const ConstantBuffer* ConstantBufferByName(const char* name) const noexcept
        {
            for (const auto& cbuffer : m_constantBuffers)
            {
                if (std::strcmp(cbuffer.Name(), name) == 0)
                {
                    return &cbuffer;
                }
            }

            return nullptr;
        }

        uint32_t NumInputParameters() const noexcept
        {
            return static_cast<uint32_t>(m_inputParams.size());
        }

        const SignatureParameterDesc* InputParameter(uint32_t index) const noexcept
        {
            if (index < m_inputParams.size())
            {
                return &m_inputParams[index];
            }

            return nullptr;
        }

        uint32_t NumOutputParameters() const noexcept
        {
            return static_cast<uint32_t>(m_outputParams.size());
        }

        const SignatureParameterDesc* OutputParameter(uint32_t index) const noexcept
        {
            if (index < m_outputParams.size())
            {
                return &m_outputParams[index];
            }

            return nullptr;
        }

        PrimitiveTopology GSHSInputPrimitive() const noexcept
        {
            return m_gsHSInputPrimitive;
        }

        PrimitiveTopology GSOutputTopology() const noexcept
        {
            return m_gsOutputTopology;
        }

        uint32_t GSMaxNumOutputVertices() const noexcept
        {
            return m_gsMaxNumOutputVertices;
        }

        uint32_t GSNumInstances() const noexcept
        {
            return m_gsNumInstances;
        }

        TessellatorOutputPrimitive HSOutputPrimitive() const noexcept
        {
            return m_hsOutputPrimitive;
        }

        TessellatorPartitioning HSPartitioning() const noexcept
        {
            return m_hsPartitioning;
        }

        TessellatorDomain HSDSTessellatorDomain() const noexcept
        {
            return m_hSDSTessellatorDomain;
        }

        uint32_t HSDSNumPatchConstantParameters() const noexcept
        {
            return static_cast<uint32_t>(m_hsDSPatchConstantParams.size());
        }

        const SignatureParameterDesc* HSDSPatchConstantParameter(uint32_t index) const noexcept
        {
            if (index < m_hsDSPatchConstantParams.size())
            {
                return &m_hsDSPatchConstantParams[index];
            }

            return nullptr;
        }

        uint32_t HSDSNumConrolPoints() const noexcept
        {
            return m_hsDSNumCtrlPoints;
        }

        uint32_t CSBlockSizeX() const noexcept
        {
            return m_csBlockSizeX;
        }

        uint32_t CSBlockSizeY() const noexcept
        {
            return m_csBlockSizeY;
        }

        uint32_t CSBlockSizeZ() const noexcept
        {
            return m_csBlockSizeZ;
        }

        uint32_t NumFunctions() const noexcept
        {
            return static_cast<uint32_t>(m_funcs.size());
        }

        const Function* FunctionByIndex(uint32_t index) const noexcept
        {
            if (index < m_funcs.size())
            {
                return &m_funcs[index];
            }

            return nullptr;
        }

        const Function* FunctionByName(const char* name) const noexcept
        {
            for (const auto& func : m_funcs)
            {
                if (std::strcmp(func.Name(), name) == 0)
                {
                    return &func;
                }
            }

            return nullptr;
        }

        static Reflection Make(IDxcBlob* dxilBlob, bool asModule)
        {
            Reflection ret;
            ret.m_impl = new ReflectionImpl(dxilBlob, asModule);
            return ret;
        }

        static Reflection Make(const spirv_cross::Compiler& compiler)
        {
            Reflection ret;
            ret.m_impl = new ReflectionImpl(compiler);
            return ret;
        }

        static ConstantBuffer Make(ID3D12ShaderReflectionConstantBuffer* constantBuffer)
        {
            ConstantBuffer ret;
            ret.m_impl = new ConstantBuffer::ConstantBufferImpl(constantBuffer);
            return ret;
        }

        static ConstantBuffer Make(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& resource)
        {
            ConstantBuffer ret;
            ret.m_impl = new ConstantBuffer::ConstantBufferImpl(compiler, resource);
            return ret;
        }

    private:
        static bool IsNonTrivialBuiltIn(spv::BuiltIn builtin) noexcept
        {
            switch (builtin)
            {
            case spv::BuiltInPosition:
            case spv::BuiltInFragCoord:
            case spv::BuiltInFragDepth:
            case spv::BuiltInVertexId:
            case spv::BuiltInVertexIndex:
            case spv::BuiltInInstanceId:
            case spv::BuiltInInstanceIndex:
            case spv::BuiltInSampleId:
            case spv::BuiltInSampleMask:
            case spv::BuiltInTessLevelInner:
            case spv::BuiltInTessLevelOuter:
                return true;

            default:
                return false;
            }
        }

        static void ExtractReflection(ResourceDesc& reflectionDesc, const spirv_cross::Compiler& compiler, spirv_cross::ID id)
        {
            const uint32_t descSet = compiler.get_decoration(id, spv::DecorationDescriptorSet);
            const uint32_t binding = compiler.get_decoration(id, spv::DecorationBinding);

            const std::string& res_name = compiler.get_name(id);
            std::strncpy(reflectionDesc.name, res_name.c_str(), sizeof(reflectionDesc.name));
            reflectionDesc.name[sizeof(reflectionDesc.name) - 1] = '\0';
            reflectionDesc.space = descSet;
            reflectionDesc.bindPoint = binding;
            reflectionDesc.bindCount = 1;
        }

        static SignatureParameterDesc ExtractParameter(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& resource)
        {
            SignatureParameterDesc paramDesc{};

            const std::string& semantic = compiler.get_decoration_string(resource.id, spv::DecorationHlslSemanticGOOGLE);
            paramDesc.semanticIndex = 0;
            for (auto iter = semantic.rbegin(); iter != semantic.rend(); ++iter)
            {
                if (!std::isdigit(*iter))
                {
                    const int sep = static_cast<int>(std::distance(semantic.begin(), iter.base()));
                    const std::string indexPart = semantic.substr(sep);
                    if (indexPart.empty())
                    {
                        paramDesc.semanticIndex = 0;
                    }
                    else
                    {
                        paramDesc.semanticIndex = std::atoi(indexPart.c_str());
                    }
                    std::strncpy(paramDesc.semantic, semantic.c_str(), std::min<int>(sep, sizeof(paramDesc.semantic)));
                    paramDesc.semantic[sizeof(paramDesc.semantic) - 1] = '\0';
                    break;
                }
            }

            const auto& type = compiler.get_type(resource.type_id);
            switch (type.basetype)
            {
            case spirv_cross::SPIRType::UInt:
                paramDesc.componentType = VariableType::DataType::Uint;
                break;
            case spirv_cross::SPIRType::Int:
                paramDesc.componentType = VariableType::DataType::Int;
                break;
            case spirv_cross::SPIRType::Float:
                paramDesc.componentType = VariableType::DataType::Float;
                break;

            default:
                SC_UNREACHABLE("Unsupported parameter component type.");
            }

            if (type.vecsize > 0)
            {
                paramDesc.mask = ComponentMask::X;
            }
            if (type.vecsize > 1)
            {
                paramDesc.mask |= ComponentMask::Y;
            }
            if (type.vecsize > 2)
            {
                paramDesc.mask |= ComponentMask::Z;
            }
            if (type.vecsize > 3)
            {
                paramDesc.mask |= ComponentMask::W;
            }

            paramDesc.location = compiler.get_decoration(resource.id, spv::DecorationLocation);

            return paramDesc;
        }

    private:
        std::vector<ResourceDesc> m_resourceDescs;
        std::vector<ConstantBuffer> m_constantBuffers;

        std::vector<SignatureParameterDesc> m_inputParams;
        std::vector<SignatureParameterDesc> m_outputParams;

        PrimitiveTopology m_gsHSInputPrimitive = PrimitiveTopology::Undefined;
        PrimitiveTopology m_gsOutputTopology = PrimitiveTopology::Undefined;
        uint32_t m_gsMaxNumOutputVertices = 0;
        uint32_t m_gsNumInstances = 0;

        TessellatorOutputPrimitive m_hsOutputPrimitive = TessellatorOutputPrimitive::Undefined;
        TessellatorPartitioning m_hsPartitioning = TessellatorPartitioning::Undefined;
        TessellatorDomain m_hSDSTessellatorDomain = TessellatorDomain::Undefined;
        std::vector<SignatureParameterDesc> m_hsDSPatchConstantParams;
        uint32_t m_hsDSNumCtrlPoints = 0;

        uint32_t m_csBlockSizeX = 0;
        uint32_t m_csBlockSizeY = 0;
        uint32_t m_csBlockSizeZ = 0;

        std::vector<Function> m_funcs;
    };

    Reflection::Reflection() noexcept = default;

    Reflection::Reflection(const Reflection& other) : m_impl(other.m_impl ? new ReflectionImpl(*other.m_impl) : nullptr)
    {
    }

    Reflection::Reflection(Reflection&& other) noexcept : m_impl(std::move(other.m_impl))
    {
        other.m_impl = nullptr;
    }

    Reflection::~Reflection() noexcept
    {
        delete m_impl;
    }

    Reflection& Reflection::operator=(const Reflection& other)
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = nullptr;

            if (other.m_impl)
            {
                m_impl = new ReflectionImpl(*other.m_impl);
            }
        }
        return *this;
    }

    Reflection& Reflection::operator=(Reflection&& other) noexcept
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = std::move(other.m_impl);
            other.m_impl = nullptr;
        }
        return *this;
    }

    bool Reflection::Valid() const noexcept
    {
        return m_impl != nullptr;
    }

    uint32_t Reflection::NumResources() const noexcept
    {
        assert(Valid());
        return m_impl->NumResources();
    }

    const Reflection::ResourceDesc* Reflection::ResourceByIndex(uint32_t index) const noexcept
    {
        assert(Valid());
        return m_impl->ResourceByIndex(index);
    }

    const Reflection::ResourceDesc* Reflection::ResourceByName(const char* name) const noexcept
    {
        assert(Valid());
        return m_impl->ResourceByName(name);
    }

    uint32_t Reflection::NumConstantBuffers() const noexcept
    {
        assert(Valid());
        return m_impl->NumConstantBuffers();
    }

    const Reflection::ConstantBuffer* Reflection::ConstantBufferByIndex(uint32_t index) const noexcept
    {
        assert(Valid());
        return m_impl->ConstantBufferByIndex(index);
    }

    const Reflection::ConstantBuffer* Reflection::ConstantBufferByName(const char* name) const noexcept
    {
        assert(Valid());
        return m_impl->ConstantBufferByName(name);
    }

    uint32_t Reflection::NumInputParameters() const noexcept
    {
        assert(Valid());
        return m_impl->NumInputParameters();
    }

    const Reflection::SignatureParameterDesc* Reflection::InputParameter(uint32_t index) const noexcept
    {
        assert(Valid());
        return m_impl->InputParameter(index);
    }

    uint32_t Reflection::NumOutputParameters() const noexcept
    {
        assert(Valid());
        return m_impl->NumOutputParameters();
    }

    const Reflection::SignatureParameterDesc* Reflection::OutputParameter(uint32_t index) const noexcept
    {
        assert(Valid());
        return m_impl->OutputParameter(index);
    }

    Reflection::PrimitiveTopology Reflection::GSHSInputPrimitive() const noexcept
    {
        assert(Valid());
        return m_impl->GSHSInputPrimitive();
    }

    Reflection::PrimitiveTopology Reflection::GSOutputTopology() const noexcept
    {
        assert(Valid());
        return m_impl->GSOutputTopology();
    }

    uint32_t Reflection::GSMaxNumOutputVertices() const noexcept
    {
        assert(Valid());
        return m_impl->GSMaxNumOutputVertices();
    }

    uint32_t Reflection::GSNumInstances() const noexcept
    {
        assert(Valid());
        return m_impl->GSNumInstances();
    }

    Reflection::TessellatorOutputPrimitive Reflection::HSOutputPrimitive() const noexcept
    {
        assert(Valid());
        return m_impl->HSOutputPrimitive();
    }

    Reflection::TessellatorPartitioning Reflection::HSPartitioning() const noexcept
    {
        assert(Valid());
        return m_impl->HSPartitioning();
    }

    Reflection::TessellatorDomain Reflection::HSDSTessellatorDomain() const noexcept
    {
        assert(Valid());
        return m_impl->HSDSTessellatorDomain();
    }

    uint32_t Reflection::HSDSNumPatchConstantParameters() const noexcept
    {
        assert(Valid());
        return m_impl->HSDSNumPatchConstantParameters();
    }

    const Reflection::SignatureParameterDesc* Reflection::HSDSPatchConstantParameter(uint32_t index) const noexcept
    {
        assert(Valid());
        return m_impl->HSDSPatchConstantParameter(index);
    }

    uint32_t Reflection::HSDSNumConrolPoints() const noexcept
    {
        assert(Valid());
        return m_impl->HSDSNumConrolPoints();
    }

    uint32_t Reflection::CSBlockSizeX() const noexcept
    {
        assert(Valid());
        return m_impl->CSBlockSizeX();
    }

    uint32_t Reflection::CSBlockSizeY() const noexcept
    {
        assert(Valid());
        return m_impl->CSBlockSizeY();
    }

    uint32_t Reflection::CSBlockSizeZ() const noexcept
    {
        assert(Valid());
        return m_impl->CSBlockSizeZ();
    }

    uint32_t Reflection::NumFunctions() const noexcept
    {
        assert(Valid());
        return m_impl->NumFunctions();
    }

    const Reflection::Function* Reflection::FunctionByIndex(uint32_t index) const noexcept
    {
        assert(Valid());
        return m_impl->FunctionByIndex(index);
    }

    const Reflection::Function* Reflection::FunctionByName(const char* name) const noexcept
    {
        assert(Valid());
        return m_impl->FunctionByName(name);
    }
    
    // modify by johnson3d
    void* Reflection::GetD3D12ShaderReflection() const noexcept
    {
        return m_impl->m_shaderReflection.Get();
    }
    // end modify

    Compiler::ResultDesc Compiler::Compile(const SourceDesc& source, const Options& options, const TargetDesc& target)
    {
        ResultDesc result;
        Compiler::Compile(source, options, &target, 1, &result);
        return result;
    }

    void Compiler::Compile(const SourceDesc& source, const Options& options, const TargetDesc* targets, uint32_t numTargets,
                           ResultDesc* results)
    {
        SourceDesc sourceOverride = source;
        if (!sourceOverride.entryPoint || (std::strlen(sourceOverride.entryPoint) == 0))
        {
            sourceOverride.entryPoint = "main";
        }
        if (!sourceOverride.loadIncludeCallback)
        {
            sourceOverride.loadIncludeCallback = DefaultLoadCallback;
        }

        std::vector<std::tuple<TargetDesc, bool>> binaryTargets(numTargets);
        for (uint32_t i = 0; i < numTargets; ++i)
        {
            TargetDesc binaryTarget = targets[i];
            if (targets[i].language != ShadingLanguage::Dxil)
            {
                if (targets[i].language != ShadingLanguage::SpirV)
                {
                    binaryTarget.version = "15"; // Use SPIR-V 1.5 as the intermediate version
                }
                binaryTarget.language = ShadingLanguage::SpirV;
            }

            binaryTargets[i] = {std::move(binaryTarget), false};
        }

        for (uint32_t i = 0; i < numTargets; ++i)
        {
            if (!std::get<1>(binaryTargets[i]))
            {
                const auto& currBinaryTarget = std::get<0>(binaryTargets[i]);
                const auto binaryResult = CompileToBinary(sourceOverride, options, currBinaryTarget);

                for (uint32_t j = i; j < numTargets; ++j)
                {
                    const auto& binaryTarget = std::get<0>(binaryTargets[j]);
                    if ((j == i) ||
                        ((binaryTarget.language == currBinaryTarget.language) && (binaryTarget.asModule == currBinaryTarget.asModule) &&
                         ((binaryTarget.version == currBinaryTarget.version) ||
                          (std::strcmp(binaryTarget.version, currBinaryTarget.version) == 0))))
                    {
                        results[j] = ConvertBinary(binaryResult, sourceOverride, options, targets[j]);
                        std::get<1>(binaryTargets[j]) = true;
                    }
                }
            }
        }
    }

    Compiler::ResultDesc Compiler::Disassemble(const DisassembleDesc& source)
    {
        assert((source.language == ShadingLanguage::SpirV) || (source.language == ShadingLanguage::Dxil));

        Compiler::ResultDesc ret;

        ret.isText = true;

        if (source.language == ShadingLanguage::SpirV)
        {
            const uint32_t* spirvIr = reinterpret_cast<const uint32_t*>(source.binary.Data());
            const size_t spirvSize = source.binary.Size() / sizeof(uint32_t);

            spv_context context = spvContextCreate(SPV_ENV_UNIVERSAL_1_5);
            uint32_t options = SPV_BINARY_TO_TEXT_OPTION_NONE | SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES;
            spv_text text = nullptr;
            spv_diagnostic diagnostic = nullptr;

            spv_result_t error = spvBinaryToText(context, spirvIr, spirvSize, options, &text, &diagnostic);
            spvContextDestroy(context);

            if (error)
            {
                ret.errorWarningMsg.Reset(diagnostic->error, static_cast<uint32_t>(std::strlen(diagnostic->error)));
                ret.hasError = true;
                spvDiagnosticDestroy(diagnostic);
            }
            else
            {
                ret.target.Reset(text->str, static_cast<uint32_t>(std::strlen(text->str)));
                ret.hasError = false;
            }

            spvTextDestroy(text);
        }
        else
        {
            DxcBuffer sourceBuf;
            sourceBuf.Ptr = source.binary.Data();
            sourceBuf.Size = source.binary.Size();
            sourceBuf.Encoding = CP_UTF8;

            ComPtr<IDxcResult> disassemblyResult;
            TIFHR(Dxcompiler::Instance().Compiler().Disassemble(&sourceBuf, __uuidof(IDxcResult), disassemblyResult.PutVoid()));

            ConvertDxcResult(ret, disassemblyResult.Get(), source.language, false, false);
        }

        return ret;
    }

    bool Compiler::LinkSupport() noexcept
    {
        return Dxcompiler::Instance().LinkerSupport();
    }

    Compiler::ResultDesc Compiler::Link(const LinkDesc& modules, const Compiler::Options& options, const TargetDesc& target)
    {
        auto linker = Dxcompiler::Instance().CreateLinker();
        TIFFALSE(linker);

        auto& utils = Dxcompiler::Instance().Utils();

        std::vector<std::wstring> moduleNames(modules.numModules);
        std::vector<const wchar_t*> moduleNamesWide(modules.numModules);
        std::vector<ComPtr<IDxcBlobEncoding>> moduleBlobs(modules.numModules);
        for (uint32_t i = 0; i < modules.numModules; ++i)
        {
            TIFFALSE(modules.modules[i] != nullptr);

            TIFHR(utils.CreateBlob(modules.modules[i]->target.Data(), modules.modules[i]->target.Size(), CP_UTF8, moduleBlobs[i].Put()));
            TIFFALSE(moduleBlobs[i]->GetBufferSize() >= 4);

            moduleNames[i] = Utf8ToWide(modules.modules[i]->name);
            moduleNamesWide[i] = moduleNames[i].c_str();
            TIFHR(linker->RegisterLibrary(moduleNamesWide[i], moduleBlobs[i].Get()));
        }

        const std::wstring entryPointWide = Utf8ToWide(modules.entryPoint);

        const std::wstring shaderProfile = ShaderProfileName(modules.stage, options.shaderModel, false);
        ComPtr<IDxcOperationResult> linkOpResult;
        TIFHR(linker->Link(entryPointWide.c_str(), shaderProfile.c_str(), moduleNamesWide.data(),
                           static_cast<UINT32>(moduleNamesWide.size()), nullptr, 0, linkOpResult.Put()));

        ComPtr<IDxcResult> linkResult = linkOpResult.As<IDxcResult>();

        Compiler::ResultDesc binaryResult{};
        ConvertDxcResult(binaryResult, linkResult.Get(), ShadingLanguage::Dxil, false, options.needReflection);

        Compiler::SourceDesc source{};
        source.entryPoint = modules.entryPoint;
        source.stage = modules.stage;
        return ConvertBinary(binaryResult, source, options, target);
    }
} // namespace ShaderConductor

namespace
{
    Reflection MakeDxilReflection(IDxcBlob* dxilBlob, bool asModule)
    {
        return Reflection::ReflectionImpl::Make(dxilBlob, asModule);
    }

    Reflection MakeSpirVReflection(const spirv_cross::Compiler& compiler)
    {
        return Reflection::ReflectionImpl::Make(compiler);
    }

    template <typename T>
    void ExtractDxilResourceDesc(std::vector<Reflection::ResourceDesc>& resourceDescs,
                                 std::vector<Reflection::ConstantBuffer>& constantBuffers, T* d3d12Reflection, uint32_t resourceIndex)
    {
        D3D12_SHADER_INPUT_BIND_DESC bindDesc;
        d3d12Reflection->GetResourceBindingDesc(resourceIndex, &bindDesc);

        Reflection::ResourceDesc reflectionDesc{};

        std::strncpy(reflectionDesc.name, bindDesc.Name, sizeof(reflectionDesc.name));
        reflectionDesc.name[sizeof(reflectionDesc.name) - 1] = '\0';
        reflectionDesc.space = bindDesc.Space;
        reflectionDesc.bindPoint = bindDesc.BindPoint;
        reflectionDesc.bindCount = bindDesc.BindCount;

        if (bindDesc.Type == D3D_SIT_CBUFFER || bindDesc.Type == D3D_SIT_TBUFFER)
        {
            reflectionDesc.type = ShaderResourceType::ConstantBuffer;

            ID3D12ShaderReflectionConstantBuffer* d3d12CBuffer = d3d12Reflection->GetConstantBufferByName(bindDesc.Name);
            constantBuffers.emplace_back(Reflection::ReflectionImpl::Make(d3d12CBuffer));
        }
        else
        {
            switch (bindDesc.Type)
            {
            case D3D_SIT_TEXTURE:
                reflectionDesc.type = ShaderResourceType::Texture;
                break;

            case D3D_SIT_SAMPLER:
                reflectionDesc.type = ShaderResourceType::Sampler;
                break;

            case D3D_SIT_STRUCTURED:
            case D3D_SIT_BYTEADDRESS:
                reflectionDesc.type = ShaderResourceType::ShaderResourceView;
                break;

            case D3D_SIT_UAV_RWTYPED:
            case D3D_SIT_UAV_RWSTRUCTURED:
            case D3D_SIT_UAV_RWBYTEADDRESS:
            case D3D_SIT_UAV_APPEND_STRUCTURED:
            case D3D_SIT_UAV_CONSUME_STRUCTURED:
            case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
                reflectionDesc.type = ShaderResourceType::UnorderedAccessView;
                break;

            default:
                SC_UNREACHABLE("Unknown bind type.");
            }
        }

        resourceDescs.emplace_back(std::move(reflectionDesc));
    }
} // namespace

#ifdef _WIN32
BOOL WINAPI DllMain([[maybe_unused]] HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    BOOL result = TRUE;
    if (reason == DLL_PROCESS_DETACH)
    {
        Dxcompiler::DllDetaching(true);

        if (reserved == 0)
        {
            // FreeLibrary has been called or the DLL load failed
            Dxcompiler::Instance().Destroy();
        }
        else
        {
            // Process termination. We should not call FreeLibrary()
            Dxcompiler::Instance().Terminate();
        }
    }

    return result;
}
#endif
