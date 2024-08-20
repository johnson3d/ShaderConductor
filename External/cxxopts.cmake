# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(cxxopts_REV "32afbc65263e42fa089f473d5a6131983d9b7200")

UpdateExternalLib("cxxopts" "https://github.com/jarro2783/cxxopts.git" ${cxxopts_REV})

set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CXXOPTS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(cxxopts EXCLUDE_FROM_ALL)
