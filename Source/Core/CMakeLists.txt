# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

if(WIN32)
    set(dxcompilerDeployDir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}")
else()
    set(dxcompilerDeployDir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()

add_custom_target(DeployDxcompiler ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${dxcompilerDeployDir}
    COMMAND ${CMAKE_COMMAND} -E create_hardlink
        $<TARGET_FILE:DxcPackage>
        ${dxcompilerDeployDir}/$<TARGET_FILE_NAME:DxcPackage>
    COMMENT "Deploying dxcompiler..."
)
add_dependencies(DeployDxcompiler DxcPackage)

set_target_properties(DeployDxcompiler PROPERTIES FOLDER "Core")


add_library(ShaderConductor "SHARED"
    ${PROJECT_SOURCE_DIR}/Source/Core/ShaderConductor.cpp
    ${PROJECT_SOURCE_DIR}/Include/ShaderConductor/ShaderConductor.hpp
)

target_include_directories(ShaderConductor
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Include

    PRIVATE
        $<TARGET_PROPERTY:DxcPackage,INTERFACE_INCLUDE_DIRECTORIES>
)
if(MSVC)
    target_compile_definitions(ShaderConductor
        PRIVATE
            -D_CRT_SECURE_NO_WARNINGS
    )
endif()
target_link_libraries(ShaderConductor
    PRIVATE
        DirectX-Headers
        spirv-cross-core
        spirv-cross-glsl
        spirv-cross-hlsl
        spirv-cross-msl
        spirv-cross-util
        SPIRV-Tools
)
if(NOT WIN32)
    target_link_libraries(ShaderConductor
        PRIVATE
            LLVMDxcSupport
            dl
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(ShaderConductor
            PRIVATE
                stdc++fs
        )
    endif()
endif()

add_dependencies(ShaderConductor DeployDxcompiler)

set_target_properties(ShaderConductor PROPERTIES FOLDER "Core")
