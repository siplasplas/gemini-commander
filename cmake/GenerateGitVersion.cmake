# Writes GIT_VERSION_HEADER with the commit of SOURCE_DIR and of the libraries
# built from source (TEXTVIEWERS_DIR, QCODEEDIT_DIR, QCEDITOR_DIR; empty when
# not built in): `git describe` and the date of the commit.

# Sets <prefix>_DESCRIBE and <prefix>_DATE for the git work tree in dir;
# "unknown" and "" when it is not one.
function(git_commit_info dir prefix describeArgs)
    set(describe "")
    set(date "")
    if(dir AND EXISTS "${dir}")
        execute_process(
            COMMAND git describe ${describeArgs}
            WORKING_DIRECTORY ${dir}
            OUTPUT_VARIABLE describe
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        execute_process(
            COMMAND git log -1 "--format=%cd" "--date=format:%Y-%m-%d %H:%M"
            WORKING_DIRECTORY ${dir}
            OUTPUT_VARIABLE date
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
    if(NOT describe)
        set(describe "unknown")
    endif()
    set(${prefix}_DESCRIBE "${describe}" PARENT_SCOPE)
    set(${prefix}_DATE "${date}" PARENT_SCOPE)
endfunction()

# Get git SHA with dirty flag
git_commit_info("${SOURCE_DIR}" GIT "--always;--dirty")
git_commit_info("${TEXTVIEWERS_DIR}" TEXTVIEWERS "--tags;--always;--dirty")
git_commit_info("${QCODEEDIT_DIR}" QCODEEDIT "--tags;--always;--dirty")
git_commit_info("${QCEDITOR_DIR}" QCEDITOR "--tags;--always;--dirty")

set(NEW_CONTENT "#pragma once\n")
string(APPEND NEW_CONTENT "#define GIT_SHA \"${GIT_DESCRIBE}\"\n")
string(APPEND NEW_CONTENT "#define GIT_DATE \"${GIT_DATE}\"\n")
foreach(lib TEXTVIEWERS QCODEEDIT QCEDITOR)
    string(APPEND NEW_CONTENT "#define ${lib}_GIT_DESCRIBE \"${${lib}_DESCRIBE}\"\n")
    string(APPEND NEW_CONTENT "#define ${lib}_GIT_DATE \"${${lib}_DATE}\"\n")
endforeach()

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
