#include "Entity/Data/Logic.h"
#include "Input/ActionMap.h"

#include <Engine/Entity/Component2D.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Scripting/ScriptCache.h>

#include <Engine/Scripting/Interpreter/Interpreter.h>
#include <Engine/Scripting/Interpreter/Resolver.h>

#include <Engine/Scripting/VirtualMachine/VirtualMachine.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/NativeFunction.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/DynamicEvent.h>

#include <Engine/Scripting/ScriptAssetType.h>
#include <Common/System/Query.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <Engine/Entity/ComponentSoftReference.inl>

namespace ngine::Entity::Data
{
	Logic::Logic(const Logic& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_scriptIdentifier(templateComponent.m_scriptIdentifier)
		, m_scriptProperties(templateComponent.m_scriptProperties)
	{
		if constexpr (SCRIPTING_COMPONENT_USE_INTERPRETER)
		{
			new (&m_interpreterData) InterpreterData();

			Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
			SharedPtr<Scripting::Environment> pGlobalEnvironment = scriptCache.GetIntermediateEnvironment();

			m_interpreterData.m_pEnvironment = SharedPtr<Scripting::Environment>::Make(Move(pGlobalEnvironment));
			m_interpreterData.m_pInterpreter = UniquePtr<Scripting::Interpreter>::Make(m_interpreterData.m_pEnvironment);
		}
		else
		{
			new (&m_virtualMachineData) VirtualMachineData();

			m_virtualMachineData.m_pVirtualMachine = UniquePtr<Scripting::VirtualMachine>::Make();
			m_virtualMachineData.m_pVirtualMachine->SetEntitySceneRegistry(cloner.GetSceneRegistry());
		}

		Threading::JobBatch jobBatch = LoadScriptInternal(cloner.GetParent());
		if (jobBatch.IsValid())
		{
			if (cloner.m_pJobBatch.IsValid())
			{
				cloner.m_pJobBatch->QueueAfterStartStage(jobBatch);
			}
			else
			{
				if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
				{
					pThread->Queue(jobBatch);
				}
				else
				{
					System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadLogic);
				}
			}
		}
	}

	Logic::Logic(Initializer&& initializer)
		: m_owner(initializer.GetParent())
		, m_scriptIdentifier(initializer.m_scriptIdentifier)
	{
		if constexpr (SCRIPTING_COMPONENT_USE_INTERPRETER)
		{
			new (&m_interpreterData) InterpreterData();

			Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
			SharedPtr<Scripting::Environment> pGlobalEnvironment = scriptCache.GetIntermediateEnvironment();

			m_interpreterData.m_pEnvironment = SharedPtr<Scripting::Environment>::Make(Move(pGlobalEnvironment));
			m_interpreterData.m_pInterpreter = UniquePtr<Scripting::Interpreter>::Make(m_interpreterData.m_pEnvironment);
		}
		else
		{
			new (&m_virtualMachineData) VirtualMachineData();

			m_virtualMachineData.m_pVirtualMachine = UniquePtr<Scripting::VirtualMachine>::Make();
			m_virtualMachineData.m_pVirtualMachine->SetEntitySceneRegistry(initializer.GetSceneRegistry());
		}

		Threading::JobBatch jobBatch = LoadScriptInternal(initializer.GetParent());
		if (jobBatch.IsValid())
		{
			if (initializer.m_pJobBatch.IsValid())
			{
				initializer.m_pJobBatch->QueueAfterStartStage(jobBatch);
			}
			else
			{
				if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
				{
					pThread->Queue(jobBatch);
				}
				else
				{
					System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadLogic);
				}
			}
		}
	}

	Logic::Logic(const Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
	{
		if constexpr (SCRIPTING_COMPONENT_USE_INTERPRETER)
		{
			new (&m_interpreterData) InterpreterData();

			Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
			SharedPtr<Scripting::Environment> pGlobalEnvironment = scriptCache.GetIntermediateEnvironment();

			m_interpreterData.m_pEnvironment = SharedPtr<Scripting::Environment>::Make(Move(pGlobalEnvironment));
			m_interpreterData.m_pInterpreter = UniquePtr<Scripting::Interpreter>::Make(m_interpreterData.m_pEnvironment);
		}
		else
		{
			new (&m_virtualMachineData) VirtualMachineData();

			m_virtualMachineData.m_pVirtualMachine = UniquePtr<Scripting::VirtualMachine>::Make();
			m_virtualMachineData.m_pVirtualMachine->SetEntitySceneRegistry(deserializer.GetSceneRegistry());
		}
	}

	Logic::~Logic()
	{
		Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
		scriptCache.RemoveListener(m_scriptIdentifier, this);

		if constexpr (SCRIPTING_COMPONENT_USE_INTERPRETER)
		{
			m_interpreterData.~InterpreterData();
		}
		else
		{
			m_virtualMachineData.~VirtualMachineData();
		}
	}

	void Logic::OnCreated(Entity::HierarchyComponentBase& owner)
	{
		if (m_scriptIdentifier.IsValid())
		{
			if (const Optional<Threading::Job*> pJob = ExecuteScriptInternal(owner))
			{
				if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
				{
					pJob->Queue(*pThread);
				}
				else
				{
					pJob->Queue(System::Get<Threading::JobManager>());
				}
			}
		}
	}

	void Logic::OnDestroying(Entity::HierarchyComponentBase& owner)
	{
		if (owner.IsSimulationActive())
		{
			DeregisterUpdate(owner);
		}

		if constexpr (!SCRIPTING_COMPONENT_USE_INTERPRETER)
		{
			Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
			Threading::UniqueLock lock(m_virtualMachineData.m_eventFunctionObjectsMutex);
			for (auto it = m_virtualMachineData.m_eventFunctionObjects.begin(), endIt = m_virtualMachineData.m_eventFunctionObjects.end();
			     it != endIt;
			     ++it)
			{
				const Optional<const Reflection::FunctionData*> pEventFunctionData = reflectionRegistry.FindEvent(it->first);
				Assert(pEventFunctionData.IsValid());
				if (LIKELY(pEventFunctionData.IsValid()))
				{
					using GetEventNativeFunction =
						Scripting::VM::NativeFunction<Optional<Scripting::VM::DynamicEvent*>(Entity::HierarchyComponentBase&)>;
					const GetEventNativeFunction getEventNativeFunction(pEventFunctionData->m_function);
					if (const Optional<Scripting::VM::DynamicEvent*> pEvent = getEventNativeFunction(owner))
					{
						pEvent->Remove(owner);
					}
				}
			}
		}
	}

	void Logic::SetLogicAsset(const Asset::Picker asset)
	{
		if (asset.GetAssetGuid().IsValid())
		{
			Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
			SetScript(m_owner, scriptCache.FindOrRegisterAsset(asset.GetAssetGuid()));
		}
	}

	void Logic::SetScript(Entity::HierarchyComponentBase& owner, const Scripting::ScriptIdentifier scriptIdentifier)
	{
		Assert(scriptIdentifier != m_scriptIdentifier);
		m_scriptIdentifier = scriptIdentifier;
		Threading::JobBatch jobBatch = LoadScriptInternal(owner);
		if (jobBatch.IsValid())
		{
			if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
			{
				pThread->Queue(jobBatch);
			}
			else
			{
				System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadLogic);
			}
		}
	}

	void Logic::OnScriptChanged(Entity::HierarchyComponentBase& owner)
	{
		Threading::JobBatch jobBatch = LoadScriptInternal(owner);
		if (jobBatch.IsValid())
		{
			if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
			{
				pThread->Queue(jobBatch);
			}
			else
			{
				System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadLogic);
			}
		}
	}

	Threading::JobBatch Logic::LoadScriptInternal(Entity::HierarchyComponentBase& owner)
	{
		Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
		if constexpr (SCRIPTING_COMPONENT_USE_INTERPRETER)
		{
			Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
			return scriptCache.TryLoadAstGraph(
				m_scriptIdentifier,
				Scripting::ScriptCache::ScriptLoadListenerData{
					*this,
					[ownerSoftReference = Entity::ComponentSoftReference{owner, sceneRegistry},
			     &sceneRegistry](Logic& logic, const Scripting::ScriptIdentifier scriptIdentifier)
					{
						const Optional<Entity::HierarchyComponentBase*> pOwner = ownerSoftReference.Find<Entity::HierarchyComponentBase>(sceneRegistry);
						if (pOwner.IsInvalid())
						{
							return EventCallbackResult::Remove;
						}

						Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
						const Optional<const Scripting::AST::Graph*> pAstGraph = scriptCache.FindAstGraph(scriptIdentifier);
						Assert(pAstGraph.IsValid());
						if (UNLIKELY(!pAstGraph.IsValid()))
						{
							return EventCallbackResult::Remove;
						}

						logic.m_interpreterData.m_pBeginFunctionExpression = pAstGraph->FindFunction(BeginEntryPointTypeGuid);
						// Detect function with no function body, ensure we skip any possible execution
						if (logic.m_interpreterData.m_pBeginFunctionExpression.IsValid())
						{
							if (logic.m_interpreterData.m_pBeginFunctionExpression->GetStatements().IsEmpty())
							{
								logic.m_interpreterData.m_pBeginFunctionExpression = {};
							}
						}

						const bool hadTick = logic.m_interpreterData.m_pTickFunctionExpression.IsValid();
						logic.m_interpreterData.m_pTickFunctionExpression = pAstGraph->FindFunction(TickEntryPointTypeGuid);
						// Detect function with no function body, ensure we skip any possible execution
						if (logic.m_interpreterData.m_pTickFunctionExpression.IsValid())
						{
							if (logic.m_interpreterData.m_pTickFunctionExpression->GetStatements().IsEmpty())
							{
								logic.m_interpreterData.m_pTickFunctionExpression = {};
							}
						}

						if (hadTick != logic.m_interpreterData.m_pTickFunctionExpression.IsValid() && pOwner->GetFlags(sceneRegistry).IsNotSet(Entity::ComponentFlags::IsSimulationPaused))
						{
							if (hadTick)
							{
								logic.DeregisterUpdate(*pOwner);
							}
							else
							{
								logic.RegisterForUpdate(*pOwner);
							}
						}

						return EventCallbackResult::Remove;
					}
				}
			);
		}
		else
		{
			Threading::JobBatch jobBatch{Threading::JobBatch::IntermediateStage};

			Threading::IntermediateStage& finishedStage = Threading::CreateIntermediateStage();
			finishedStage.AddSubsequentStage(jobBatch.GetFinishedStage());

			Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
			jobBatch
				.QueueAfterStartStage(scriptCache
			                          .TryLoadAstGraph(
																	m_scriptIdentifier,
																	Scripting::ScriptCache::ScriptLoadListenerData{
																		*this,
																		[ownerSoftReference = Entity::ComponentSoftReference{owner, sceneRegistry},
			                               &sceneRegistry,
			                               &finishedStage](Logic& logic, const Scripting::ScriptIdentifier scriptIdentifier)
																		{
																			const Optional<Entity::HierarchyComponentBase*> pOwner =
																				ownerSoftReference.Find<Entity::HierarchyComponentBase>(sceneRegistry);
																			if (pOwner.IsInvalid())
																			{
																				return EventCallbackResult::Remove;
																			}

																			Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();

																			Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
																			const Optional<const Scripting::AST::Graph*> pAstGraph = scriptCache.FindAstGraph(scriptIdentifier);
																			Assert(pAstGraph.IsValid());
																			if (UNLIKELY(!pAstGraph.IsValid()))
																			{
																				finishedStage.SignalExecutionFinishedAndDestroying(thread);
																				return EventCallbackResult::Remove;
																			}

																			Threading::JobBatch jobBatch;

																			if (pAstGraph->FindFunction(BeginEntryPointTypeGuid).IsValid())
																			{
																				jobBatch.QueueAfterStartStage(scriptCache.TryLoadScript(
																					logic.m_scriptIdentifier,
																					BeginEntryPointTypeGuid,
																					Scripting::ScriptCache::ScriptLoadListenerData{
																						logic,
																						[](Logic& logic, const Scripting::ScriptIdentifier scriptIdentifier)
																						{
																							Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
																							logic.m_virtualMachineData.m_pBeginFunctionObject =
																								scriptCache.FindFunction(scriptIdentifier, BeginEntryPointTypeGuid);

																							return EventCallbackResult::Remove;
																						}
																					}
																				));
																			}
																			if (pAstGraph->FindFunction(TickEntryPointTypeGuid).IsValid())
																			{
																				jobBatch.QueueAfterStartStage(scriptCache.TryLoadScript(
																					logic.m_scriptIdentifier,
																					TickEntryPointTypeGuid,
																					Scripting::ScriptCache::ScriptLoadListenerData{
																						logic,
																						[ownerSoftReference,
					                                   &sceneRegistry](Logic& logic, const Scripting::ScriptIdentifier scriptIdentifier)
																						{
																							const Optional<Entity::HierarchyComponentBase*> pOwner =
																								ownerSoftReference.Find<Entity::HierarchyComponentBase>(sceneRegistry);
																							if (pOwner.IsInvalid())
																							{
																								return EventCallbackResult::Remove;
																							}

																							Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();

																							const bool hadTick = logic.m_virtualMachineData.m_pTickFunctionObject.IsValid();
																							logic.m_virtualMachineData.m_pTickFunctionObject =
																								scriptCache.FindFunction(scriptIdentifier, TickEntryPointTypeGuid);
																							if (hadTick != logic.m_virtualMachineData.m_pTickFunctionObject.IsValid() && pOwner->GetFlags(sceneRegistry).IsNotSet(Entity::ComponentFlags::IsSimulationPaused))
																							{
																								if (hadTick)
																								{
																									logic.DeregisterUpdate(*pOwner);
																								}
																								else
																								{
																									logic.RegisterForUpdate(*pOwner);
																								}
																							}

																							return EventCallbackResult::Remove;
																						}
																					}
																				));
																			}

																			Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
																			pAstGraph
																				->IterateFunctions(
																					[&logic,
				                                   &owner = *pOwner,
				                                   &sceneRegistry,
				                                   &reflectionRegistry,
				                                   &jobBatch](const Scripting::AST::Expression::Base& variableBase, const Scripting::AST::Expression::Function&)
																					{
																						Guid functionGuid;
																						Optional<Entity::HierarchyComponentBase*> pTargetComponent{owner};
																						switch (variableBase.GetType())
																						{
																							case Scripting::AST::NodeType::VariableDeclaration:
																							{
																								const Scripting::AST::Expression::VariableDeclaration& variableDeclaration =
																									static_cast<const Scripting::AST::Expression::VariableDeclaration&>(variableBase);
																								functionGuid = variableDeclaration.GetIdentifier().identifier;
																							}
																							break;
																							case Scripting::AST::NodeType::Variable:
																							{
																								const Scripting::AST::Expression::Variable& variable =
																									static_cast<const Scripting::AST::Expression::Variable&>(variableBase);
																								functionGuid = variable.GetIdentifier().identifier;

																								if (variable.GetObject().IsValid())
																								{
																									Assert(variable.GetObject()->GetType() == Scripting::AST::NodeType::Literal);
																									if (variable.GetObject()->GetType() == Scripting::AST::NodeType::Literal)
																									{
																										const Scripting::AST::Expression::Literal& literalExpression =
																											static_cast<const Scripting::AST::Expression::Literal&>(*variable.GetObject());
																										if (const Optional<const Entity::ComponentSoftReference*> pComponentSoftReference = literalExpression.GetValue().Get<Entity::ComponentSoftReference>())
																										{
																											if (const Optional<Entity::HierarchyComponentBase*> pComponent = pComponentSoftReference->Find<Entity::HierarchyComponentBase>(sceneRegistry))
																											{
																												pTargetComponent = pComponent;
																											}
																										}
																									}
																								}
																							}
																							break;
																							default:
																								ExpectUnreachable();
																						}

																						if (const Optional<const Reflection::EventInfo*> pEventInfo = reflectionRegistry.FindEventDefinition(functionGuid))
																						{
																							Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
																							jobBatch.QueueAfterStartStage(scriptCache.TryLoadScript(
																								logic.m_scriptIdentifier,
																								pEventInfo->m_guid,
																								Scripting::ScriptCache::ScriptLoadListenerData{
																									logic,
																									[targetSoftReference = Entity::ComponentSoftReference{*pTargetComponent, sceneRegistry},
						                                       &sceneRegistry,
						                                       eventGuid =
						                                         pEventInfo->m_guid](Logic& logic, const Scripting::ScriptIdentifier scriptIdentifier)
																									{
																										const Optional<Entity::HierarchyComponentBase*> pTarget =
																											targetSoftReference.Find<Entity::HierarchyComponentBase>(sceneRegistry);
																										if (pTarget.IsInvalid())
																										{
																											return EventCallbackResult::Remove;
																										}

																										Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
																										const Optional<const Reflection::FunctionData*> pEventFunctionData =
																											reflectionRegistry.FindEvent(eventGuid);
																										Optional<Scripting::VM::DynamicEvent*> pEvent;
																										Assert(pEventFunctionData.IsValid());
																										if (LIKELY(pEventFunctionData.IsValid()))
																										{
																											using GetEventNativeFunction = Scripting::VM::NativeFunction<
																												Optional<Scripting::VM::DynamicEvent*>(Entity::HierarchyComponentBase&)>;
																											const GetEventNativeFunction getEventNativeFunction(pEventFunctionData->m_function);
																											pEvent = getEventNativeFunction(*pTarget);
																										}

																										if (scriptIdentifier.IsValid())
																										{
																											Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
																											if (const Optional<const Scripting::FunctionObject*> pFunctionObject =
								                                            scriptCache.FindFunction(scriptIdentifier, eventGuid);
								                                          pFunctionObject.IsValid() && pEvent.IsValid())
																											{
																												// Subscribe to the event
																												pEvent->Emplace(pFunctionObject->CreateDelegate(Memory::GetAddressOf(*pTarget)));

																												Threading::UniqueLock lock(logic.m_virtualMachineData.m_eventFunctionObjectsMutex);
																												logic.m_virtualMachineData.m_eventFunctionObjects.EmplaceOrAssign(
																													Guid{eventGuid},
																													ReferenceWrapper<const Scripting::FunctionObject>{*pFunctionObject}
																												);

																												return EventCallbackResult::Remove;
																											}
																										}

																										Threading::UniqueLock lock(logic.m_virtualMachineData.m_eventFunctionObjectsMutex);
																										auto it = logic.m_virtualMachineData.m_eventFunctionObjects.Find(eventGuid);
																										if (it != logic.m_virtualMachineData.m_eventFunctionObjects.end())
																										{
																											logic.m_virtualMachineData.m_eventFunctionObjects.Remove(it);

																											if (pEvent.IsValid())
																											{
																												pEvent->Remove(Memory::GetAddressOf(*pTarget));
																											}
																										}

																										return EventCallbackResult::Remove;
																									}
																								}
																							));
																						}
																					}
																				);

																			jobBatch.QueueAsNewFinishedStage(finishedStage);
																			thread.Queue(jobBatch);

																			return EventCallbackResult::Remove;
																		}
																	}
																));

			return jobBatch;
		}
	}

	Optional<Threading::Job*> Logic::ExecuteScriptInternal(Entity::HierarchyComponentBase& owner)
	{
		Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if constexpr (SCRIPTING_COMPONENT_USE_INTERPRETER)
		{
			return scriptCache.TryLoadAstGraph(
				m_scriptIdentifier,
				Scripting::ScriptCache::ScriptLoadListenerData{
					*this,
					[ownerSoftReference = Entity::ComponentSoftReference{owner, sceneRegistry},
			     &sceneRegistry](Logic& logic, const Scripting::ScriptIdentifier)
					{
						const Optional<Entity::HierarchyComponentBase*> pOwner = ownerSoftReference.Find<Entity::HierarchyComponentBase>(sceneRegistry);
						if (pOwner.IsInvalid())
						{
							return EventCallbackResult::Remove;
						}

						logic.ExecuteBegin(*pOwner);
						return EventCallbackResult::Remove;
					}
				}
			);
		}
		else
		{
			return scriptCache.TryLoadScript(
				m_scriptIdentifier,
				BeginEntryPointTypeGuid,
				Scripting::ScriptCache::ScriptLoadListenerData{
					*this,
					[ownerSoftReference = Entity::ComponentSoftReference{owner, sceneRegistry},
			     &sceneRegistry](Logic& logic, const Scripting::ScriptIdentifier)
					{
						const Optional<Entity::HierarchyComponentBase*> pOwner = ownerSoftReference.Find<Entity::HierarchyComponentBase>(sceneRegistry);
						if (pOwner.IsInvalid())
						{
							return EventCallbackResult::Remove;
						}

						logic.ExecuteBegin(*pOwner);
						return EventCallbackResult::Remove;
					}
				}
			);
		}
	}

	Asset::Picker Logic::GetLogicAsset() const
	{
		Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
		const Asset::Guid assetGuid = m_scriptIdentifier.IsValid() ? scriptCache.GetAssetGuid(m_scriptIdentifier) : Asset::Guid();
		return {assetGuid, Scripting::ScriptAssetType::AssetFormat.assetTypeGuid};
	}

	Threading::JobBatch Logic::SetDeserializedLogicAsset(
		const Asset::Picker asset,
		[[maybe_unused]] const Serialization::Reader objectReader,
		[[maybe_unused]] const Serialization::Reader typeReader
	)
	{
		if (asset.IsValid())
		{
			Scripting::ScriptCache& scriptCache = System::Get<Scripting::ScriptCache>();
			m_scriptIdentifier = scriptCache.FindOrRegisterAsset(asset.GetAssetGuid());
			return LoadScriptInternal(m_owner);
		}
		return {};
	}

	void Logic::RegisterForUpdate(Entity::HierarchyComponentBase& owner)
	{
		if constexpr (SCRIPTING_COMPONENT_USE_INTERPRETER)
		{
			if (m_interpreterData.m_pTickFunctionExpression.IsValid())
			{
				Entity::ComponentTypeSceneData<Logic>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<Logic>();
				sceneData.EnableUpdate(*this);
			}
		}
		else
		{
			if (m_virtualMachineData.m_pTickFunctionObject.IsValid())
			{
				Entity::ComponentTypeSceneData<Logic>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<Logic>();
				sceneData.EnableUpdate(*this);
			}
		}
	}

	void Logic::DeregisterUpdate(Entity::HierarchyComponentBase& owner)
	{
		Entity::ComponentTypeSceneData<Logic>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<Logic>();
		sceneData.DisableUpdate(*this);
	}

	void Logic::OnEnable(Entity::HierarchyComponentBase& owner)
	{
		if (owner.IsSimulationActive())
		{
			RegisterForUpdate(owner);
		}
	}

	void Logic::OnDisable(Entity::HierarchyComponentBase& owner)
	{
		DeregisterUpdate(owner);
	}

	void Logic::OnSimulationResumed(Entity::HierarchyComponentBase& owner)
	{
		if (owner.IsEnabled())
		{
			RegisterForUpdate(owner);
		}
	}

	void Logic::OnSimulationPaused(Entity::HierarchyComponentBase& owner)
	{
		DeregisterUpdate(owner);
	}

	void Logic::Update()
	{
		Entity::HierarchyComponentBase& owner = m_owner;

		FrameTime deltaTime;
		if (owner.Is3D())
		{
			deltaTime = static_cast<Entity::Component3D&>(owner).GetCurrentFrameTime();
		}
		else
		{
			Assert(owner.Is2D());
			deltaTime = static_cast<Entity::Component2D&>(owner).GetCurrentFrameTime();
		}

		if constexpr (SCRIPTING_COMPONENT_USE_INTERPRETER)
		{
			Assert(m_interpreterData.m_pTickFunctionExpression.IsValid());
			Scripting::Resolver resolver;
			SharedPtr<Scripting::ResolvedVariableMap> pVariables =
				resolver.Resolve(*m_interpreterData.m_pTickFunctionExpression, *m_interpreterData.m_pEnvironment);

			Scripting::ScriptValues arguments;
			if (owner.Is3D())
			{
				arguments.EmplaceBack(ConstAnyView{static_cast<Entity::Component3D&>(owner)});
			}
			else
			{
				Assert(owner.Is2D());
				arguments.EmplaceBack(ConstAnyView{static_cast<Entity::Component2D&>(owner)});
			}
			arguments.EmplaceBack(Scripting::ScriptValue((float)deltaTime));
			m_interpreterData.m_pInterpreter->Interpret(*m_interpreterData.m_pTickFunctionExpression, Move(arguments), pVariables);
		}
		else
		{
			Assert(m_virtualMachineData.m_pTickFunctionObject.IsValid());
			if (UNLIKELY(!m_virtualMachineData.m_pTickFunctionObject.IsValid()))
			{
				return;
			}

			m_virtualMachineData.m_pVirtualMachine->Initialize(*m_virtualMachineData.m_pTickFunctionObject);

			Array<Scripting::RawValue, 2> arguments;
			if (owner.Is3D())
			{
				arguments[0] = Scripting::RawValue{Scripting::VM::DynamicInvoke::LoadArgument(&static_cast<Entity::Component3D&>(owner))};
			}
			else
			{
				Assert(owner.Is2D());
				arguments[0] = Scripting::RawValue{Scripting::VM::DynamicInvoke::LoadArgument(&static_cast<Entity::Component2D&>(owner))};
			}
			arguments[1] = Scripting::RawValue{(float)deltaTime};

			Array<Scripting::RawValue, 1> results;
			m_virtualMachineData.m_pVirtualMachine->Execute(arguments, results);
		}
	}

	void Logic::ExecuteBegin(Entity::HierarchyComponentBase& owner)
	{
		if constexpr (SCRIPTING_COMPONENT_USE_INTERPRETER)
		{
			if (!m_interpreterData.m_pBeginFunctionExpression.IsValid())
			{
				return;
			}

			Scripting::Resolver resolver;
			SharedPtr<Scripting::ResolvedVariableMap> pVariables =
				resolver.Resolve(*m_interpreterData.m_pBeginFunctionExpression, *m_interpreterData.m_pEnvironment);

			Scripting::ScriptValues arguments;
			if (owner.Is3D())
			{
				arguments.EmplaceBack(ConstAnyView{static_cast<Entity::Component3D&>(owner)});
			}
			else
			{
				Assert(owner.Is2D());
				arguments.EmplaceBack(ConstAnyView{static_cast<Entity::Component2D&>(owner)});
			}
			m_interpreterData.m_pInterpreter->Interpret(*m_interpreterData.m_pBeginFunctionExpression, Move(arguments), pVariables);

			Reflection::PropertyBinder newScriptProperties;
			/*if (pPropertyTable.IsValid())
			{
			  const Scripting::ScriptTable::MapType& properties = pPropertyTable->Get();
			  for (auto propertyIt : properties)
			  {
			    const Guid key = propertyIt.first;
			              ConstAnyView property = m_scriptProperties.GetProperty(key);
			    if (property.IsValid())
			    {
			      Scripting::ScriptValue scriptValue = m_interpreterData.m_pInterpreter->Convert(property);
			      pPropertyTable->Set(key, Move(scriptValue));

			      // only update editor ui structure for non shipping builds
			      if constexpr (!SHIPPING_BUILD)
			      {
			        Any value = m_interpreterData.m_pInterpreter->Convert(propertyIt.second);
			        newScriptProperties.SetProperty(key, Any(property));
			      }
			    }
			    else if constexpr (!SHIPPING_BUILD) // only update editor ui structure for non shipping builds
			    {
			      Any value = m_interpreterData.m_pInterpreter->Convert(propertyIt.second);
			      newScriptProperties.SetProperty(key, Move(value));
			    }
			  }
			}*/
			m_scriptProperties = Move(newScriptProperties);
		}
		else
		{
			if (!m_virtualMachineData.m_pBeginFunctionObject.IsValid())
			{
				return;
			}

			m_virtualMachineData.m_pVirtualMachine->Initialize(*m_virtualMachineData.m_pBeginFunctionObject);

			Array<Scripting::RawValue, 1> arguments;
			if (owner.Is3D())
			{
				arguments[0] = Scripting::RawValue{Scripting::VM::DynamicInvoke::LoadArgument(&static_cast<Entity::Component3D&>(owner))};
			}
			else
			{
				Assert(owner.Is2D());
				arguments[0] = Scripting::RawValue{Scripting::VM::DynamicInvoke::LoadArgument(&static_cast<Entity::Component2D&>(owner))};
			}

			Array<Scripting::RawValue, 1> results;
			m_virtualMachineData.m_pVirtualMachine->Execute(arguments, results);

			Reflection::PropertyBinder newScriptProperties;
			/*if (pPropertyTable)
			{
			  for (auto propertyIt : pPropertyTable->values)
			  {
			    ConstAnyView property = m_scriptProperties.GetProperty(propertyIt.first);
			    if (property.IsValid())
			    {
			      Scripting::Value scriptValue = Scripting::Convert(property, gc);
			      pPropertyTable->values.EmplaceOrAssign(Guid(propertyIt.first), Move(scriptValue));

			      if constexpr (!SHIPPING_BUILD) // only update editor ui structure for non shipping builds
			      {
			        Any value = Scripting::Convert(propertyIt.second);
			        newScriptProperties.SetProperty(propertyIt.first, Any(property));
			      }
			    }
			    else if constexpr (!SHIPPING_BUILD) // only update editor ui structure for non shipping builds
			    {
			      Any value = Scripting::Convert(propertyIt.second);
			      newScriptProperties.SetProperty(propertyIt.first, Move(value));
			    }
			  }
			}*/
			m_scriptProperties = Move(newScriptProperties);
		}
	}

	void Logic::ScriptPropertiesChanged()
	{
		/*if constexpr (!SHIPPING_BUILD) // Only update from editor ui for non shipping builds
		{
		  if constexpr (SCRIPTING_COMPONENT_USE_INTERPRETER)
		  {
		    if (m_interpreterData.m_pEnvironment)
		    {
		      const Reflection::PropertyBinder::PropertyMap& properties = m_scriptProperties.GetProperties();
		      if (properties.HasElements())
		      {
		        if (Optional<Scripting::ScriptTable*> pPropertyTable = m_interpreterData.m_pEnvironment->GetTable("property",
		Scripting::Token::GuidFromScriptString("property")))
		        {
		          for (auto propertyIt : properties)
		          {
		            Scripting::ScriptValue value = m_interpreterData.m_pInterpreter->Convert(propertyIt.second);
		            pPropertyTable->Set(Scripting::ScriptValue{Scripting::StringType{propertyIt.first}}, Move(value));
		          }
		        }
		      }
		    }
		  }
		  else
		  {
		    if (m_virtualMachineData.m_pVirtualMachine)
		    {
		      const Reflection::PropertyBinder::PropertyMap& properties = m_scriptProperties.GetProperties();
		      if (properties.HasElements())
		      {
		        Scripting::GC& gc = m_virtualMachineData.m_pVirtualMachine->GetGC();
		        if (Scripting::TableObject* pPropertyTable = GetGlobalTable(*m_virtualMachineData.m_pVirtualMachine,
		Scripting::Token::GuidFromScriptString("property")))
		        {
		          for (auto propertyIt : properties)
		          {
		            Scripting::Value value = Scripting::Convert(propertyIt.second, gc);
		            pPropertyTable->values.EmplaceOrAssign(Guid(propertyIt.first), Move(value));
		          }
		        }
		      }
		    }
		  }
		}*/
	}

	[[maybe_unused]] const bool wasLogicComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Logic>>::Make());
	[[maybe_unused]] const bool wasLogicComponentTypeRegistered = Reflection::Registry::RegisterType<Logic>();
}
