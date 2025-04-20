cmake_minimum_required (VERSION 3.14)

include("${ENGINE_CMAKE_DIRECTORY}/MakePlugin.cmake")

MakePlugin(5DC6A146-4AE7-43E8-9B53-69D46FC184AF ${CMAKE_CURRENT_LIST_DIR} "FontRendering" "lib" "bin" "../FontRendering.nplugin")

if (PLATFORM_LINUX)
    find_package(Freetype REQUIRED)
endif()
if(FREETYPE_FOUND)
    target_link_libraries(5DC6A146-4AE7-43E8-9B53-69D46FC184AF PRIVATE Freetype::Freetype)
else()
    target_link_libraries(5DC6A146-4AE7-43E8-9B53-69D46FC184AF PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/freetype/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/${CMAKE_SHARED_LIBRARY_PREFIX}freetype${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_include_directories(5DC6A146-4AE7-43E8-9B53-69D46FC184AF PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/freetype/include")
endif()