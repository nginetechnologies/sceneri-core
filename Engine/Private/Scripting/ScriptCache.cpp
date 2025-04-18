#include "Scripting/ScriptCache.h"

#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Asset/AssetManager.h>

#include <Engine/Asset/AssetType.inl>

#include <Engine/Scripting/ScriptAssetType.h>
#include <Engine/Scripting/Parser/Lexer.h>
#include <Engine/Scripting/Parser/Parser.h>
#include <Engine/Scripting/Parser/AST/Graph.h>
#include <Engine/Scripting/Interpreter/Resolver.h>
#include <Engine/Scripting/Compiler/Compiler.h>

#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Variant.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/System/Query.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Serialization/SerializedData.h>
#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>
#include <Common/Asset/Format/Guid.h>
#include <Common/IO/Path.h>
#include <Common/IO/Format/Path.h>
#include <Common/IO/Log.h>
#include <Common/Reflection/Type.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Scripting
{
	struct LoadScriptJob : public Threading::Job
	{
		enum class Mode : uint8
		{
			Interpreted,
			Compiled
		};

		LoadScriptJob(const ScriptIdentifier identifier, const Guid functionGuid, const Mode mode)
			: Job(Priority::LoadLogic)
			, m_identifier(identifier)
			, m_functionGuid(functionGuid)
			, m_mode(mode)
		{
		}

		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override
		{
			ScriptCache& scriptCache = System::Get<ScriptCache>();
			switch (m_state)
			{
				case State::AwaitingStart:
				{
					m_state = State::AwaitingDataLoad;

					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					const Asset::Guid assetGuid = scriptCache.GetAssetGuid(m_identifier);

					Threading::Job* pLoadJob = nullptr;

					switch (m_mode)
					{
						case Mode::Interpreted:
							pLoadJob = assetManager.RequestAsyncLoadAssetMetadata(
								assetGuid,
								Threading::JobPriority::LoadLogic,
								[&abstractSyntaxTreeData = m_abstractSyntaxTreeData](const ConstByteView data)
								{
									Assert(data.HasElements());
									if (LIKELY(data.HasElements()))
									{
										abstractSyntaxTreeData = Serialization::Data(
											ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
										);
									}
								}
							);
							break;
						case Mode::Compiled:
						{
							Assert(m_functionGuid.IsValid());
							const IO::Path assetPath = assetManager.GetAssetPath(assetGuid);
							const IO::Path binaryFilePath = IO::Path::Combine(
								assetPath.GetParentPath(),
								IO::Path::Merge(m_functionGuid.ToString().GetView(), ScriptAssetType::FunctionExtension)
							);
							pLoadJob = assetManager.RequestAsyncLoadAssetPath(
								assetGuid,
								binaryFilePath,
								Threading::JobPriority::LoadLogic,
								[&code = m_code](const ConstByteView data)
								{
									if (data.HasElements())
									{
										code.Resize((uint32)data.GetDataSize());
										ByteView(code.GetView()).CopyFrom(data);
									}
								},
								{},
								Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
							);
						}
						break;
					}

					Assert(pLoadJob != nullptr);
					if (pLoadJob != nullptr)
					{
						pLoadJob->AddSubsequentStage(*this);
						pLoadJob->Queue(thread);
					}
				}
				break;
				case State::AwaitingDataLoad:
					break;
				default:
					ExpectUnreachable();
			}
		}

		virtual Result OnExecute(Threading::JobRunnerThread&) override
		{
			ScriptCache& scriptCache = System::Get<ScriptCache>();
			switch (m_state)
			{
				case State::AwaitingDataLoad:
				{
					switch (m_mode)
					{
						case Mode::Interpreted:
						{
							const Serialization::Reader astReader(m_abstractSyntaxTreeData);
							AST::Graph astGraph;
							if (UNLIKELY(!astGraph.Serialize(astReader)))
							{
								LogError("Failed to deserialize AST asset '{}'", scriptCache.GetAssetGuid(m_identifier));
								scriptCache.OnInterpretedLoadFailed(m_identifier);
								return Result::FinishedAndDelete;
							}

							if (astGraph.IsInvalid())
							{
								LogError("Failed to deserialize asset '{}'", scriptCache.GetAssetGuid(m_identifier));
								scriptCache.OnInterpretedLoadFailed(m_identifier);
								return Result::FinishedAndDelete;
							}

							Script& scriptData = scriptCache.GetAssetData(m_identifier);
							if (scriptData.m_pAstGraph.IsValid())
							{
								*scriptData.m_pAstGraph = Move(astGraph);
							}
							else
							{
								scriptData.m_pAstGraph.CreateInPlace(Move(astGraph));
							}

							scriptCache.OnLoadedInterpreted(m_identifier);
						}
						break;
						case Mode::Compiled:
						{
							Compiler compiler;
							UniquePtr<FunctionObject> pFunction = compiler.Load(m_code.GetView());
							if (pFunction.IsInvalid())
							{
								scriptCache.OnCompiledLoadFailed(m_identifier);
								return Result::FinishedAndDelete;
							}

							Script& scriptData = scriptCache.GetAssetData(m_identifier);
							{
								Threading::UniqueLock lock(scriptData.m_functionMutex);
								auto it = scriptData.m_compiledFunctions.Find(m_functionGuid);
								if (it == scriptData.m_compiledFunctions.end())
								{
									it = scriptData.m_compiledFunctions.Emplace(Guid{m_functionGuid}, {});
								}
								it->second = Move(pFunction);
							}

							scriptCache.OnLoadedCompiled(m_identifier);
						}
						break;
					}

					return Result::FinishedAndDelete;
				}
				default:
					return Result::AwaitExternalFinish;
			}
		}
	protected:
		ScriptIdentifier m_identifier;
		Guid m_functionGuid;
		const Mode m_mode;
		Serialization::Data m_abstractSyntaxTreeData;
		Vector<ByteType> m_code;
		enum class State : uint8
		{
			AwaitingStart,
			AwaitingDataLoad,
		};
		State m_state{State::AwaitingStart};
	};

	ScriptCache::ScriptCache(Asset::Manager& assetManager)
		: m_pFunctionCache(UniquePtr<ScriptFunctionCache>::Make())
		, m_pTableCache(UniquePtr<ScriptTableCache>::Make())
		, m_pIntermediateEnvironment(Environment::Create(*m_pFunctionCache, *m_pTableCache))
	{
		RegisterAssetModifiedCallback(assetManager);
	}

	ScriptCache::~ScriptCache() = default;

	ScriptIdentifier ScriptCache::FindOrRegisterAsset(Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[](ScriptIdentifier, Asset::Guid)
			{
				return Script{};
			}
		);
	}

	ScriptIdentifier ScriptCache::RegisterAsset(Asset::Guid guid)
	{
		const ScriptIdentifier identifier = BaseType::RegisterAsset(
			guid,
			[](ScriptIdentifier, Asset::Guid)
			{
				return Script{};
			}
		);
		return identifier;
	}

	Optional<const AST::Graph*> ScriptCache::FindAstGraph(const ScriptIdentifier identifier) const
	{
		return GetAssetData(identifier).m_pAstGraph.Get();
	}

	Optional<const FunctionObject*> ScriptCache::FindFunction(ScriptIdentifier identifier, const Guid localFunctionIdentifier) const
	{
		const Script& scriptData = GetAssetData(identifier);
		Threading::SharedLock lock(scriptData.m_functionMutex);
		const auto it = scriptData.m_compiledFunctions.Find(localFunctionIdentifier);
		if (it != scriptData.m_compiledFunctions.end())
		{
			return it->second.Get();
		}
		return Invalid;
	}

	SharedPtr<Environment> ScriptCache::GetIntermediateEnvironment() const
	{
		return m_pIntermediateEnvironment;
	}

	Optional<Threading::Job*> ScriptCache::TryLoadScript(
		const ScriptIdentifier identifier, const Guid localFunctionIdentifier, ScriptLoadListenerData&& newListenerData
	)
	{
		if (newListenerData.m_callback.IsValid())
		{
			m_compiledScriptLoadEvents[identifier].Emplace(Forward<ScriptLoadListenerData>(newListenerData));
		}

		Script& scriptData = GetAssetData(identifier);

		{
			Threading::SharedLock lock(scriptData.m_functionMutex);
			const auto it = scriptData.m_compiledFunctions.Find(localFunctionIdentifier);
			if (it != scriptData.m_compiledFunctions.end())
			{
				m_compiledScriptLoadEvents[identifier].Execute(newListenerData.m_identifier, identifier);
				return nullptr;
			}
		}

		{
			Threading::UniqueLock lock(scriptData.m_functionMutex);
			const auto it = scriptData.m_compiledFunctions.Find(localFunctionIdentifier);
			if (it != scriptData.m_compiledFunctions.end())
			{
				m_compiledScriptLoadEvents[identifier].Execute(newListenerData.m_identifier, identifier);
				return nullptr;
			}
			else if (m_loadingCompiledScripts.Set(identifier))
			{
				return new LoadScriptJob(identifier, localFunctionIdentifier, LoadScriptJob::Mode::Compiled);
			}
		}

		return nullptr;
	}

	Optional<Threading::Job*> ScriptCache::TryLoadAstGraph(const ScriptIdentifier identifier, ScriptLoadListenerData&& newListenerData)
	{
		if (newListenerData.m_callback.IsValid())
		{
			m_interpretedScriptLoadEvents[identifier].Emplace(Forward<ScriptLoadListenerData>(newListenerData));
		}

		Threading::Job* pJob{nullptr};

		const Optional<const AST::Graph*> pAstGraph = FindAstGraph(identifier);
		if (pAstGraph.IsValid())
		{
			m_interpretedScriptLoadEvents[identifier].Execute(newListenerData.m_identifier, identifier);
		}
		else
		{
			if (m_loadingInterpretedScripts.Set(identifier))
			{
				pJob = new LoadScriptJob(identifier, {}, LoadScriptJob::Mode::Interpreted);
			}
		}

		return pJob;
	}

	void ScriptCache::RemoveListener(ScriptIdentifier identifier, const ScriptLoadListenerIdentifier listenerIdentifier)
	{
		m_interpretedScriptLoadEvents[identifier].Remove(listenerIdentifier);
		m_compiledScriptLoadEvents[identifier].Remove(listenerIdentifier);
	}

	bool ScriptCache::AssignAstGraph(ScriptIdentifier identifier, AST::Graph&& astGraph)
	{
		if (m_loadingInterpretedScripts.Set(identifier))
		{
			Script& scriptData = GetAssetData(identifier);
			if (scriptData.m_pAstGraph.IsValid())
			{
				*scriptData.m_pAstGraph = Forward<AST::Graph>(astGraph);
			}
			else
			{
				scriptData.m_pAstGraph.CreateInPlace(Forward<AST::Graph>(astGraph));
			}

			OnLoadedInterpreted(identifier);
			return true;
		}
		else
		{
			return false;
		}
	}

	bool ScriptCache::AssignCompiledFunction(
		ScriptIdentifier identifier, const Guid localFunctionIdentifier, UniquePtr<FunctionObject>&& pFunctionObject
	)
	{
		if (m_loadingCompiledScripts.Set(identifier))
		{
			Script& scriptData = GetAssetData(identifier);
			{
				Threading::UniqueLock lock(scriptData.m_functionMutex);
				auto it = scriptData.m_compiledFunctions.Find(localFunctionIdentifier);
				if (it != scriptData.m_compiledFunctions.end())
				{
					it->second = Forward<UniquePtr<FunctionObject>>(pFunctionObject);
				}
				else
				{
					scriptData.m_compiledFunctions.Emplace(Guid{localFunctionIdentifier}, Forward<UniquePtr<FunctionObject>>(pFunctionObject));
				}
			}

			OnLoadedCompiled(identifier);
			return true;
		}
		else
		{
			return false;
		}
	}

	void ScriptCache::RemoveCompiledFunction(ScriptIdentifier identifier, const Guid localFunctionIdentifier)
	{
		if (m_loadingCompiledScripts.Set(identifier))
		{
			Script& scriptData = GetAssetData(identifier);
			{
				Threading::UniqueLock lock(scriptData.m_functionMutex);
				auto it = scriptData.m_compiledFunctions.Find(localFunctionIdentifier);
				if (it != scriptData.m_compiledFunctions.end())
				{
					it->second.DestroyElement();
				}
			}

			OnLoadedCompiled(identifier);
		}
	}

	void ScriptCache::OnLoadedInterpreted(ScriptIdentifier identifier)
	{
		m_reloadingAssets.Clear(identifier);
		m_loadingInterpretedScripts.Clear(identifier);

		m_interpretedScriptLoadEvents[identifier](identifier);
	}

	void ScriptCache::OnInterpretedLoadFailed(ScriptIdentifier identifier)
	{
		m_reloadingAssets.Clear(identifier);
		m_loadingInterpretedScripts.Clear(identifier);

		m_interpretedScriptLoadEvents[identifier](identifier);
	}

	void ScriptCache::OnLoadedCompiled(ScriptIdentifier identifier)
	{
		m_reloadingAssets.Clear(identifier);
		m_loadingCompiledScripts.Clear(identifier);

		m_compiledScriptLoadEvents[identifier](identifier);
	}

	void ScriptCache::OnCompiledLoadFailed(ScriptIdentifier identifier)
	{
		m_reloadingAssets.Clear(identifier);
		m_loadingCompiledScripts.Clear(identifier);

		m_compiledScriptLoadEvents[identifier](identifier);
	}

	void ScriptCache::OnAssetModified(const Asset::Guid, const IdentifierType identifier, const IO::PathView)
	{
		Script& scriptData = GetAssetData(identifier);
		const bool shouldLoadAST = scriptData.m_pAstGraph.IsValid();
		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
		if (shouldLoadAST)
		{
			Threading::Job* pJob = new LoadScriptJob(identifier, {}, LoadScriptJob::Mode::Interpreted);
			thread.Queue(*pJob);
		}
		else
		{
			Threading::UniqueLock lock(scriptData.m_functionMutex);
			for (auto it = scriptData.m_compiledFunctions.begin(), endIt = scriptData.m_compiledFunctions.end(); it != endIt; ++it)
			{
				Threading::Job* pJob = new LoadScriptJob(identifier, it->first, LoadScriptJob::Mode::Compiled);
				thread.Queue(*pJob);
			}
		}
	}
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Scripting::ScriptAssetType>
	{
		static constexpr auto Type = Reflection::Reflect<Scripting::ScriptAssetType>(
			Scripting::ScriptAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Script"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
	[[maybe_unused]] const bool wasScriptAssetTypeRegistered = Registry::RegisterType<Scripting::ScriptAssetType>();
}
