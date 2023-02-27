file(GLOB headers ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

add_custom_target(MovHeaders SOURCES ${headers})