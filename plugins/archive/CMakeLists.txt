# The archive module on libarchive: listing archives (Enter on an archive).
gemini_add_plugin(gemini-archive-plugin LibarchivePlugin.cpp)
target_include_directories(gemini-archive-plugin PRIVATE ${LIBARCHIVE_INCLUDE_DIRS})
target_link_libraries(gemini-archive-plugin PRIVATE ${LIBARCHIVE_LIBRARIES})
