# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(SPIRV_Headers_REV "87d5b782bec60822aa878941e6b13c0a9a954c9b")

UpdateExternalLib("SPIRV-Headers" "https://github.com/KhronosGroup/SPIRV-Headers.git" ${SPIRV_Headers_REV})

set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
set(SPIRV_HEADERS_SKIP_INSTALL ON CACHE BOOL "" FORCE)

add_subdirectory(SPIRV-Headers EXCLUDE_FROM_ALL)
