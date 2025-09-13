project(sample
    VERSION 1.0
    LANGUAGES C CXX
)

list(APPEND SRC main.c)

add_executable(${PROJECT_NAME} ${SRC})
