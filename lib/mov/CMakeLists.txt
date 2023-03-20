set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_library(mov "VkUtils.cpp" "VkBuffer.cpp" "VkImage.cpp" "GameObject.cpp" "surface/SDLSurface.cpp" "backend/VulkanInstance.cpp" "Application.cpp" "backend/VulkanDebugger.hpp")
target_include_directories(mov PRIVATE Vulkan::Headers ${CMAKE_SOURCE_DIR}/include spdlog::spdlog)
target_link_libraries(mov PRIVATE Vulkan::Vulkan spdlog::spdlog)
