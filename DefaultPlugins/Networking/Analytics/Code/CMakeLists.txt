cmake_minimum_required (VERSION 3.14)

include("${ENGINE_CMAKE_DIRECTORY}/MakePlugin.cmake")
include("${ENGINE_CMAKE_DIRECTORY}/DeployBinaryDependencies.cmake")

MakePlugin(50472FD7-1240-418B-B988-4BAD607632B6 ${CMAKE_CURRENT_LIST_DIR} "Analytics" "lib" "bin" "../Analytics.nplugin")

if(PLATFORM_APPLE_IOS AND NOT PLATFORM_APPLE_VISIONOS AND NOT PLATFORM_APPLE_MACCATALYST)
	if (NOT DEFINED OPTION_APPSFLYER_DEV_KEY)
		set(OPTION_APPSFLYER_DEV_KEY "" CACHE STRING "The AppsFlyer dev key to use")
	endif()
	target_compile_definitions(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE APPSFLYER_DEV_KEY="${OPTION_APPSFLYER_DEV_KEY}")
	if (NOT DEFINED OPTION_APPSFLYER_APPLE_APPID)
		set(OPTION_APPSFLYER_APPLE_APPID "" CACHE STRING "The AppsFlyer apple app ID to use")
	endif()
	target_compile_definitions(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE APPSFLYER_APPLE_APPID="${OPTION_APPSFLYER_APPLE_APPID}")

	set(APPFLYER_DIRECTORY_PATH "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/AppsFlyer" CACHE INTERNAL "")
	set(APPFLYER_LIB_PATH "${APPFLYER_DIRECTORY_PATH}/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/AppsFlyerLib.framework/AppsFlyerLib" CACHE INTERNAL "")
	target_link_libraries(50472FD7-1240-418B-B988-4BAD607632B6 PUBLIC "${APPFLYER_LIB_PATH}")

	target_include_directories(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE "${APPFLYER_DIRECTORY_PATH}/include")

	target_compile_definitions(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE USE_APPFLYER_SDK=1)
else()
	target_compile_definitions(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE USE_APPFLYER_SDK=0)
endif()

option(OPTION_SENTRY "Enable Sentry monitoring" ON)
if (OPTION_SENTRY)
	set(SENTRY_DIRECTORY_PATH "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/sentry" CACHE INTERNAL "")

	if (NOT DEFINED OPTION_SENTRY_DSN)
		set(OPTION_SENTRY_DSN "" CACHE STRING "The Sentry DSN to use")
	endif()
	target_compile_definitions(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE SENTRY_DSN="${OPTION_SENTRY_DSN}")

	if (PLATFORM_APPLE)
		target_link_libraries(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE "${SENTRY_DIRECTORY_PATH}/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/Sentry.framework/Sentry")
		target_include_directories(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE "${SENTRY_DIRECTORY_PATH}/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/Sentry.framework/Headers")
		target_compile_definitions(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE USE_SENTRY=1)

		target_link_libraries(50472FD7-1240-418B-B988-4BAD607632B6 PUBLIC "-framework MetricKit")
	elseif(PLATFORM_WINDOWS OR PLATFORM_LINUX)
		# Use the native SDK
		target_link_libraries(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE "${SENTRY_DIRECTORY_PATH}/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/${CMAKE_STATIC_LIBRARY_PREFIX}sentry${CMAKE_STATIC_LIBRARY_SUFFIX}")
		target_link_libraries(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE "${SENTRY_DIRECTORY_PATH}/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/${CMAKE_STATIC_LIBRARY_PREFIX}crashpad_client${CMAKE_STATIC_LIBRARY_SUFFIX}")
		target_link_libraries(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE "${SENTRY_DIRECTORY_PATH}/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/${CMAKE_STATIC_LIBRARY_PREFIX}crashpad_util${CMAKE_STATIC_LIBRARY_SUFFIX}")
		target_link_libraries(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE "${SENTRY_DIRECTORY_PATH}/lib/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/${CMAKE_STATIC_LIBRARY_PREFIX}mini_chromium${CMAKE_STATIC_LIBRARY_SUFFIX}")
		target_include_directories(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE "${SENTRY_DIRECTORY_PATH}/include")
		DeployBinaryDependency("${SENTRY_DIRECTORY_PATH}/bin/${PLATFORM_NAME}/${PLATFORM_ARCHITECTURE}/crashpad_handler${CMAKE_EXECUTABLE_SUFFIX}")
		target_compile_definitions(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE USE_SENTRY=1)

		if (PLATFORM_WINDOWS)
			target_link_libraries(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE Winhttp.lib)
			target_link_libraries(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE Version.lib)
			target_link_libraries(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE Dbghelp.lib)
		endif()
	elseif(PLATFORM_ANDROID)
		target_compile_definitions(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE USE_SENTRY=1)
  		find_package(sentry-native-ndk REQUIRED CONFIG)

		target_link_libraries(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE
			sentry-native-ndk::sentry-android
			sentry-native-ndk::sentry
		)
	else()
		target_compile_definitions(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE USE_SENTRY=0)
	endif()
else()
	target_compile_definitions(50472FD7-1240-418B-B988-4BAD607632B6 PRIVATE USE_SENTRY=0)
endif()

if(PLATFORM_APPLE)
	target_link_libraries(50472FD7-1240-418B-B988-4BAD607632B6 PUBLIC "-framework UserNotifications")
endif()

if(NOT TARGET 4732DB7A-619B-421E-8569-BF31F607BBB7)
	add_subdirectory("${ENGINE_CODE_DIRECTORY}/DefaultPlugins/Networking/Http/Code" "${CMAKE_BINARY_DIR}/4732DB7A-619B-421E-8569-BF31F607BBB7")
endif()
LinkPlugin(50472FD7-1240-418B-B988-4BAD607632B6 4732DB7A-619B-421E-8569-BF31F607BBB7)

if(NOT TARGET 087DEE23-94E5-44D9-88E9-AF82B784F025)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Networking/Backend/Code" "${CMAKE_BINARY_DIR}/087DEE23-94E5-44D9-88E9-AF82B784F025")
endif()
LinkPlugin(50472FD7-1240-418B-B988-4BAD607632B6 087DEE23-94E5-44D9-88E9-AF82B784F025)

if(NOT TARGET 72D83E65-B5D9-4CCD-8B8B-8227C805570E)
	add_subdirectory("${ENGINE_CODE_DIRECTORY}/DefaultPlugins/Editor/AssetCompilerCore/Code" "${CMAKE_BINARY_DIR}/72D83E65-B5D9-4CCD-8B8B-8227C805570E")
endif()
LinkPlugin(50472FD7-1240-418B-B988-4BAD607632B6 72D83E65-B5D9-4CCD-8B8B-8227C805570E)

if(PLATFORM_APPLE) 
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/Private/Plugin.cpp" PROPERTIES LANGUAGE OBJCXX)
endif()
