target_sources(${PROJECT_NAME} PRIVATE
    generic_memory.c
    memory.c
    memory_operation.c
)

if(SGL_CFG_HAS_NEON)
    target_sources(${PROJECT_NAME} PRIVATE neon_memory.c)
endif()
