cmake_minimum_required (VERSION 3.14)

include("${ENGINE_CMAKE_DIRECTORY}/MakePlugin.cmake")

MakePlugin(4CC21FD4-730F-475D-9807-FBF9E5595308 ${CMAKE_CURRENT_LIST_DIR} "Animation" "lib" "bin" "../Animation.eduplugin")

if((PLATFORM_DESKTOP OR PLATFORM_APPLE_IOS) AND NOT PLATFORM_LINUX)
	set(FBX_LIB_PATH "${CMAKE_CURRENT_LIST_DIR}/Public/Animation/3rdparty/fbxsdk/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/libfbxsdk${CMAKE_STATIC_LIBRARY_SUFFIX}")
	set(XML2_LIB_PATH "${CMAKE_CURRENT_LIST_DIR}/Public/Animation/3rdparty/fbxsdk/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/libxml2${CMAKE_STATIC_LIBRARY_SUFFIX}")

	# We do not currently have FBX libraries for Mac Catalyst
	# We really should find a way to drop the FBX SDK as it's closed source.
	if(PLATFORM_APPLE AND NOT PLATFORM_APPLE_MACCATALYST)
		target_compile_definitions(4CC21FD4-730F-475D-9807-FBF9E5595308 PUBLIC HAS_FBX_SDK=1)

		target_link_libraries(4CC21FD4-730F-475D-9807-FBF9E5595308 PUBLIC "${FBX_LIB_PATH}")
		target_link_libraries(4CC21FD4-730F-475D-9807-FBF9E5595308 PUBLIC "${XML2_LIB_PATH}")
	else()
		target_compile_definitions(4CC21FD4-730F-475D-9807-FBF9E5595308 PUBLIC HAS_FBX_SDK=1)

		target_link_libraries(4CC21FD4-730F-475D-9807-FBF9E5595308 PUBLIC ${FBX_LIB_PATH})
		target_link_libraries(4CC21FD4-730F-475D-9807-FBF9E5595308 PUBLIC ${XML2_LIB_PATH})
	endif()
	
	if(PLATFORM_APPLE)
		target_link_libraries(4CC21FD4-730F-475D-9807-FBF9E5595308 PUBLIC -lz)
	elseif(PLATFORM_EMSCRIPTEN)
		target_link_options(4CC21FD4-730F-475D-9807-FBF9E5595308 PRIVATE -USE_ZLIB=1)
	endif()

	if(PLATFORM_APPLE_IOS)
		target_link_libraries(4CC21FD4-730F-475D-9807-FBF9E5595308 PUBLIC iconv)
	endif()
else()
	target_compile_definitions(4CC21FD4-730F-475D-9807-FBF9E5595308 PUBLIC HAS_FBX_SDK=0)
endif()
