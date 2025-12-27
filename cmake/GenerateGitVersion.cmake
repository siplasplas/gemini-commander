# Get git SHA with dirty flag
execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_SHA)
    set(GIT_SHA "unknown")
endif()

set(NEW_CONTENT "#pragma once\n#define GIT_SHA \"${GIT_SHA}\"\n")

# Read existing content if file exists
if(EXISTS ${GIT_VERSION_HEADER})
    file(READ ${GIT_VERSION_HEADER} OLD_CONTENT)
else()
    set(OLD_CONTENT "")
endif()

# Only write if content changed (avoids unnecessary recompilation)
if(NOT "${NEW_CONTENT}" STREQUAL "${OLD_CONTENT}")
    file(WRITE ${GIT_VERSION_HEADER} "${NEW_CONTENT}")
endif()
