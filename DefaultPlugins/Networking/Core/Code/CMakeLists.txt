cmake_minimum_required (VERSION 3.14)

include("${ENGINE_CMAKE_DIRECTORY}/MakePlugin.cmake")

MakePlugin(CBFE8887-9CB2-4703-9328-94F0B8223ECE ${CMAKE_CURRENT_LIST_DIR} "NetworkingCore" "lib" "bin" "../Core.nplugin")

target_include_directories(CBFE8887-9CB2-4703-9328-94F0B8223ECE PUBLIC "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/enet/include/")

if(PLATFORM_WINDOWS)
	target_link_libraries(CBFE8887-9CB2-4703-9328-94F0B8223ECE PUBLIC "Ws2_32.lib")
endif()

if(OPTION_BUILD_FEATURE_TESTS)
	MakeFeatureTests(CBFE8887-9CB2-4703-9328-94F0B8223ECE NetworkingCore)
	LinkStaticLibrary(NetworkingCoreFeatureTests Common)
	LinkStaticLibrary(NetworkingCoreFeatureTests Engine)
	LinkStaticLibrary(NetworkingCoreFeatureTests Renderer)
endif()
