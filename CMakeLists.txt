cmake_minimum_required (VERSION 3.21)
set(CORE_ROOT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
set(COMMON_ROOT_DIRECTORY "${CORE_ROOT_DIRECTORY}/Common")

include("${COMMON_ROOT_DIRECTORY}/cmake/InitialSettings.cmake")

include("${COMMON_ROOT_DIRECTORY}/cmake/json-cmake/JSONParser.cmake")
get_filename_component(ENGINE_CONFIG_FILEPATH "sceneri.ngine" REALPATH BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")

file (STRINGS "${ENGINE_CONFIG_FILEPATH}" ENGINE_CONFIG_CONTENTS)
sbeParseJson(ENGINE_CONFIG_JSON ENGINE_CONFIG_CONTENTS)

message("Engine: ${ENGINE_CONFIG_JSON.name} ${ENGINE_CONFIG_JSON.version}")
if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
	project("${ENGINE_CONFIG_JSON.name}" VERSION ${ENGINE_CONFIG_JSON.version} LANGUAGES CXX C Swift OBJC OBJCXX)
else()
	project("${ENGINE_CONFIG_JSON.name}" VERSION ${ENGINE_CONFIG_JSON.version} LANGUAGES CXX C)
endif()

get_filename_component(ENGINE_CODE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE CACHE PATH "" FORCE)
set(ENGINE_ROOT_DIRECTORY ${ENGINE_CODE_DIRECTORY} CACHE INTERNAL "")

include("${COMMON_ROOT_DIRECTORY}/cmake/InitialConfiguration.cmake")

option(OPTION_BUILD_UNIT_TESTS "Build unit tests" OFF)
option(OPTION_BUILD_FEATURE_TESTS "Build feature tests" OFF)
option(BUILD_STATIC_LIBS "Build static libraries" ON)

if(OPTION_BUILD_UNIT_TESTS OR OPTION_BUILD_FEATURE_TESTS)
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
	add_subdirectory(${ENGINE_CODE_DIRECTORY}/Common/Public/Common/3rdparty/googletest EXCLUDE_FROM_ALL)

	set_target_properties(gtest PROPERTIES FOLDER "Tests/gtest")
	set_target_properties(gtest_main PROPERTIES FOLDER "Tests/gtest")

	enable_testing()
endif()

include("${COMMON_ROOT_DIRECTORY}/CommonLibrary.cmake")
add_subdirectory(Engine)
add_subdirectory(Renderer)
add_subdirectory(Launchers)
add_subdirectory(AssetCompiler)

export(TARGETS CommonAPI Common EngineAPI Engine RendererAPI Renderer FILE EngineConfig.cmake)

if(OPTION_INSTALL)
	install(FILES ${ENGINE_ROOT_DIRECTORY}/bin/${PLATFORM_NAME}/$<CONFIG>/sceneri.ngine
        DESTINATION /
	)
	install(FILES ${ENGINE_ROOT_DIRECTORY}/bin/${PLATFORM_NAME}/$<CONFIG>/AvailablePlugins.json
        DESTINATION /
	)
	install(FILES ${ENGINE_ROOT_DIRECTORY}/bin/${PLATFORM_NAME}/$<CONFIG>/PackagedBundle.json
        DESTINATION /
	)
	install(FILES ${ENGINE_ROOT_DIRECTORY}/bin/${PLATFORM_NAME}/$<CONFIG>/EngineAssets
        DESTINATION /
	)
	install(FILES ${ENGINE_ROOT_DIRECTORY}/bin/${PLATFORM_NAME}/$<CONFIG>/EngineAssets.nassetdb
        DESTINATION /
	)
	install(FILES ${ENGINE_ROOT_DIRECTORY}/bin/${PLATFORM_NAME}/$<CONFIG>/Plug-ins
        DESTINATION /
	)

	set(CPACK_PACKAGE_NAME "${ENGINE_CONFIG_JSON.name}")
	set(CPACK_PACKAGE_VENDOR "${ENGINE_CONFIG_JSON.creator}")
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${ENGINE_CONFIG_JSON.description}")
	set(CPACK_PACKAGE_EXECUTABLES Editor;${ENGINE_CONFIG_JSON.name})
	set(CPACK_PACKAGE_HOMEPAGE_URL "${ENGINE_CONFIG_JSON.website_url}")
	set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/pkg")
	set(CPACK_PACKAGE_INSTALL_DIRECTORY "${ENGINE_CONFIG_JSON.name} ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}")
	set(CPACK_PACKAGE_VERSION "${ENGINE_CONFIG_JSON.version}")
	set(CPACK_PACKAGE_VERSION_MAJOR "${CMAKE_PROJECT_VERSION_MAJOR}")
	set(CPACK_PACKAGE_VERSION_MINOR "${CMAKE_PROJECT_VERSION_MINOR}")
	set(CPACK_PACKAGE_VERSION_PATCH "${CMAKE_PROJECT_VERSION_PATCH}")
	set(CPACK_PACKAGING_INSTALL_PREFIX "")
	set(CPACK_PACKAGE_FILE_NAME "${ENGINE_CONFIG_JSON.version}")
	set(CPACK_TOPLEVEL_TAG "")
	set(CPACK_THREADS 0)

	set (CPACK_RESOURCE_FILE_LICENSE
     "${CMAKE_CURRENT_SOURCE_DIR}/Launchers/Editor/Private/Windows/TermsOfService.txt")
	set (CPACK_PACKAGE_ICON
     "${CMAKE_CURRENT_SOURCE_DIR}/Launchers/Editor/Private/Icon/Icon-01.ico")
	set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/Launchers/Editor/Private/Icon/Icon-01.ico")
	set(CPACK_WIX_UI_BANNER "${CMAKE_CURRENT_SOURCE_DIR}/Launchers/Editor/Private/Windows/InstallerBanner.bmp")
	set(CPACK_WIX_UI_DIALOG "${CMAKE_CURRENT_SOURCE_DIR}/Launchers/Editor/Private/Windows/InstallerDialog.png")

	set(CMAKE_INSTALL_PREFIX "")
	set(CPACK_SET_DESTDIR FALSE)

	# Suppress validation.  It does not work without
	# an interactive session or an admin account.
	# https://github.com/wixtoolset/issues/issues/3968
 	list(APPEND CPACK_WIX_LIGHT_EXTRA_FLAGS "-sval")
endif()

if(PLATFORM_WINDOWS AND OPTION_INSTALL)
	set(CPACK_GENERATOR WIX)

	set(CPACK_WIX_UPGRADE_GUID "${ENGINE_CONFIG_JSON.guid}")
	set(CPACK_WIX_PRODUCT_GUID "${ENGINE_CONFIG_JSON.guid}")

	include(CPack)
endif()

sbeClearJson(ENGINE_CONFIG_JSON)