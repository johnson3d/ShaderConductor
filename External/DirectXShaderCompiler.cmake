# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

if(NOT (SC_PREBUILT_DXC_URL STREQUAL ""))
    if(SC_PREBUILT_DXC_URL STREQUAL "AUTO")
        set(prebuiltDxcPrefix "https://artprodeus21.artifacts.visualstudio.com/A73ee8395-692b-4b30-bcb0-4fe7d5297417/29a70e0e-8840-4d3f-b249-ba73d30ad448/_apis/artifact/")
        set(prebuiltDxcUrl_Windows_x64 "${prebuiltDxcPrefix}cGlwZWxpbmVhcnRpZmFjdDovL2dvbmdtaW5taW4vcHJvamVjdElkLzI5YTcwZTBlLTg4NDAtNGQzZi1iMjQ5LWJhNzNkMzBhZDQ0OC9idWlsZElkLzE0ODIvYXJ0aWZhY3ROYW1lL2R4Yy1hcnRpZmFjdHMtV2luZG93c19WUzIwMjJfeDY0X1JlbGVhc2U1/content?format=zip")
        set(prebuiltDxcName_Windows_x64 "dxc-artifacts-Windows_VS2022_x64_Release")
        set(prebuiltDxcUrl_Windows_arm64 "${prebuiltDxcPrefix}cGlwZWxpbmVhcnRpZmFjdDovL2dvbmdtaW5taW4vcHJvamVjdElkLzI5YTcwZTBlLTg4NDAtNGQzZi1iMjQ5LWJhNzNkMzBhZDQ0OC9idWlsZElkLzE0ODIvYXJ0aWZhY3ROYW1lL2R4Yy1hcnRpZmFjdHMtV2luZG93c19WUzIwMjJfYXJtNjRfUmVsZWFzZQ2/content?format=zip")
        set(prebuiltDxcName_Windows_arm64 "dxc-artifacts-Windows_VS2022_arm64_Release")
        set(prebuiltDxcUrl_Ubuntu_20_clang "${prebuiltDxcPrefix}cGlwZWxpbmVhcnRpZmFjdDovL2dvbmdtaW5taW4vcHJvamVjdElkLzI5YTcwZTBlLTg4NDAtNGQzZi1iMjQ5LWJhNzNkMzBhZDQ0OC9idWlsZElkLzE0ODIvYXJ0aWZhY3ROYW1lL2R4Yy1hcnRpZmFjdHMtVWJ1bnR1XzIwX0NsYW5nXzEwX3g2NF9SZWxlYXNl0/content?format=zip")
        set(prebuiltDxcName_Ubuntu_20_clang "dxc-artifacts-Ubuntu_20_Clang_10_x64_Release")
        set(prebuiltDxcUrl_Ubuntu_22_clang "${prebuiltDxcPrefix}cGlwZWxpbmVhcnRpZmFjdDovL2dvbmdtaW5taW4vcHJvamVjdElkLzI5YTcwZTBlLTg4NDAtNGQzZi1iMjQ5LWJhNzNkMzBhZDQ0OC9idWlsZElkLzE0ODIvYXJ0aWZhY3ROYW1lL2R4Yy1hcnRpZmFjdHMtVWJ1bnR1XzIyX0NsYW5nXzE0X3g2NF9SZWxlYXNl0/content?format=zip")
        set(prebuiltDxcName_Ubuntu_22_clang "dxc-artifacts-Ubuntu_22_Clang_14_x64_Release")
        set(prebuiltDxcUrl_MacOS_11_clang "${prebuiltDxcPrefix}cGlwZWxpbmVhcnRpZmFjdDovL2dvbmdtaW5taW4vcHJvamVjdElkLzI5YTcwZTBlLTg4NDAtNGQzZi1iMjQ5LWJhNzNkMzBhZDQ0OC9idWlsZElkLzE0ODIvYXJ0aWZhY3ROYW1lL2R4Yy1hcnRpZmFjdHMtTWFjT1NfMTFfQ2xhbmdfMTFfeDY0X1JlbGVhc2U1/content?format=zip")
        set(prebuiltDxcName_MacOS_11_clang "dxc-artifacts-MacOS_11_Clang_11_x64_Release")

        if(WIN32)
            if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "x64")
                set(prebuiltDxcUrl ${prebuiltDxcUrl_Windows_x64})
                set(prebuiltDxcName ${prebuiltDxcName_Windows_x64})
            else()
                set(prebuiltDxcUrl ${prebuiltDxcUrl_Windows_arm64})
                set(prebuiltDxcName ${prebuiltDxcName_Windows_arm64})
            endif()
        elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
            set(prebuiltDxcUrl ${prebuiltDxcUrl_MacOS_11_clang})
            set(prebuiltDxcName ${prebuiltDxcName_MacOS_11_clang})
        else()
            if(CMAKE_SYSTEM_VERSION VERSION_GREATER_EQUAL 6.0)
                set(prebuiltDxcUrl ${prebuiltDxcUrl_Ubuntu_22_clang})
                set(prebuiltDxcName ${prebuiltDxcName_Ubuntu_22_clang})
            else()
                set(prebuiltDxcUrl ${prebuiltDxcUrl_Ubuntu_20_clang})
                set(prebuiltDxcName ${prebuiltDxcName_Ubuntu_20_clang})
            endif()
        endif()
    else()
        set(prebuiltDxcUrl ${SC_PREBUILT_DXC_URL})
        set(prebuiltDxcName ${SC_PREBUILT_DXC_NAME})
    endif()

    set(prebuiltDxcTmpDir "${CMAKE_CURRENT_BINARY_DIR}/PrebuiltDxc")
    set(prebuiltDxcPath "${prebuiltDxcTmpDir}/${prebuiltDxcName}.zip")
    if(NOT EXISTS ${prebuiltDxcPath})
        file(DOWNLOAD ${prebuiltDxcUrl} ${prebuiltDxcPath} SHOW_PROGRESS)
    endif()

    set(prebuiltDxcDir "${prebuiltDxcTmpDir}/${prebuiltDxcName}")
    if (NOT EXISTS ${prebuiltDxcDir})
        file(ARCHIVE_EXTRACT INPUT "${prebuiltDxcPath}"
            DESTINATION "${prebuiltDxcTmpDir}"
        )
    endif()

    set(SC_PREBUILT_DXC_DIR ${prebuiltDxcDir})
endif()

if(WIN32)
    set(dxcompilerName "dxcompiler.dll")
    set(dxcompilerLibDir "bin")
else()
    if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(dxcompilerName "libdxcompiler.dylib")
    else()
        set(dxcompilerName "libdxcompiler.so")
    endif()
    set(dxcompilerLibDir "lib")
endif()

set(usePrebuilt FALSE)
if(SC_PREBUILT_DXC_DIR AND (IS_DIRECTORY ${SC_PREBUILT_DXC_DIR}))
    find_file(prebuiltDxcBinary
        ${dxcompilerName}
        PATHS ${SC_PREBUILT_DXC_DIR}/${dxcompilerLibDir}
        NO_DEFAULT_PATH
    )
    find_path(prebuiltDxcIncludeDir
        dxc/dxcapi.h
        PATHS ${SC_PREBUILT_DXC_DIR}/include
        NO_DEFAULT_PATH
    )
    if(prebuiltDxcBinary AND prebuiltDxcIncludeDir)
        set(usePrebuilt TRUE)
    else()
        if(NOT prebuiltDxcBinary)
            message(WARNING "Can't find dxc binaries in ${SC_PREBUILT_DXC_DIR}.")
        endif()
        if(NOT prebuiltDxcIncludeDir)
            message(WARNING "Can't find dxc headers in ${SC_PREBUILT_DXC_DIR}.")
        endif()
    endif()
endif()

if(usePrebuilt)
    set(dxcPackageIncludeDir ${prebuiltDxcIncludeDir})
    set(dxcPackageBinary ${prebuiltDxcBinary})

    message(STATUS "Using prebuilt dxc binary from ${prebuiltDxcBinary}.")
else()
    set(dxcPackageIncludeDir
        ${CMAKE_CURRENT_BINARY_DIR}/DirectXShaderCompiler/include
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectXShaderCompiler/include
    )
    set(dxcPackageBinary ${CMAKE_CURRENT_BINARY_DIR}/DirectXShaderCompiler/${CMAKE_CFG_INTDIR}/${dxcompilerLibDir}/${dxcompilerName})
endif()

add_library(DxcPackage SHARED IMPORTED GLOBAL)
target_include_directories(DxcPackage
    INTERFACE
        ${dxcPackageIncludeDir}
)
set_target_properties(DxcPackage PROPERTIES
    IMPORTED_LOCATION ${dxcPackageBinary}
)

if(usePrebuilt)
    # The prebuilt package is complete. No need to sync the repository of DirectXShaderCompiler.
    return()
else()
    add_dependencies(DxcPackage dxcompiler)
endif()

set(DirectXShaderCompiler_REV "18328d51057863c0d643a23d3b72ee77912d4b54")

UpdateExternalLib("DirectXShaderCompiler" "https://github.com/Microsoft/DirectXShaderCompiler.git" ${DirectXShaderCompiler_REV} need_patch)
if(need_patch)
    foreach(patch "0001-Enable-to-set-the-location-of-DirectX-Headers" "0002-Fix-compilation-issues-on-clang" "0003-Fix-build-on-ARM-with-non-VS-generator")
        ApplyPatch("DirectXShaderCompiler" "${CMAKE_CURRENT_SOURCE_DIR}/Patches/${patch}.patch")
    endforeach()
endif()

set(ENABLE_SPIRV_CODEGEN ON CACHE BOOL "" FORCE)
set(ENABLE_DXIL2SPV OFF CACHE BOOL "" FORCE)
set(CLANG_ENABLE_ARCMT OFF CACHE BOOL "" FORCE)
set(CLANG_ENABLE_STATIC_ANALYZER OFF CACHE BOOL "" FORCE)
set(CLANG_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(HLSL_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(HLSL_BUILD_DXILCONV OFF CACHE BOOL "" FORCE)
set(HLSL_SUPPORT_QUERY_GIT_COMMIT_INFO OFF CACHE BOOL "" FORCE)
set(HLSL_ENABLE_DEBUG_ITERATORS ON CACHE BOOL "" FORCE)
set(LLVM_TARGETS_TO_BUILD "None" CACHE STRING "" FORCE)
set(LLVM_INCLUDE_DOCS OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBCLANG_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(LLVM_OPTIMIZED_TABLEGEN OFF CACHE BOOL "" FORCE)
set(LLVM_REQUIRES_EH ON CACHE BOOL "" FORCE)
set(LLVM_APPEND_VC_REV ON CACHE BOOL "" FORCE)
set(LLVM_ENABLE_RTTI ON CACHE BOOL "" FORCE)
set(LLVM_ENABLE_EH ON CACHE BOOL "" FORCE)
set(LLVM_ENABLE_TERMINFO OFF CACHE BOOL "" FORCE)
set(LLVM_DEFAULT_TARGET_TRIPLE "dxil-ms-dx" CACHE STRING "" FORCE)
set(CLANG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLVM_REQUIRES_RTTI ON CACHE BOOL "" FORCE)
set(CLANG_CL OFF CACHE BOOL "" FORCE)
set(SPIRV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)

add_subdirectory(DirectXShaderCompiler EXCLUDE_FROM_ALL)
foreach(target
    "clang" "dxc"
    "clangAnalysis" "clangAST" "clangASTMatchers" "clangBasic" "clangCodeGen" "clangDriver" "clangEdit" "clangFormat" "clangFrontend"
    "clangFrontendTool" "clangIndex" "clangLex" "clangParse" "clangRewrite" "clangRewriteFrontend" "clangSema" "clangSPIRV" "clangTooling"
    "clangToolingCore" "dxcompiler" "libclang"
    "ClangAttrClasses" "ClangAttrDump" "ClangAttrHasAttributeImpl" "ClangAttrImpl" "ClangAttrList" "ClangAttrParsedAttrImpl"
    "ClangAttrParsedAttrKinds" "ClangAttrParsedAttrList" "ClangAttrParserStringSwitches" "ClangAttrPCHRead" "ClangAttrPCHWrite"
    "ClangAttrSpellingListIndex" "ClangAttrTemplateInstantiate" "ClangAttrVisitor" "ClangCommentCommandInfo" "ClangCommentCommandList"
    "ClangCommentHTMLNamedCharacterReferences" "ClangCommentHTMLTags" "ClangCommentHTMLTagsProperties" "ClangCommentNodes" "ClangDeclNodes"
    "ClangDiagnosticAnalysis" "ClangDiagnosticAST" "ClangDiagnosticComment" "ClangDiagnosticCommon" "ClangDiagnosticDriver"
    "ClangDiagnosticFrontend" "ClangDiagnosticGroups" "ClangDiagnosticIndexName" "ClangDiagnosticLex" "ClangDiagnosticParse"
    "ClangDiagnosticSema" "ClangDiagnosticSerialization" "ClangStmtNodes"
    "DxcDisassembler" "DxcOptimizer" "DxilConstants" "DxilCounters" "DxilDocs" "DxilInstructions" "DxilIntrinsicTables"
    "DxilMetadata" "DxilOperations" "DxilPIXPasses" "DxilShaderModel" "DxilShaderModelInc" "DxilSigPoint" "DxilValidation" "DxilValidationInc"
    "HCTGen" "HLSLIntrinsicOp" "HLSLOptions"
    "LLVMAnalysis" "LLVMAsmParser" "LLVMBitReader" "LLVMBitWriter" "LLVMCore" "LLVMDxcBindingTable" "LLVMDxcSupport" "LLVMDXIL" "LLVMDxilCompression" "LLVMDxilContainer"
    "LLVMDxilPIXPasses" "LLVMDxilRootSignature" "LLVMDxrFallback" "LLVMHLSL" "LLVMInstCombine" "LLVMipa" "LLVMipo" "LLVMIRReader"
    "LLVMLinker" "LLVMMSSupport" "LLVMOption" "LLVMPasses" "LLVMPassPrinters" "LLVMProfileData" "LLVMScalarOpts" "LLVMSupport"
    "LLVMTableGen" "LLVMTarget" "LLVMTransformUtils" "LLVMVectorize"
    "ClangDriverOptions" "DxcEtw" "intrinsics_gen" "TablegenHLSLOptions"
    "clang-tblgen" "llvm-tblgen" "hlsl_dxcversion_autogen" "hlsl_version_autogen" "RDAT_LibraryTypes")
    get_target_property(vsFolder ${target} FOLDER)
    if(NOT vsFolder)
        set(vsFolder "")
    endif()
    set_target_properties(${target} PROPERTIES FOLDER "External/DirectXShaderCompiler/${vsFolder}")
endforeach()
if(WIN32)
    foreach(target
        "dndxc" "dxa" "dxl" "dxopt" "dxr" "dxv"
        "d3dcompiler_dxc_bridge" "dxlib_sample" "dxrfallbackcompiler"
        "dxexp" "LLVMDxilDia")
        get_target_property(vsFolder ${target} FOLDER)
        if(NOT vsFolder)
            set(vsFolder "")
        endif()
        set_target_properties(${target} PROPERTIES FOLDER "External/DirectXShaderCompiler/${vsFolder}")
    endforeach()
endif()
