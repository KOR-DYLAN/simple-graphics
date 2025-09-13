project(sgl-core
    VERSION 0.0.1
    LANGUAGES ASM C
)

add_library(${PROJECT_NAME})
add_subdirectory(convert)
add_subdirectory(crop)
add_subdirectory(rotate)
