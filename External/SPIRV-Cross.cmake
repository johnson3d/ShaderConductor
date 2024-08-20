# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(SPIRV_Cross_REV "54997fb4bc3adeb47b9b9f7bb67f1c25eaca2204")

UpdateExternalLib("SPIRV-Cross" "https://github.com/KhronosGroup/SPIRV-Cross.git" ${SPIRV_Cross_REV})

set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)

add_subdirectory(SPIRV-Cross EXCLUDE_FROM_ALL)

set_target_properties(spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl spirv-cross-reflect
    spirv-cross-util PROPERTIES FOLDER "External/SPIRV-Cross")
