#pragma once

#include "Engine/Engine.h"
#include "Event/EventManager.h"
#include "IO/Filesystem.h"
#include "Input/InputManager.h"
#include "Reflection/Registry.h"
#include "Scripting/ScriptCache.h"
#include "Entity/Manager.h"
#include "Tag/TagRegistry.h"
#include "DataSource/DataSourceCache.h"
#include "Asset/AssetManager.h"
#include "Threading/JobManager.h"
#include "Project/Project.h"

#include <Common/Application/Application.h>
#include <Common/Project System/EngineInfo.h>
#include <Common/CommandLine/CommandLineArguments.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Renderer/Renderer.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/Platform/NoUniqueAddress.h>

#include <Common/Threading/ThreadId.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/IntermediateStage.h>
#include <Common/Function/ThreadSafeEvent.h>

namespace ngine
{
	struct EngineSystems final
	{
		EngineSystems(const CommandLine::InitializationParameters& commandLineArguments);

		Threading::JobBatch m_startupJobBatch;

		// Helper class to make sure that EngineQuery gets initialized first and receives the engines address.
		struct EngineQueryHelper
		{
			EngineQueryHelper(EngineSystems&);
			~EngineQueryHelper();
		};
		NO_UNIQUE_ADDRESS EngineQueryHelper m_engineQueryHelper;

		Engine m_engine;

		Threading::EngineJobManager m_jobManager;
		IO::Filesystem m_filesystem;
		Events::Manager m_eventManager;
		DataSource::Cache m_dataSourceCache;
		Tag::Registry m_tagRegistry;
		Asset::EngineManager m_assetManager;
		Project m_currentProject;
		Reflection::EngineRegistry m_reflectionRegistry;
		Scripting::ScriptCache m_scriptCache;
		Entity::Manager m_entityManager;
		Input::Manager m_inputManager;
		Rendering::Renderer m_renderer;
	};
}
