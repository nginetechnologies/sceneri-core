cmake_minimum_required(VERSION 3.14)

include("${ENGINE_CMAKE_DIRECTORY}/MakePlugin.cmake")

MakePlugin(F6C31290-CB03-452C-B3DE-78F19A8CF943 ${CMAKE_CURRENT_LIST_DIR} "PhysicsCore" "lib" "bin" "../Physics.nplugin")

target_include_directories(F6C31290-CB03-452C-B3DE-78F19A8CF943 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/Public/PhysicsCore/3rdparty/jolt")

file(
	GLOB_RECURSE _PHYSICS_private_source_list
	LIST_DIRECTORIES false
	"${CMAKE_CURRENT_LIST_DIR}/Public/PhysicsCore/*.cpp*"
)

target_sources(F6C31290-CB03-452C-B3DE-78F19A8CF943 PRIVATE ${_PHYSICS_private_source_list})

if(NOT TARGET CBFE8887-9CB2-4703-9328-94F0B8223ECE)
	add_subdirectory("${ENGINE_CODE_DIRECTORY}/DefaultPlugins/Networking/Core/Code" "${CMAKE_BINARY_DIR}/CBFE8887-9CB2-4703-9328-94F0B8223ECE")
endif()
LinkPlugin(F6C31290-CB03-452C-B3DE-78F19A8CF943 CBFE8887-9CB2-4703-9328-94F0B8223ECE)
