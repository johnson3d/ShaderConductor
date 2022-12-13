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

#include "Common.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <iterator> // For std::size on raw array
#include <string>
#include <vector>

namespace ShaderConductor
{
    std::vector<uint8_t> LoadFile(const std::string& name, bool isText)
    {
        std::vector<uint8_t> ret;
        std::ios_base::openmode mode = std::ios_base::in;
        if (!isText)
        {
            mode |= std::ios_base::binary;
        }
        std::ifstream file(name, mode);
        if (file)
        {
            file.seekg(0, std::ios::end);
            ret.resize(static_cast<size_t>(file.tellg()));
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(ret.data()), ret.size());
            ret.resize(static_cast<size_t>(file.gcount()));
        }
        return ret;
    }

    void CompareWithExpected(const std::vector<uint8_t>& actual, bool isText, const std::string& compareName)
    {
        std::vector<uint8_t> expected = LoadFile(TEST_DATA_DIR "Expected/" + compareName, isText);
        if (expected != actual)
        {
            if (!actual.empty())
            {
                std::ios_base::openmode mode = std::ios_base::out;
                if (!isText)
                {
                    mode |= std::ios_base::binary;
                }
                std::ofstream actualFile(TEST_DATA_DIR "Result/" + compareName, mode);
                actualFile.write(reinterpret_cast<const char*>(actual.data()), actual.size());
            }
        }

        EXPECT_EQ(std::string(expected.begin(), expected.end()), std::string(actual.begin(), actual.end()));
    }

    void RemoveDxilAsmHash(std::vector<uint8_t>& text)
    {
        const std::tuple<std::string, uint32_t> landmarks[] = {{"shader debug name: ", 32}, {"shader hash: ", 32}, {"\"dxc(private) ", 22}};

        const std::string_view text_view(reinterpret_cast<const char*>(text.data()), text.size());
        for (size_t i = 0; i < std::size(landmarks); ++i)
        {
            size_t location = text_view.find(std::get<0>(landmarks[i]));
            if (location != std::string::npos)
            {
                location += std::get<0>(landmarks[i]).size();
                for (size_t j = 0; j < std::get<1>(landmarks[i]); ++j)
                {
                    text[location] = '*';
                    ++location;
                }
            }
        }
    }
} // namespace ShaderConductor

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    int retVal = RUN_ALL_TESTS();
    if (retVal != 0)
    {
        getchar();
    }

    return retVal;
}
