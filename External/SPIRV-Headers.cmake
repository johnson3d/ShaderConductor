# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(SPIRV_Headers_REV "b2a156e1c0434bc8c99aaebba1c7be98be7ac580")

UpdateExternalLib("SPIRV-Headers" "https://github.com/KhronosGroup/SPIRV-Headers.git" ${SPIRV_Headers_REV})

set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
set(SPIRV_HEADERS_SKIP_INSTALL ON CACHE BOOL "" FORCE)

add_subdirectory(SPIRV-Headers EXCLUDE_FROM_ALL)
foreach(target
    "install-headers")
    set_target_properties(${target} PROPERTIES FOLDER "External/SPIRV-Headers")
endforeach()
