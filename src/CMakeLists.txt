set(TARGET tairstring_module)

set(SRCS
        tairstring.h
        tairstring.c
        redismodule.h )

add_library(${TARGET} SHARED ${SRCS} ${USRC})
set_target_properties(${TARGET} PROPERTIES SUFFIX ".so")
set_target_properties(${TARGET} PROPERTIES PREFIX "")