cmake_minimum_required (VERSION 3.14)

include("${ENGINE_CMAKE_DIRECTORY}/MakePlugin.cmake")

MakePlugin(EAF8C037-4ADF-4A97-99FD-915341D1854C ${CMAKE_CURRENT_LIST_DIR} "GameFramework" "lib" "bin" "../GameFramework.nplugin")

if(NOT TARGET 4CC21FD4-730F-475D-9807-FBF9E5595308)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Animation/Code" "${CMAKE_BINARY_DIR}/4CC21FD4-730F-475D-9807-FBF9E5595308")
endif()
LinkPlugin(EAF8C037-4ADF-4A97-99FD-915341D1854C 4CC21FD4-730F-475D-9807-FBF9E5595308)

if(NOT TARGET F6C31290-CB03-452C-B3DE-78F19A8CF943)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Physics/PhysicsCore/Code" "${CMAKE_BINARY_DIR}/F6C31290-CB03-452C-B3DE-78F19A8CF943")
endif()
LinkPlugin(EAF8C037-4ADF-4A97-99FD-915341D1854C F6C31290-CB03-452C-B3DE-78F19A8CF943)

if(NOT TARGET B0ABCB76-705B-4244-9474-F7B29131FC70)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/VisualDebug/Code" "${CMAKE_BINARY_DIR}/B0ABCB76-705B-4244-9474-F7B29131FC70")
endif()
LinkPlugin(EAF8C037-4ADF-4A97-99FD-915341D1854C B0ABCB76-705B-4244-9474-F7B29131FC70)

if(NOT TARGET AAEA95B0-9B18-442D-B043-0629481E8C95)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Widgets/Code" "${CMAKE_BINARY_DIR}/AAEA95B0-9B18-442D-B043-0629481E8C95")
endif()
LinkPlugin(EAF8C037-4ADF-4A97-99FD-915341D1854C AAEA95B0-9B18-442D-B043-0629481E8C95)

if(NOT TARGET a4bbf061-5f65-43b1-acc0-711f28b3bb56)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Audio/AudioCore/Code" "${CMAKE_BINARY_DIR}/a4bbf061-5f65-43b1-acc0-711f28b3bb56")
endif()
LinkPlugin(EAF8C037-4ADF-4A97-99FD-915341D1854C a4bbf061-5f65-43b1-acc0-711f28b3bb56)

if(NOT TARGET 087DEE23-94E5-44D9-88E9-AF82B784F025)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Networking/Backend/Code" "${CMAKE_BINARY_DIR}/087DEE23-94E5-44D9-88E9-AF82B784F025")
endif()
LinkPlugin(EAF8C037-4ADF-4A97-99FD-915341D1854C 087DEE23-94E5-44D9-88E9-AF82B784F025)

if(NOT TARGET CBFE8887-9CB2-4703-9328-94F0B8223ECE)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Networking/Core/Code" "${CMAKE_BINARY_DIR}/CBFE8887-9CB2-4703-9328-94F0B8223ECE")
endif()
LinkPlugin(EAF8C037-4ADF-4A97-99FD-915341D1854C CBFE8887-9CB2-4703-9328-94F0B8223ECE)

if(NOT TARGET 50472FD7-1240-418B-B988-4BAD607632B6)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Networking/Analytics/Code" "${CMAKE_BINARY_DIR}/50472FD7-1240-418B-B988-4BAD607632B6")
endif()
LinkPlugin(EAF8C037-4ADF-4A97-99FD-915341D1854C 50472FD7-1240-418B-B988-4BAD607632B6)
