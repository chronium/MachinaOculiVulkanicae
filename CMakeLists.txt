cmake_minimum_required(VERSION 3.16)
project(vrstuff VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

find_package(Vulkan REQUIRED glslc glslang)

FetchContent_Declare(
	spdlog
	GIT_REPOSITORY https://github.com/gabime/spdlog.git
	GIT_TAG v1.x
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    OpenXR
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK-Source.git
    GIT_TAG main
    GIT_SHALLOW ON
    OPTIONS
)
FetchContent_MakeAvailable(OpenXR)

set(CODEGEN_FOLDER ${CMAKE_CURRENT_BINARY_DIR}/codegen)
add_subdirectory(include)
add_subdirectory(apps)
add_subdirectory(data)

