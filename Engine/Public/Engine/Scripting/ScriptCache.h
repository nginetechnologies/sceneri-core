#pragma once

#include "Engine/Scripting/Interpreter/Environment.h"
#include "Engine/Scripting/Interpreter/ScriptFunctionCache.h"
#include "Engine/Scripting/Interpreter/ScriptTableCache.h"

#include <Engine/Asset/AssetType.h>

#include <Engine/Scripting/Parser/AST/Graph.h>
#include <Engine/Scripting/Compiler/Object.h>
#include <Engine/Scripting/ScriptIdentifier.h>

#include <Common/System/SystemType.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Memory/Optional.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Memory/SharedPtr.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Threading/Mutexes/SharedMutex.h>

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Scripting
{
	struct Script
	{
		Script()
		{
		}
		Script(const Script&) = delete;
		Script(Script&& other)
			: m_compiledFunctions(Move(other.m_compiledFunctions))
			, m_pAstGraph(Move(other.m_pAstGraph))
		{
		}
		Script& operator=(const Script&) = delete;
		Script& operator=(Script&& other) = delete;

		mutable Threading::SharedMutex m_functionMutex;
		UnorderedMap<Guid, UniquePtr<FunctionObject>, Guid::Hash> m_compiledFunctions;
		UniquePtr<AST::Graph> m_pAstGraph;
	};

	struct ScriptCache final : public Asset::Type<ScriptIdentifier, Script>
	{
	public:
		static constexpr System::Type SystemType = System::Type::ScriptCache;

		using BaseType = Asset::Type<ScriptIdentifier, Script>;

		ScriptCache(Asset::Manager& assetManager);
		ScriptCache(ScriptCache&&) = delete;
		ScriptCache(const ScriptCache&) = delete;
		ScriptCache& operator=(ScriptCache&&) = delete;
		ScriptCache& operator=(const ScriptCache&) = delete;
		~ScriptCache();

		[[nodiscard]] ScriptIdentifier FindOrRegisterAsset(Asset::Guid guid);
		[[nodiscard]] ScriptIdentifier RegisterAsset(Asset::Guid guid);

		[[nodiscard]] Optional<const AST::Graph*> FindAstGraph(ScriptIdentifier identifier) const;
		[[nodiscard]] Optional<const FunctionObject*> FindFunction(ScriptIdentifier identifier, const Guid localFunctionIdentifier) const;

		[[nodiscard]] SharedPtr<Environment> GetIntermediateEnvironment() const;

		using ScriptLoadEvent = ThreadSafe::Event<EventCallbackResult(void*, const ScriptIdentifier), 24, false>;
		using ScriptLoadListenerData = ScriptLoadEvent::ListenerData;
		using ScriptLoadListenerIdentifier = ScriptLoadEvent::ListenerIdentifier;

		[[nodiscard]] Optional<Threading::Job*>
		TryLoadScript(ScriptIdentifier identifier, const Guid localFunctionIdentifier, ScriptLoadListenerData&& listenerData);
		[[nodiscard]] Optional<Threading::Job*> TryLoadAstGraph(ScriptIdentifier identifier, ScriptLoadListenerData&& listenerData);
		void RemoveListener(ScriptIdentifier identifier, const ScriptLoadListenerIdentifier listenerIdentifier);

		[[nodiscard]] bool AssignAstGraph(ScriptIdentifier identifier, AST::Graph&& astGraph);
		[[nodiscard]] bool
		AssignCompiledFunction(ScriptIdentifier identifier, const Guid localFunctionIdentifier, UniquePtr<FunctionObject>&& pFunctionObject);
		void RemoveCompiledFunction(ScriptIdentifier identifier, const Guid localFunctionIdentifier);

	protected:
		friend struct LoadScriptJob;
		void OnLoadedInterpreted(ScriptIdentifier identifier);
		void OnInterpretedLoadFailed(ScriptIdentifier identifier);
		void OnLoadedCompiled(ScriptIdentifier identifier);
		void OnCompiledLoadFailed(ScriptIdentifier identifier);
		
		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override final;

	private:
		Threading::AtomicIdentifierMask<ScriptIdentifier> m_loadingCompiledScripts;
		Threading::AtomicIdentifierMask<ScriptIdentifier> m_loadingInterpretedScripts;
		TIdentifierArray<ScriptLoadEvent, ScriptIdentifier> m_interpretedScriptLoadEvents;
		TIdentifierArray<ScriptLoadEvent, ScriptIdentifier> m_compiledScriptLoadEvents;

		UniquePtr<ScriptFunctionCache> m_pFunctionCache;
		UniquePtr<ScriptTableCache> m_pTableCache;
		SharedPtr<Environment> m_pIntermediateEnvironment;
	};
}
