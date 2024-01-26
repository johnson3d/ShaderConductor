# ShaderConductor

[![Build Status](https://gongminmin.visualstudio.com/ShaderConductor/_apis/build/status%2Fgongminmin.ShaderConductor?branchName=develop)](https://gongminmin.visualstudio.com/ShaderConductor/_build/latest?definitionId=5&branchName=develop)
[![License](https://img.shields.io/github/license/mashape/apistatus.svg)](LICENSE)


ShaderConductor is a tool designed for cross-compiling HLSL to other shading languages.

## Features

* Converts HLSL to readable, usable and efficient GLSL
* Converts HLSL to readable, usable and efficient ESSL
* Converts HLSL to readable, usable and efficient Metal Shading Language (MSL)
* Converts HLSL to readable, usable and efficient old shader model HLSL
* Supports all stages of shaders, vertex, pixel, hull, domain, geometry, and compute.

Note that this project is still in an early stage, and it is under active development.

## Architecture

ShaderConductor is not a real compiler. Instead, it glues existing open source components to do the cross-compiling.
1. [DirectX Shader Compiler (DXC)](https://github.com/Microsoft/DirectXShaderCompiler) to compile HLSL to [DXIL](https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/DXIL.rst) or [SPIR-V](https://www.khronos.org/registry/spir-v/),
1. [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) to convert SPIR-V to target shading languages.

![Architecture](Doc/Arch.svg)

## Prerequisites

* [Git](http://git-scm.com/downloads). Put git into the PATH is recommended.
* [Visual Studio 2017](https://www.visualstudio.com/downloads). Select the following workloads: Universal Windows Platform Development and Desktop Development with C++.
* [CMake](https://www.cmake.org/download/). Version 3.20 or up. It's highly recommended to choose "Add CMake to the system PATH for all users" during installation.
* [Python](https://www.python.org/downloads/). Version 3.6 or up. You need not change your PATH variable during installation.

## Building

ShaderConductor has been tested on Windows, Linux, and macOS.

### The script way:

```
  BuildAll.py <BuildSystem> <Compiler> <Architecture> <Configuration>
```
where,
* \<BuildSystem\> can be ninja, vs2017, vs2019, or vs2022. Default is vs2022.
* \<Compiler\> can be vc141, vc142, or vc143 on Windows, gcc or clang on Linux, clang on macOS.
* \<Architecture\> can be x64 or arm64 on Windows, x64 on other platforms.
* \<Configuration\> can be Debug, Release, RelWithDebInfo, or MinSizeRel. Default is Release.
 
This script automatically grabs external dependencies to External folder, generates project file in Build/\<BuildSystem\>-\<Compiler\>-\<Platform\>-\<Architecture\>[-\<Configuration\>], and builds it.

### The manual way:

```
  mkdir Build
  cd Build
  cmake -G "Visual Studio 17" -T host=x64 -A x64 ../
  cmake --build .
```

After building, the output file ShaderConductor.dll can be located in \<YourCMakeTargetFolder\>/Bin/\<Configuration\>/. It depends on dxcompiler.dll in the same folder.

By setting some CMake variables, you can use a prebuilt DXC package. The building time is dramatically shortened by doing that. The `SC_PREBUILT_DXC_DIR` represent a local directory of prebuilt DXC. More advance ones are `SC_PREBUILT_DXC_URL` and `SC_PREBUILT_DXC_NAME`. You can set a URL and the file name of an online DXC package. It'll be downloaded and extracted. Or you can simply set them to `AUTO`. There are predefined URLs and file names for Windows x64/arm64, Ubuntu 20/22 x64, and macOS x64.

### Artifacts

You can download [the prebuilt binaries generated by CI system](https://gongminmin.visualstudio.com/ShaderConductor/_build?definitionId=5&_a=summary). Currently, artifacts for Windows, Linux, macOS are published every commit.

## License

ShaderConductor is distributed under the terms of MIT License. See [LICENSE](LICENSE) for details.

## Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
