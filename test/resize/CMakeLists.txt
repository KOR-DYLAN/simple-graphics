project(resize
    VERSION 1.0
    LANGUAGES C CXX
)

list(APPEND SRC main.c)

add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE test-util sgl-core)
