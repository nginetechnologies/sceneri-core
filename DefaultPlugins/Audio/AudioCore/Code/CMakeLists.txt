cmake_minimum_required(VERSION 3.14)

include("${ENGINE_CMAKE_DIRECTORY}/MakePlugin.cmake")

MakePlugin(a4bbf061-5f65-43b1-acc0-711f28b3bb56 ${CMAKE_CURRENT_LIST_DIR} "AudioCore" "lib" "bin" "../Audio.nplugin")

target_include_directories(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/libnyquist")

target_compile_definitions(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PRIVATE AL_LIBTYPE_STATIC=1)

if(PLATFORM_WINDOWS)
    set(OPENAL_LIB_PATH "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/openal-soft/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/OpenAL32${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(OPENAL_COMMON_LIB_PATH "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/openal-soft/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/common${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(LIBNYQUIST_LIB_PATH "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/libnyquist/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/libnyquist${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_compile_definitions(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PRIVATE HAS_LIBNYQUIST=1)
    target_include_directories(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/openal-soft")

    target_link_libraries(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC ${OPENAL_LIB_PATH})
    target_link_libraries(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC ${LIBNYQUIST_LIB_PATH})
elseif(PLATFORM_EMSCRIPTEN)
    set(OPENAL_LIB_PATH "openal")
    target_compile_definitions(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PRIVATE HAS_LIBNYQUIST=0)

    target_link_libraries(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC ${OPENAL_LIB_PATH})
    target_link_libraries(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC ${LIBNYQUIST_LIB_PATH})
elseif(PLATFORM_APPLE_VISIONOS)
    set(OPENAL_LIB_PATH "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/openal-soft/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/${CMAKE_STATIC_LIBRARY_PREFIX}openal${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_compile_definitions(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PRIVATE HAS_LIBNYQUIST=0)
    target_include_directories(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/openal-soft")

    target_link_libraries(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC ${OPENAL_LIB_PATH})
    target_link_libraries(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC ${LIBNYQUIST_LIB_PATH})
elseif(PLATFORM_LINUX)
    find_package(OpenAL REQUIRED)
    target_link_libraries(Renderer PRIVATE ${OPENAL_LIBRARY})
    target_include_directories(Renderer PRIVATE ${OPENAL_INCLUDE_DIR})

    target_compile_definitions(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PRIVATE HAS_LIBNYQUIST=0)
else()
    set(OPENAL_LIB_PATH "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/openal-soft/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/${CMAKE_STATIC_LIBRARY_PREFIX}openal${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(LIBNYQUIST_LIB_PATH "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/libnyquist/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/${CMAKE_STATIC_LIBRARY_PREFIX}libnyquist${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_compile_definitions(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PRIVATE HAS_LIBNYQUIST=1)
    target_include_directories(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/openal-soft")

    target_link_libraries(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC ${OPENAL_LIB_PATH})
    target_link_libraries(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC ${LIBNYQUIST_LIB_PATH})
endif()

if(PLATFORM_ANDROID)
    target_link_libraries(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC OpenSLES)
endif()

if(PLATFORM_WINDOWS)
    target_link_libraries(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC ${OPENAL_COMMON_LIB_PATH})
    target_link_libraries(a4bbf061-5f65-43b1-acc0-711f28b3bb56 PUBLIC "Winmm.lib")
endif()

if(PLATFORM_APPLE) 
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/Private/OpenAL/OpenALSystem.cpp" PROPERTIES LANGUAGE OBJCXX)
endif()
