# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(DATA_INC_FILES
    Data/Input/Inc/HeaderA.hlsli
    Data/Input/Inc/HeaderB.hlsli
    Data/Input/Inc/HeaderEmpty.hlsli
)

set(DATA_FILES
    Data/Input/CalcLight.hlsl
    Data/Input/CalcLightDiffuse.hlsl
    Data/Input/CalcLightDiffuseSpecular.hlsl
    Data/Input/Common.hlsli
    Data/Input/Constant_PS.hlsl
    Data/Input/Constant_VS.hlsl
    Data/Input/DetailTessellation_HS.hlsl
    Data/Input/Fluid_CS.hlsl
    Data/Input/IncludeEmptyHeader.hlsl
    Data/Input/IncludeExist.hlsl
    Data/Input/IncludeNotExist.hlsl
    Data/Input/HalfDataType.hlsl
    Data/Input/Particle_GS.hlsl
    Data/Input/PassThrough_PS.hlsl
    Data/Input/PassThrough_VS.hlsl
    Data/Input/PNTriangles_DS.hlsl
    Data/Input/Subpass_PS.hlsl
    Data/Input/ToneMapping_PS.hlsl
    Data/Input/Transform_VS.hlsl
)

set_source_files_properties(${DATA_FILES}
    PROPERTIES VS_TOOL_OVERRIDE "None"
)

source_group("Data Files\\Input" FILES ${DATA_FILES})
source_group("Data Files\\Input\\Inc" FILES ${DATA_INC_FILES})

add_executable(ShaderConductorTest
    Common.hpp
    Common.cpp
    CrossCompileTest.cpp
    ReflectionTest.cpp
    ${DATA_FILES}
    ${DATA_INC_FILES}
)

target_compile_definitions(ShaderConductorTest
    PRIVATE
        -DTEST_DATA_DIR="${PROJECT_SOURCE_DIR}/Source/Tests/Data/"
)
target_link_libraries(ShaderConductorTest
    PRIVATE
        ShaderConductor
        gtest
)

set_target_properties(ShaderConductorTest PROPERTIES FOLDER "Tests")
