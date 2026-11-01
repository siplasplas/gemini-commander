# Modules of Gemini Commander (Qt plugins). Each keeps its dependencies out
# of the main program, which works without any of them.

# A plugin module: built into <build>/plugins, installed to
# lib/gemini-commander/plugins.
function(gemini_add_plugin target)
    add_library(${target} MODULE ${ARGN})
    set_target_properties(${target} PROPERTIES
        AUTOMOC ON
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${GEMINI_PLUGIN_BUILD_DIR}")
    target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${target} PRIVATE Qt6::Core)
    install(TARGETS ${target} LIBRARY DESTINATION ${GEMINI_PLUGIN_INSTALL_DIR})
endfunction()

if(LIBARCHIVE_FOUND)
    add_subdirectory(archive)
else()
    message(STATUS "libarchive not found: no archive module (archives listed by lsar)")
endif()
