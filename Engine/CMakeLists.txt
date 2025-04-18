include("${ENGINE_CMAKE_DIRECTORY}/DeployBinaryDependencies.cmake")

MakeStaticModule(Engine ${CMAKE_CURRENT_LIST_DIR} Engine)

set_target_properties(Engine PROPERTIES FOLDER Engine)
target_link_libraries(Engine PUBLIC CommonAPI RendererAPI)

MakeUnitTests(Engine Engine)
if (OPTION_BUILD_UNIT_TESTS)
	LinkStaticLibrary(EngineUnitTests Common)
	LinkStaticLibrary(EngineUnitTests Engine)
	# Currently needed because the engine imports the renderer
	# Should be removed when the renderer is a plug-in
	LinkStaticLibrary(EngineUnitTests Renderer)
endif()

MakeFeatureTests(Engine Engine)
if(OPTION_BUILD_FEATURE_TESTS)
	LinkStaticLibrary(EngineFeatureTests Common)
	# Currently needed because the engine imports the renderer
	# Should be removed when the renderer is a plug-in
	LinkStaticLibrary(EngineFeatureTests Renderer)
endif()

if(PLATFORM_APPLE) 
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/Private/Input/Feedback/Feedback.cpp" PROPERTIES LANGUAGE OBJCXX)
endif()

if (OPTION_CODE_HOT_RELOAD)
	target_include_directories(Engine PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/Live++/include")
	target_compile_definitions(Engine PRIVATE HAS_LIVEPP=1)

	foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
		string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
		file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER}})

		file(COPY "${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/Live++" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER}}")
	endforeach()

	if(OPTION_INSTALL)
		install(FILES ${CMAKE_CURRENT_LIST_DIR}/Private/3rdparty/Live+
			DESTINATION bin/
		)
	endif()
else()
	target_compile_definitions(Engine PRIVATE HAS_LIVEPP=0)
endif()