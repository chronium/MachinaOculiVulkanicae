set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(core "core.main.cpp" "core/Controller.hpp" "core/Controller.cpp")
add_dependencies(core shaders generate_openxr_header)

target_include_directories(core PRIVATE Vulkan::Headers ${openxr_SOURCE_DIR}/include spdlog::spdlog ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(core PRIVATE Vulkan::Vulkan openxr_loader XrApiLayer_core_validation XrApiLayer_api_dump spdlog::spdlog mov assimp::assimp)
