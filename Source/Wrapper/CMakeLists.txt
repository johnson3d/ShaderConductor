# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

add_library(ShaderConductorWrapper SHARED
    Native.cpp
    Native.h
)

target_link_libraries(ShaderConductorWrapper
    PRIVATE
        ShaderConductor
)

set_target_properties(ShaderConductorWrapper PROPERTIES FOLDER "Wrapper")

set(DATA_FILES
    shader.hlsl
)

set_source_files_properties(${DATA_FILES} PROPERTIES
    VS_TOOL_OVERRIDE "None"
    VS_COPY_TO_OUT_DIR "PreserveNewest"
)

add_executable(CSharpPinvoke
    Program.cs
    Wrapper.cs
    ${DATA_FILES}
)
add_dependencies(CSharpPinvoke ShaderConductorWrapper)

set_target_properties(CSharpPinvoke PROPERTIES FOLDER "Wrapper")
