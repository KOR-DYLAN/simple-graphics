project(test-util
    VERSION 1.0
    LANGUAGES C CXX
)

list(APPEND SRC util.c)

add_library(${PROJECT_NAME} ${SRC})
add_dependencies(${PROJECT_NAME} sgl-test-dependencies)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE SGLTest::PNG SGLTest::ZLIB)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
