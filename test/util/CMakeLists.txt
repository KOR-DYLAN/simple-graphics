project(test-util
    VERSION 1.0
    LANGUAGES C CXX
)

find_package(ZLIB CONFIG REQUIRED)
find_package(PNG  CONFIG REQUIRED)

list(APPEND SRC util.c)

add_library(${PROJECT_NAME} ${SRC})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE PNG::PNG ZLIB::ZLIB)
