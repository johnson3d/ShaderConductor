# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

if(WIN32)
    set(dxcompilerSrcName "dxcompiler.dll")
    set(dxcompilerDstName ${dxcompilerSrcName})
    set(dxcompilerOutputDir "bin")
    set(dxcompilerDeployDir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}")
else()
    if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(dxcompilerSrcName "libdxcompiler.3.7.dylib")
        set(dxcompilerDstName "libdxcompiler.dylib")
    else()
        set(dxcompilerSrcName "libdxcompiler.so.3.7")
        set(dxcompilerDstName "libdxcompiler.so")
    endif()
    set(dxcompilerOutputDir "lib")
    set(dxcompilerDeployDir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()

add_custom_target(DeployDxcompiler ALL
    COMMAND ${CMAKE_COMMAND} -E create_hardlink
        ${SC_BUILD_DIR}/External/DirectXShaderCompiler/${CMAKE_CFG_INTDIR}/${dxcompilerOutputDir}/${dxcompilerSrcName}
        ${dxcompilerDeployDir}/${dxcompilerDstName}
    COMMENT "Deploying dxcompiler...")
add_dependencies(DeployDxcompiler dxcompiler)

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

target_include_directories(${LIB_NAME}
    PUBLIC
        ${SC_ROOT_DIR}/Include

    PRIVATE
        ${SC_BUILD_DIR}/External/DirectXShaderCompiler/include
        ${SC_ROOT_DIR}/External/DirectXShaderCompiler/include
)
target_compile_definitions(${LIB_NAME}
    PRIVATE
        -DSHADER_CONDUCTOR_SOURCE
)
if(MSVC)
    target_compile_definitions(${LIB_NAME}
        PRIVATE
            -D_CRT_SECURE_NO_DEPRECATE
            -D_CRT_SECURE_NO_WARNINGS
            -D_SCL_SECURE_NO_DEPRECATE
            -D_SCL_SECURE_NO_WARNINGS
            -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
    )
endif()
target_link_libraries(${LIB_NAME}
    PRIVATE
        LLVMDxcSupport
        LLVMSupport
        spirv-cross-core
        spirv-cross-glsl
        spirv-cross-hlsl
        spirv-cross-msl
        spirv-cross-util
        SPIRV-Tools
)

add_dependencies(${LIB_NAME} spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl)
add_dependencies(${LIB_NAME} DeployDxcompiler)
add_dependencies(${LIB_NAME} SPIRV-Tools)

set_target_properties(${LIB_NAME} PROPERTIES FOLDER "Core")
