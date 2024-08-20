# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(DirectX_Headers_REV "980971e835876dc0cde415e8f9bc646e64667bf7")

UpdateExternalLib("DirectX-Headers" "https://github.com/microsoft/DirectX-Headers.git" ${DirectX_Headers_REV})

set(DXHEADERS_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(DXHEADERS_INSTALL OFF CACHE BOOL "" FORCE)
set(DXHEADERS_BUILD_GOOGLE_TEST OFF CACHE BOOL "" FORCE)

add_subdirectory(DirectX-Headers EXCLUDE_FROM_ALL)
