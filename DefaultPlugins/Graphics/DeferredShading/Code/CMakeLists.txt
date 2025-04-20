cmake_minimum_required (VERSION 3.14)

include("${ENGINE_CMAKE_DIRECTORY}/MakePlugin.cmake")

MakePlugin(B7921721-4029-47CE-873C-5D356D604AA9 ${CMAKE_CURRENT_LIST_DIR} "DeferredShading" "lib" "bin" "../DeferredShading.nplugin")
