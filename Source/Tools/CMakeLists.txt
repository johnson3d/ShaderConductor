# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

add_executable(ShaderConductorCmd
    ShaderConductorCmd.cpp
)

target_link_libraries(ShaderConductorCmd
    PRIVATE
        ShaderConductor
        cxxopts
)

set_target_properties(ShaderConductorCmd PROPERTIES FOLDER "Tools")
