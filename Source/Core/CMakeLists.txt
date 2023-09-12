# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

if(WIN32)
    set(dxcompilerName "dxcompiler.dll")
    set(dxcompilerDeployDir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}")
else()
    if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(dxcompilerName "libdxcompiler.dylib")
    else()
        set(dxcompilerName "libdxcompiler.so")
    endif()
    set(dxcompilerDeployDir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.19") 
    set(operation "create_hardlink")
else()
    set(operation "copy")
endif()

add_custom_target(DeployDxcompiler ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${dxcompilerDeployDir}
    COMMAND ${CMAKE_COMMAND} -E ${operation}
        ${dxcompilerBinary}
        ${dxcompilerDeployDir}/${dxcompilerName}
    COMMENT "Deploying dxcompiler...")
if(NOT dxcompileUsePrebuilt)
    add_dependencies(DeployDxcompiler dxcompiler)
endif()

set_target_properties(DeployDxcompiler PROPERTIES FOLDER "Core")


set(LIB_NAME ShaderConductor)

set(SOURCE_FILES
    ${SC_ROOT_DIR}/Source/Core/ShaderConductor.cpp
)

set(HEADER_FILES
    ${SC_ROOT_DIR}/Include/ShaderConductor/ShaderConductor.hpp
)

source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Header Files" FILES ${HEADER_FILES})

add_library(${LIB_NAME} "SHARED"
    ${SOURCE_FILES} ${HEADER_FILES}
)

if(NOT dxcompileUsePrebuilt)
    target_include_directories(${LIB_NAME}
        PRIVATE
            ${dxcompilerIncludeDir}
    )
endif()
target_include_directories(${LIB_NAME}
    PUBLIC
        ${SC_ROOT_DIR}/Include

    PRIVATE
        ${SC_BUILD_DIR}/External/DirectXShaderCompiler/include
        ${SC_ROOT_DIR}/External/DirectXShaderCompiler/include
)
if(MSVC)
    target_compile_definitions(${LIB_NAME}
        PRIVATE
            -D_CRT_SECURE_NO_DEPRECATE
            -D_CRT_SECURE_NO_WARNINGS
    )
endif()
target_link_libraries(${LIB_NAME}
    PRIVATE
        DirectX-Headers
        spirv-cross-core
        spirv-cross-glsl
        spirv-cross-hlsl
        spirv-cross-msl
        spirv-cross-util
        SPIRV-Tools
)
if(NOT MSVC)
    target_link_libraries(${LIB_NAME}
        PRIVATE
            LLVMDxcSupport
            dl
    )
endif()

add_dependencies(${LIB_NAME} DeployDxcompiler)

set_target_properties(${LIB_NAME} PROPERTIES FOLDER "Core")
