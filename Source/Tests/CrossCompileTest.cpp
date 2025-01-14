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

#include "Common.hpp"

#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

using namespace ShaderConductor;

namespace
{
    Compiler::ResultDesc Disassemble(ShadingLanguage language, const Compiler::ResultDesc& result)
    {
        Compiler::DisassembleDesc disasmDesc;
        disasmDesc.language = language;
        disasmDesc.binary = result.target;
        return Compiler::Disassemble(disasmDesc);
    }

    void HlslToAnyTest(const std::string& name, const Compiler::SourceDesc& source, const Compiler::Options& options,
                       const std::vector<Compiler::TargetDesc>& targets, const std::vector<bool>& expectSuccessFlags)
    {
        static const std::string extMap[] = {"dxilasm", "spvasm", "hlsl", "glsl", "essl", "msl", "msl"};
        static_assert(sizeof(extMap) / sizeof(extMap[0]) == static_cast<uint32_t>(ShadingLanguage::NumShadingLanguages),
                      "extMap doesn't match with the number of shading languages.");

        std::vector<Compiler::ResultDesc> results(targets.size());
        Compiler::Compile(source, options, targets.data(), static_cast<uint32_t>(targets.size()), results.data());
        for (size_t i = 0; i < targets.size(); ++i)
        {
            if (expectSuccessFlags[i])
            {
                auto& result = results[i];

                EXPECT_FALSE(result.hasError);

                if ((targets[i].language == ShadingLanguage::Dxil) || (targets[i].language == ShadingLanguage::SpirV))
                {
                    EXPECT_FALSE(result.isText);
                    result = Disassemble(targets[i].language, result);
                }

                EXPECT_FALSE(result.hasError);
                EXPECT_EQ(result.errorWarningMsg.Size(), 0U);
                EXPECT_TRUE(result.isText);

                std::string compareName = name;
                if (targets[i].version != nullptr)
                {
                    compareName += "." + std::string(targets[i].version);
                }
                compareName += "." + extMap[static_cast<uint32_t>(targets[i].language)];

                const uint8_t* targetPtr = reinterpret_cast<const uint8_t*>(result.target.Data());
                std::vector<uint8_t> actualTarget(targetPtr, targetPtr + result.target.Size());
                if (targets[i].language == ShadingLanguage::Dxil)
                {
                    RemoveDxilAsmHash(actualTarget);
                }

                CompareWithExpected(actualTarget, result.isText, compareName);
            }
            else
            {
                const auto& result = results[i];
                EXPECT_TRUE(result.hasError);
                EXPECT_EQ(result.target.Size(), 0U);
            }
        }
    }

    Compiler::ModuleDesc CompileToModule(const char* moduleName, const std::string& inputFileName, const Compiler::Options& options,
                                         const Compiler::TargetDesc& target)
    {
        std::vector<uint8_t> input = LoadFile(inputFileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        auto result = Compiler::Compile({source.c_str(), inputFileName.c_str(), "", ShaderStage::PixelShader}, options, target);

        EXPECT_FALSE(result.hasError);
        EXPECT_FALSE(result.isText);

        return {moduleName, std::move(result.target)};
    }

    class TestBase : public testing::Test
    {
    public:
        TestBase()
        {
            m_expectSuccessFlags.assign(m_testTargets.size(), true);
        }

        void SetUp() override
        {
            for (auto& src : m_testSources)
            {
                const std::string& name = std::get<0>(src);
                Compiler::SourceDesc& source = std::get<1>(src);

                std::get<2>(src) = TEST_DATA_DIR "Input/" + name + ".hlsl";
                source.fileName = std::get<2>(src).c_str();

                std::vector<uint8_t> input = LoadFile(source.fileName, true);
                std::get<3>(src) = std::string(reinterpret_cast<char*>(input.data()), input.size());
                source.source = std::get<3>(src).c_str();
            }
        }

        void RunTests(ShadingLanguage targetSl, const Compiler::Options& options = {})
        {
            for (const auto& combination : m_testSources)
            {
                std::vector<Compiler::TargetDesc> targetSubset;
                std::vector<bool> expectSuccessSubset;
                for (size_t i = 0; i < m_testTargets.size(); ++i)
                {
                    if (m_testTargets[i].language == targetSl)
                    {
                        targetSubset.push_back(m_testTargets[i]);
                        expectSuccessSubset.push_back(m_expectSuccessFlags[i]);
                    }
                }

                HlslToAnyTest(std::get<0>(combination), std::get<1>(combination), options, targetSubset, expectSuccessSubset);
            }
        }

    protected:
        // test name, source desc, input file name, input source
        std::vector<std::tuple<std::string, Compiler::SourceDesc, std::string, std::string>> m_testSources;

        // clang-format off
        const std::vector<Compiler::TargetDesc> m_testTargets =
        {
            { ShadingLanguage::Dxil },

            { ShadingLanguage::SpirV, "15" },

            { ShadingLanguage::Hlsl, "30" },
            { ShadingLanguage::Hlsl, "40" },
            { ShadingLanguage::Hlsl, "50" },

            { ShadingLanguage::Glsl, "300" },
            { ShadingLanguage::Glsl, "410" },

            { ShadingLanguage::Essl, "300" },
            { ShadingLanguage::Essl, "310" },

            { ShadingLanguage::Msl_macOS },
        };
        // clang-format on

        std::vector<bool> m_expectSuccessFlags;
    };

    class VertexShaderTest : public TestBase
    {
    public:
        void SetUp() override
        {
            // clang-format off
            m_testSources =
            {
                {
                    "Constant_VS",
                    { "", "", "VSMain", ShaderStage::VertexShader },
                    "",
                    ""
                },
                {
                    "PassThrough_VS",
                    { "", "", "VSMain", ShaderStage::VertexShader },
                    "",
                    ""
                },
                {
                    "Transform_VS",
                    { "", "", "", ShaderStage::VertexShader },
                    "",
                    ""
                },
            };
            // clang-format on

            TestBase::SetUp();
        }
    };

    class PixelShaderTest : public TestBase
    {
    public:
        void SetUp() override
        {
            // clang-format off
            m_testSources =
            {
                {
                    "Constant_PS",
                    { "", "", "PSMain", ShaderStage::PixelShader },
                    "",
                    ""
                },
                {
                    "PassThrough_PS",
                    { "", "", "PSMain", ShaderStage::PixelShader },
                    "",
                    ""
                },
                {
                    "ToneMapping_PS",
                    { "", "", "", ShaderStage::PixelShader },
                    "",
                    ""
                },
            };
            // clang-format on

            TestBase::SetUp();
        }
    };

    class GeometryShaderTest : public TestBase
    {
    public:
        void SetUp() override
        {
            // clang-format off
            m_testSources =
            {
                {
                    "Particle_GS",
                    { "", "", "", ShaderStage::GeometryShader, defines_.data(), static_cast<uint32_t>(defines_.size()) },
                    "",
                    ""
                },
            };
            // clang-format on

            m_expectSuccessFlags[2] = false; // No GS in HLSL SM3
            m_expectSuccessFlags[3] = false; // GS not supported yet
            m_expectSuccessFlags[4] = false; // GS not supported yet
            m_expectSuccessFlags[9] = false; // No GS in MSL

            TestBase::SetUp();
        }

    private:
        std::vector<MacroDefine> defines_ = {{"FIXED_VERTEX_RADIUS", "5.0"}};
    };

    class HullShaderTest : public TestBase
    {
    public:
        void SetUp() override
        {
            // clang-format off
            m_testSources =
            {
                {
                    "DetailTessellation_HS",
                    { "", "", "", ShaderStage::HullShader },
                    "",
                    ""
                },
            };
            // clang-format on

            m_expectSuccessFlags[2] = false; // No HS in HLSL SM3
            m_expectSuccessFlags[3] = false; // No HS in HLSL SM4
            m_expectSuccessFlags[4] = false; // HS not supported yet

            TestBase::SetUp();
        }
    };

    class DomainShaderTest : public TestBase
    {
    public:
        void SetUp() override
        {
            // clang-format off
            m_testSources =
            {
                {
                    "PNTriangles_DS",
                    { "", "", "", ShaderStage::DomainShader },
                    "",
                    ""
                },
            };
            // clang-format on

            m_expectSuccessFlags[2] = false; // No HS in HLSL SM3
            m_expectSuccessFlags[3] = false; // No HS in HLSL SM4
            m_expectSuccessFlags[4] = false; // DS not supported yet

            TestBase::SetUp();
        }
    };

    class ComputeShaderTest : public TestBase
    {
    public:
        void SetUp() override
        {
            // clang-format off
            m_testSources =
            {
                {
                    "Fluid_CS",
                    { "", "", "", ShaderStage::ComputeShader },
                    "",
                    ""
                },
            };
            // clang-format on

            m_expectSuccessFlags[2] = false; // No CS in HLSL SM3
            m_expectSuccessFlags[3] = false; // CS in HLSL SM4 is not supported
            m_expectSuccessFlags[7] = false; // No CS in OpenGL ES 3.0

            TestBase::SetUp();
        }
    };


    TEST_F(VertexShaderTest, ToDxil)
    {
        RunTests(ShadingLanguage::Dxil);
    }

    TEST_F(VertexShaderTest, ToSpirV)
    {
        RunTests(ShadingLanguage::SpirV);
    }

    TEST_F(VertexShaderTest, ToHlsl)
    {
        RunTests(ShadingLanguage::Hlsl);
    }

    TEST_F(VertexShaderTest, ToGlsl)
    {
        RunTests(ShadingLanguage::Glsl);
    }

    TEST_F(VertexShaderTest, ToGlslColumnMajor)
    {
        const std::string fileName = TEST_DATA_DIR "Input/Transform_VS.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options options;
        options.packMatricesInRowMajor = false;

        HlslToAnyTest("Transform_VS_ColumnMajor", {source.c_str(), fileName.c_str(), nullptr, ShaderStage::VertexShader}, options,
                      {{ShadingLanguage::Glsl, "300"}}, {true});
    }

    TEST_F(VertexShaderTest, ToEssl)
    {
        RunTests(ShadingLanguage::Essl);
    }

    TEST_F(VertexShaderTest, ToMsl)
    {
        RunTests(ShadingLanguage::Msl_macOS);
    }


    TEST_F(PixelShaderTest, ToDxil)
    {
        RunTests(ShadingLanguage::Dxil);
    }

    TEST_F(PixelShaderTest, ToSpirV)
    {
        RunTests(ShadingLanguage::SpirV);
    }

    TEST_F(PixelShaderTest, ToHlsl)
    {
        RunTests(ShadingLanguage::Hlsl);
    }

    TEST_F(PixelShaderTest, ToGlsl)
    {
        RunTests(ShadingLanguage::Glsl);
    }

    TEST_F(PixelShaderTest, ToEssl)
    {
        RunTests(ShadingLanguage::Essl);
    }

    TEST_F(PixelShaderTest, ToMsl)
    {
        RunTests(ShadingLanguage::Msl_macOS);
    }


    TEST_F(GeometryShaderTest, ToDxil)
    {
        RunTests(ShadingLanguage::Dxil);
    }

    TEST_F(GeometryShaderTest, ToSpirV)
    {
        RunTests(ShadingLanguage::SpirV);
    }

    TEST_F(GeometryShaderTest, ToHlsl)
    {
        RunTests(ShadingLanguage::Hlsl);
    }

    TEST_F(GeometryShaderTest, ToGlsl)
    {
        RunTests(ShadingLanguage::Glsl);
    }

    TEST_F(GeometryShaderTest, ToEssl)
    {
        RunTests(ShadingLanguage::Essl);
    }

    TEST_F(GeometryShaderTest, ToMsl)
    {
        RunTests(ShadingLanguage::Msl_macOS);
    }


    TEST_F(HullShaderTest, ToDxil)
    {
        RunTests(ShadingLanguage::Dxil);
    }

    TEST_F(HullShaderTest, ToSpirV)
    {
        RunTests(ShadingLanguage::SpirV);
    }

    TEST_F(HullShaderTest, ToHlsl)
    {
        RunTests(ShadingLanguage::Hlsl);
    }

    TEST_F(HullShaderTest, ToGlsl)
    {
        RunTests(ShadingLanguage::Glsl);
    }

    TEST_F(HullShaderTest, ToEssl)
    {
        RunTests(ShadingLanguage::Essl);
    }

    TEST_F(HullShaderTest, ToMsl)
    {
        RunTests(ShadingLanguage::Msl_macOS);
    }


    TEST_F(DomainShaderTest, ToDxil)
    {
        RunTests(ShadingLanguage::Dxil);
    }

    TEST_F(DomainShaderTest, ToSpirV)
    {
        RunTests(ShadingLanguage::SpirV);
    }

    TEST_F(DomainShaderTest, ToHlsl)
    {
        RunTests(ShadingLanguage::Hlsl);
    }

    TEST_F(DomainShaderTest, ToGlsl)
    {
        RunTests(ShadingLanguage::Glsl);
    }

    TEST_F(DomainShaderTest, ToEssl)
    {
        RunTests(ShadingLanguage::Essl);
    }

    TEST_F(DomainShaderTest, ToMsl)
    {
        RunTests(ShadingLanguage::Msl_macOS);
    }


    TEST_F(ComputeShaderTest, ToDxil)
    {
        RunTests(ShadingLanguage::Dxil);
    }

    TEST_F(ComputeShaderTest, ToSpirV)
    {
        RunTests(ShadingLanguage::SpirV);
    }

    TEST_F(ComputeShaderTest, ToHlsl)
    {
        RunTests(ShadingLanguage::Hlsl);
    }

    TEST_F(ComputeShaderTest, ToGlsl)
    {
        RunTests(ShadingLanguage::Glsl);
    }

    TEST_F(ComputeShaderTest, ToEssl)
    {
        RunTests(ShadingLanguage::Essl);
    }

    TEST_F(ComputeShaderTest, ToMsl)
    {
        RunTests(ShadingLanguage::Msl_macOS);
    }

    TEST(IncludeTest, IncludeExist)
    {
        const std::string fileName = TEST_DATA_DIR "Input/IncludeExist.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        const auto result =
            Compiler::Compile({source.c_str(), fileName.c_str(), "main", ShaderStage::PixelShader}, {}, {ShadingLanguage::Glsl, "30"});

        EXPECT_FALSE(result.hasError);
        EXPECT_EQ(result.errorWarningMsg.Size(), 0U);
        EXPECT_TRUE(result.isText);

        const uint8_t* targetPtr = reinterpret_cast<const uint8_t*>(result.target.Data());
        CompareWithExpected(std::vector<uint8_t>(targetPtr, targetPtr + result.target.Size()), result.isText, "IncludeExist.glsl");
    }

    TEST(IncludeTest, IncludeNotExist)
    {
        const std::string fileName = TEST_DATA_DIR "Input/IncludeNotExist.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        const auto result =
            Compiler::Compile({source.c_str(), fileName.c_str(), "main", ShaderStage::PixelShader}, {}, {ShadingLanguage::Glsl, "30"});

        EXPECT_TRUE(result.hasError);
        const char* errorStr = reinterpret_cast<const char*>(result.errorWarningMsg.Data());
        EXPECT_GE(std::string(errorStr, errorStr + result.errorWarningMsg.Size()).find("fatal error: 'Header.hlsli' file not found"), 0U);
    }

    TEST(IncludeTest, IncludeEmptyFile)
    {
        const std::string fileName = TEST_DATA_DIR "Input/IncludeEmptyHeader.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        const auto result =
            Compiler::Compile({source.c_str(), fileName.c_str(), "main", ShaderStage::PixelShader}, {}, {ShadingLanguage::Glsl, "30"});

        EXPECT_FALSE(result.hasError);
        EXPECT_EQ(result.errorWarningMsg.Size(), 0U);
        EXPECT_TRUE(result.isText);

        const uint8_t* targetPtr = reinterpret_cast<const uint8_t*>(result.target.Data());
        CompareWithExpected(std::vector<uint8_t>(targetPtr, targetPtr + result.target.Size()), result.isText, "IncludeEmptyHeader.glsl");
    }

    TEST(HalfDataTypeTest, DotHalf)
    {
        const std::string fileName = TEST_DATA_DIR "Input/HalfDataType.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options option;
        option.shaderModel = {6, 2};
        option.enable16bitTypes = true;

        const std::tuple<Compiler::TargetDesc, std::string> targets[] = {{{ShadingLanguage::Dxil}, "dxilasm"},
                                                                         {{ShadingLanguage::SpirV, "15"}, "spvasm"},
                                                                         {{ShadingLanguage::Glsl, "300"}, "glsl"},
                                                                         {{ShadingLanguage::Essl, "310"}, "essl"},
                                                                         {{ShadingLanguage::Msl_macOS}, "msl"}};

        for (const auto& target : targets)
        {
            auto result =
                Compiler::Compile({source.c_str(), fileName.c_str(), "DotHalfPS", ShaderStage::PixelShader}, option, std::get<0>(target));

            EXPECT_FALSE(result.hasError);

            if ((std::get<0>(target).language == ShadingLanguage::Dxil) || (std::get<0>(target).language == ShadingLanguage::SpirV))
            {
                EXPECT_FALSE(result.isText);
                result = Disassemble(std::get<0>(target).language, result);
            }

            EXPECT_FALSE(result.hasError);
            EXPECT_TRUE(result.isText);

            const uint8_t* targetPtr = reinterpret_cast<const uint8_t*>(result.target.Data());
            std::vector<uint8_t> actualTarget(targetPtr, targetPtr + result.target.Size());
            if (std::get<0>(target).language == ShadingLanguage::Dxil)
            {
                RemoveDxilAsmHash(actualTarget);
            }

            CompareWithExpected(actualTarget, result.isText, "DotHalfPS." + std::get<1>(target));
        }
    }

    TEST(HalfDataTypeTest, HalfOutParam)
    {
        const std::string fileName = TEST_DATA_DIR "Input/HalfDataType.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options option;
        option.shaderModel = {6, 2};
        option.enable16bitTypes = true;

        const std::tuple<Compiler::TargetDesc, std::string> targets[] = {{{ShadingLanguage::Dxil}, "dxilasm"},
                                                                         {{ShadingLanguage::SpirV, "15"}, "spvasm"},
                                                                         {{ShadingLanguage::Glsl, "300"}, "glsl"},
                                                                         {{ShadingLanguage::Essl, "310"}, "essl"},
                                                                         {{ShadingLanguage::Msl_macOS}, "msl"}};

        for (const auto& target : targets)
        {
            auto result = Compiler::Compile({source.c_str(), fileName.c_str(), "HalfOutParamPS", ShaderStage::PixelShader}, option,
                                            std::get<0>(target));

            EXPECT_FALSE(result.hasError);

            if ((std::get<0>(target).language == ShadingLanguage::Dxil) || (std::get<0>(target).language == ShadingLanguage::SpirV))
            {
                EXPECT_FALSE(result.isText);
                result = Disassemble(std::get<0>(target).language, result);
            }

            EXPECT_FALSE(result.hasError);
            EXPECT_TRUE(result.isText);

            const uint8_t* targetPtr = reinterpret_cast<const uint8_t*>(result.target.Data());
            std::vector<uint8_t> actualTarget(targetPtr, targetPtr + result.target.Size());
            if (std::get<0>(target).language == ShadingLanguage::Dxil)
            {
                RemoveDxilAsmHash(actualTarget);
            }

            CompareWithExpected(actualTarget, result.isText, "HalfOutParamPS." + std::get<1>(target));
        }
    }

    TEST(HalfDataTypeTest, HalfBuffer)
    {
        const std::string fileName = TEST_DATA_DIR "Input/HalfDataType.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options option;
        option.shaderModel = {6, 2};
        option.enable16bitTypes = true;

        const std::tuple<Compiler::TargetDesc, std::string> targets[] = {{{ShadingLanguage::Dxil}, "dxilasm"},
                                                                         {{ShadingLanguage::SpirV, "15"}, "spvasm"},
                                                                         {{ShadingLanguage::Glsl, "300"}, "glsl"},
                                                                         {{ShadingLanguage::Essl, "310"}, "essl"},
                                                                         {{ShadingLanguage::Msl_macOS}, "msl"}};

        for (const auto& target : targets)
        {
            auto result = Compiler::Compile({source.c_str(), fileName.c_str(), "HalfBufferPS", ShaderStage::PixelShader}, option,
                                            std::get<0>(target));

            EXPECT_FALSE(result.hasError);

            if ((std::get<0>(target).language == ShadingLanguage::Dxil) || (std::get<0>(target).language == ShadingLanguage::SpirV))
            {
                EXPECT_FALSE(result.isText);
                result = Disassemble(std::get<0>(target).language, result);
            }

            EXPECT_FALSE(result.hasError);
            EXPECT_TRUE(result.isText);

            const uint8_t* targetPtr = reinterpret_cast<const uint8_t*>(result.target.Data());
            std::vector<uint8_t> actualTarget(targetPtr, targetPtr + result.target.Size());
            if (std::get<0>(target).language == ShadingLanguage::Dxil)
            {
                RemoveDxilAsmHash(actualTarget);
            }

            CompareWithExpected(actualTarget, result.isText, "HalfBufferPS." + std::get<1>(target));
        }
    }

    TEST(LinkTest, LinkDxil)
    {
        if (!Compiler::LinkSupport())
        {
            GTEST_SKIP_("Link is not supported on this platform");
        }

        Compiler::Options options;
        options.shaderModel = {6, 5};

        const Compiler::TargetDesc target = {ShadingLanguage::Dxil, "", true};
        const Compiler::ModuleDesc dxilModules[] = {
            CompileToModule("CalcLight", TEST_DATA_DIR "Input/CalcLight.hlsl", options, target),
            CompileToModule("CalcLightDiffuse", TEST_DATA_DIR "Input/CalcLightDiffuse.hlsl", options, target),
            CompileToModule("CalcLightDiffuseSpecular", TEST_DATA_DIR "Input/CalcLightDiffuseSpecular.hlsl", options, target),
        };

        const Compiler::ModuleDesc* testModules[][2] = {
            {&dxilModules[0], &dxilModules[1]},
            {&dxilModules[0], &dxilModules[2]},
        };

        const std::string expectedNames[] = {"CalcLight+Diffuse.dxilasm", "CalcLight+DiffuseSpecular.dxilasm"};

        for (size_t i = 0; i < 2; ++i)
        {
            const auto linkedResult =
                Compiler::Link({"main", ShaderStage::PixelShader, testModules[i], sizeof(testModules[i]) / sizeof(testModules[i][0])},
                               options, {ShadingLanguage::Dxil, ""});

            EXPECT_FALSE(linkedResult.hasError);
            EXPECT_FALSE(linkedResult.isText);

            const auto disasmResult = Disassemble(ShadingLanguage::Dxil, linkedResult);

            EXPECT_FALSE(disasmResult.hasError);
            EXPECT_TRUE(disasmResult.isText);

            const uint8_t* targetPtr = reinterpret_cast<const uint8_t*>(disasmResult.target.Data());
            std::vector<uint8_t> actualTarget(targetPtr, targetPtr + disasmResult.target.Size());
            RemoveDxilAsmHash(actualTarget);

            CompareWithExpected(actualTarget, disasmResult.isText, expectedNames[i]);
        }
    }

    TEST(SubpassInputTest, SubpassInput)
    {
        const std::string fileName = TEST_DATA_DIR "Input/Subpass_PS.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        const std::tuple<Compiler::TargetDesc, std::string> targets[] = {{{ShadingLanguage::SpirV, "15"}, "spvasm"},
                                                                         {{ShadingLanguage::Glsl, "300"}, "glsl"},
                                                                         {{ShadingLanguage::Essl, "310"}, "essl"},
                                                                         {{ShadingLanguage::Msl_iOS}, "msl"}};

        for (const auto& target : targets)
        {
            auto result = Compiler::Compile({source.c_str(), fileName.c_str(), "main", ShaderStage::PixelShader}, {}, std::get<0>(target));

            EXPECT_FALSE(result.hasError);

            if ((std::get<0>(target).language == ShadingLanguage::Dxil) || (std::get<0>(target).language == ShadingLanguage::SpirV))
            {
                EXPECT_FALSE(result.isText);
                result = Disassemble(std::get<0>(target).language, result);
            }

            EXPECT_FALSE(result.hasError);
            EXPECT_TRUE(result.isText);

            const uint8_t* targetPtr = reinterpret_cast<const uint8_t*>(result.target.Data());
            std::vector<uint8_t> actualTarget(targetPtr, targetPtr + result.target.Size());
            if (std::get<0>(target).language == ShadingLanguage::Dxil)
            {
                RemoveDxilAsmHash(actualTarget);
            }

            CompareWithExpected(actualTarget, result.isText, "Subpass_PS." + std::get<1>(target));
        }
    }
} // namespace
