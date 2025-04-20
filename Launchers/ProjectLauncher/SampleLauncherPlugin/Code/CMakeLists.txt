cmake_minimum_required (VERSION 3.14)

include("${ENGINE_CMAKE_DIRECTORY}/MakePlugin.cmake")
include("${ENGINE_CMAKE_DIRECTORY}/LinkPlugin.cmake")

MakePlugin(84cb6ecd-1f00-4178-8434-5853cebb8601 ${CMAKE_CURRENT_LIST_DIR} "SamplerLauncher" "lib" "bin" "../SamplerLauncher.nplugin")

# Link to the HTTP plug-in
if(NOT TARGET 4732DB7A-619B-421E-8569-BF31F607BBB7)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Networking/Http/Code" "${CMAKE_BINARY_DIR}/4732DB7A-619B-421E-8569-BF31F607BBB7")
endif()
LinkPlugin(84cb6ecd-1f00-4178-8434-5853cebb8601 4732DB7A-619B-421E-8569-BF31F607BBB7)

# Link to the audio plug-in
if(NOT TARGET a4bbf061-5f65-43b1-acc0-711f28b3bb56)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Audio/AudioCore/Code" "${CMAKE_BINARY_DIR}/a4bbf061-5f65-43b1-acc0-711f28b3bb56")
endif()
LinkPlugin(84cb6ecd-1f00-4178-8434-5853cebb8601 a4bbf061-5f65-43b1-acc0-711f28b3bb56)

# Link to the animation plug-in
if(NOT TARGET 4CC21FD4-730F-475D-9807-FBF9E5595308)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Animation/Code" "${CMAKE_BINARY_DIR}/4CC21FD4-730F-475D-9807-FBF9E5595308")
endif()
LinkPlugin(84cb6ecd-1f00-4178-8434-5853cebb8601 4CC21FD4-730F-475D-9807-FBF9E5595308)

# Link to the physics plug-in
if(NOT TARGET F6C31290-CB03-452C-B3DE-78F19A8CF943)
add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Physics/PhysicsCore/Code" "${CMAKE_BINARY_DIR}/F6C31290-CB03-452C-B3DE-78F19A8CF943")
endif()
LinkPlugin(84cb6ecd-1f00-4178-8434-5853cebb8601 F6C31290-CB03-452C-B3DE-78F19A8CF943)

# Link to the networking (multiplayer) plug-in
if(NOT TARGET CBFE8887-9CB2-4703-9328-94F0B8223ECE)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Networking/Core/Code" "${CMAKE_BINARY_DIR}/CBFE8887-9CB2-4703-9328-94F0B8223ECE")
endif()
LinkPlugin(84cb6ecd-1f00-4178-8434-5853cebb8601 CBFE8887-9CB2-4703-9328-94F0B8223ECE)

# Link to the widgets (UI) plug-in
if(NOT TARGET AAEA95B0-9B18-442D-B043-0629481E8C95)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Widgets/Code" "${CMAKE_BINARY_DIR}/AAEA95B0-9B18-442D-B043-0629481E8C95")
endif()
LinkPlugin(84cb6ecd-1f00-4178-8434-5853cebb8601 AAEA95B0-9B18-442D-B043-0629481E8C95)

# Link to the deferred shading / PBR plug-in
if(NOT TARGET B7921721-4029-47CE-873C-5D356D604AA9)
	add_subdirectory("${CORE_ROOT_DIRECTORY}/DefaultPlugins/Graphics/DeferredShading/Code" "${CMAKE_BINARY_DIR}/B7921721-4029-47CE-873C-5D356D604AA9")
endif()
LinkPlugin(84cb6ecd-1f00-4178-8434-5853cebb8601 B7921721-4029-47CE-873C-5D356D604AA9)


if(PLATFORM_APPLE) 
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/Private/Plugin.cpp" PROPERTIES LANGUAGE OBJCXX)
endif()
