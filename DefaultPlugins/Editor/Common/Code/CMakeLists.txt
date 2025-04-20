cmake_minimum_required (VERSION 3.14)

include("${ENGINE_CMAKE_DIRECTORY}/MakePlugin.cmake")
include("${ENGINE_CMAKE_DIRECTORY}/LinkPlugin.cmake")

MakePlugin(4F6B545D-F826-4BAE-9A41-CD5149979D8D ${CMAKE_CURRENT_LIST_DIR} "EditorCommon" "lib" "bin" "../Common.nplugin")