include("${ENGINE_CMAKE_DIRECTORY}/MakeConsole.cmake")
include("${ENGINE_CMAKE_DIRECTORY}/LinkPlugin.cmake")

if(NOT PLATFORM_APPLE_IOS)
	MakeConsole(AssetCompiler "${CMAKE_CURRENT_LIST_DIR}")
	if(OPTION_INSTALL)
		install(TARGETS AssetCompiler RUNTIME BUNDLE DESTINATION bin/)
	endif()

	foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
		string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER )
		set_target_properties(AssetCompiler PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
	endforeach()

	if(PLATFORM_APPLE_MACOS)
		add_custom_command(TARGET AssetCompiler POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vulkan \${CODESIGNING_FOLDER_PATH}/Contents/MacOS/vulkan
		)
		add_custom_command(TARGET AssetCompiler POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tint \${CODESIGNING_FOLDER_PATH}/Contents/MacOS/tint
		)
	endif()

	# Link in the asset compiler core
	if(NOT TARGET 72D83E65-B5D9-4CCD-8B8B-8227C805570E)
		add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Editor/AssetCompilerCore/Code" "${CMAKE_BINARY_DIR}/72D83E65-B5D9-4CCD-8B8B-8227C805570E")
	endif()
	LinkPlugin(AssetCompiler 72D83E65-B5D9-4CCD-8B8B-8227C805570E)
endif()

if(PLATFORM_APPLE) 
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/Private/main.cpp" PROPERTIES LANGUAGE OBJCXX)
endif()
