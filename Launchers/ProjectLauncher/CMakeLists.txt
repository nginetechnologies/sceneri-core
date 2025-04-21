include("${ENGINE_CMAKE_DIRECTORY}/MakeLauncher.cmake")
include("${ENGINE_CMAKE_DIRECTORY}/LinkPlugin.cmake")

MakeLauncher(ProjectLauncher "${CMAKE_CURRENT_LIST_DIR}")
if(OPTION_INSTALL)
	install(TARGETS ProjectLauncher RUNTIME BUNDLE DESTINATION bin/)
endif()

target_link_directories(ProjectLauncher PRIVATE Editor Renderer)

if (PLATFORM_APPLE)
	set_target_properties(ProjectLauncher PROPERTIES
		XCODE_ATTRIBUTE_PRODUCT_NAME "scenericore"
	)
endif()

if(PLATFORM_APPLE_MACOS)
	if (OPTION_SANDBOX)
		set_target_properties(ProjectLauncher PROPERTIES
			XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_LIST_DIR}/Private/Apple/macOS_sandboxed.entitlements"
			XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.scenericore.App"
			MACOSX_BUNDLE_GUI_IDENTIFIER "com.scenericore.App"
		)
	else()
		set_target_properties(ProjectLauncher PROPERTIES
			XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_LIST_DIR}/Private/Apple/macOS.entitlements"
			XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.scenericore.App"
			MACOSX_BUNDLE_GUI_IDENTIFIER "com.scenericore.App"
		)
	endif()

	if(PLATFORM_APPLE_MACOS)
		#add_custom_command(TARGET ProjectLauncher PRE_LINK
		#	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vulkan \${CODESIGNING_FOLDER_PATH}/Contents/MacOS/vulkan
		#)
		#add_custom_command(TARGET ProjectLauncher PRE_LINK
		#	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tint \${CODESIGNING_FOLDER_PATH}/Contents/MacOS/tint
		#)

		if (USE_STEAM_SDK)
			add_custom_command(TARGET ProjectLauncher PRE_LINK
				COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libsteam_api.dylib \${CODESIGNING_FOLDER_PATH}/Contents/MacOS/libsteam_api.dylib
			)
		endif()
	endif()
elseif(PLATFORM_APPLE)
	set_target_properties(ProjectLauncher PROPERTIES
		XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_LIST_DIR}/Private/Apple/${PLATFORM_NAME}.entitlements"
		XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.scenericore.App"
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.scenericore.App"
  )
endif()

if(OPTION_PACKAGE)
	if(HOST_PLATFORM_APPLE_MACOS)
		set(ASSETCOMPILER_PATH "${BINARY_OUTPUT_DIRECTORY}/${HOST_PLATFORM_NAME}/AssetCompiler.app/Contents/${HOST_PLATFORM_NAME}/AssetCompiler")
	else()
		set(ASSETCOMPILER_PATH "${BINARY_OUTPUT_DIRECTORY}/${HOST_PLATFORM_NAME}/AssetCompiler")
	endif()

	if(PLATFORM_DESKTOP AND NOT PLATFORM_APPLE)
	   add_dependencies(ProjectLauncher AssetCompiler)
   	endif()

	if(PLATFORM_APPLE)
		if(DEFINED PROJECT_FILE)
			set(PACKAGE_MACOS_COMMAND "")
			separate_arguments(PACKAGE_MACOS_COMMAND)

			add_custom_command(TARGET ProjectLauncher PRE_LINK
				COMMAND
				if [ \"\${IS_MACCATALYST}\" = YES ]\; then :\n
					${ASSETCOMPILER_PATH} -platform macCatalyst -config $<CONFIG> -engine \"${ENGINE_CONFIG_FILEPATH}\" +package_project_editor ${PROJECT_FILE} -package_directory \${CODESIGNING_FOLDER_PATH}/Contents/Resources\n
				elif  [ \"\${PLATFORM_APPLE_MACOS}\" = 1 ]\; then :\n
					${ASSETCOMPILER_PATH} -platform macOS -config $<CONFIG> -engine \"${ENGINE_CONFIG_FILEPATH}\" +package_project_editor ${PROJECT_FILE} -package_directory \${CODESIGNING_FOLDER_PATH}/Contents/Resources\n
				else\n
					${ASSETCOMPILER_PATH} -platform iOS -config $<CONFIG> -engine \"${ENGINE_CONFIG_FILEPATH}\" +package_project_editor ${PROJECT_FILE} -package_directory \${CODESIGNING_FOLDER_PATH}\n
				fi
			)
		else()
			add_custom_command(TARGET ProjectLauncher PRE_LINK
				COMMAND
				if [ \"\${IS_MACCATALYST}\" = YES ]\; then :\n
					${ASSETCOMPILER_PATH} -platform macCatalyst -config $<CONFIG> -engine \"${ENGINE_CONFIG_FILEPATH}\" +package_editor ${ENGINE_CONFIG_FILEPATH} -package_directory \${CODESIGNING_FOLDER_PATH}/Contents/Resources\n
				elif  [ ${PLATFORM_APPLE_MACOS} = 1 ]\; then :\n
					${ASSETCOMPILER_PATH} -platform macOS -config $<CONFIG> -engine \"${ENGINE_CONFIG_FILEPATH}\" +package_editor ${ENGINE_CONFIG_FILEPATH} -package_directory \${CODESIGNING_FOLDER_PATH}/Contents/Resources\n
				else\n
					${ASSETCOMPILER_PATH} -platform iOS -config $<CONFIG> -engine \"${ENGINE_CONFIG_FILEPATH}\" +package_editor ${ENGINE_CONFIG_FILEPATH} -package_directory \${CODESIGNING_FOLDER_PATH}\n
				fi
			)
		endif()
	elseif(PLATFORM_ANDROID)
		if(DEFINED PROJECT_FILE)
			add_custom_command(TARGET ProjectLauncher PRE_LINK
					COMMAND ${ASSETCOMPILER_PATH} -platform ${PLATFORM_NAME} -config $<CONFIG> -engine \"${ENGINE_CONFIG_FILEPATH}\" +package_project_editor ${PROJECT_FILE} -package_directory \"${ANDROID_ASSETS_DIRECTORIES}\"
			)
		else()
			add_custom_command(TARGET ProjectLauncher PRE_LINK
					COMMAND ${ASSETCOMPILER_PATH} -platform ${PLATFORM_NAME} -config $<CONFIG> -engine \"${ENGINE_CONFIG_FILEPATH}\" +package_editor ${ENGINE_CONFIG_FILEPATH} -package_directory \"${ANDROID_ASSETS_DIRECTORIES}\"
			)
		endif()
	elseif(PLATFORM_EMSCRIPTEN OR PLATFORM_WINDOWS OR PLATFORM_LINUX)
		if(DEFINED PROJECT_FILE)
			add_custom_command(TARGET ProjectLauncher PRE_LINK
					COMMAND ${ASSETCOMPILER_PATH} -platform ${PLATFORM_NAME} -config $<CONFIG> -engine \"${ENGINE_CONFIG_FILEPATH}\" +package_project_editor ${PROJECT_FILE} -package_directory \"$<TARGET_FILE_DIR:ProjectLauncher>\"
			)
		else()
			add_custom_command(TARGET ProjectLauncher PRE_LINK
					COMMAND ${ASSETCOMPILER_PATH} -platform ${PLATFORM_NAME} -config $<CONFIG> -engine \"${ENGINE_CONFIG_FILEPATH}\" +package_editor ${ENGINE_CONFIG_FILEPATH} -package_directory \"$<TARGET_FILE_DIR:ProjectLauncher>\"
			)
		endif()
	endif()
endif()

if(PLATFORM_APPLE) 
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/Private/Launcher.cpp" PROPERTIES LANGUAGE OBJCXX)
endif()

# Include the sample plug-in
add_subdirectory(SampleLauncherPlugin/Code)
LinkPlugin(ProjectLauncher 84cb6ecd-1f00-4178-8434-5853cebb8601)