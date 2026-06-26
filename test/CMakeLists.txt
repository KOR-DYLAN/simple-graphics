include(${CMAKE_SOURCE_DIR}/script/config/test-dependencies.cmake)

include_directories(util)
add_subdirectory(sample)
add_subdirectory(resize)
add_subdirectory(queue)
add_subdirectory(memory)
add_subdirectory(util)
