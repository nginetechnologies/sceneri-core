#include "Entity/Splines/DistributorComponent.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include "Engine/Entity/ComponentType.h"
#include "Engine/Entity/ComponentSoftReference.inl"
#include <Engine/Entity/Serialization/ComponentValue.h>
#include <Engine/Entity/Splines/SplineComponent.h>
#include <Engine/Entity/StaticMeshComponent.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Threading/JobManager.h>
#include <Engine/Asset/AssetManager.h>

#include <Renderer/Assets/StaticMesh/StaticMesh.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>
#include <Common/Serialization/Guid.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Math/Mod.h>
#include <Common/Math/Primitives/Spline.h>
#include <Common/Math/Primitives/Intersect/LineSphere.h>
#include <Common/Reflection/Registry.inl>
#include <Common/IO/Log.h>

namespace ngine::Entity
{
	DistributorComponent::DistributorComponent(const DistributorComponent& templateComponent, const Cloner& cloner)
		: Component3D(templateComponent, cloner)
		, m_asset(templateComponent.m_asset)
	{
		Repopulate(
			[]
			{
			}
		);
	}

	DistributorComponent::DistributorComponent(const Deserializer& deserializer)
		: DistributorComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<DistributorComponent>().ToString().GetView())
			)
	{
	}

	DistributorComponent::DistributorComponent(
		const Deserializer& deserializer, [[maybe_unused]] Optional<Serialization::Reader> componentSerializer
	)
		: Component3D(deserializer)
	{
	}

	DistributorComponent::DistributorComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer))
		, m_asset(initializer.m_paintedAssetGuid)
	{
	}

	DistributorComponent::~DistributorComponent()
	{
		Threading::UniqueLock lock(m_mutex);
	}

	void DistributorComponent::OnCreated()
	{
		if (const Optional<SplineComponent*> pSpline = GetSpline())
		{
			pSpline->OnChanged.Add(
				*this,
				[](DistributorComponent& distributor)
				{
					distributor.Repopulate(
						[]
						{
						}
					);
				}
			);
		}
	}

	// TODO: Equivalent to transient properties like UE
	// Allow computing something in Editor, store in a property and save with the level (+ serialize elsewhere)
	// Useful to avoid recomputing stuff, in our case detect the distributor's spline at edit time, and only update it when the parent
	// changes. This way we don't have to requery where the spline is Property<SplineComponent*, Reflection::PropertyFlags::Transient> Spline;
	// Same can be used to cache the rope mesh, avoiding recomputation
	Optional<SplineComponent*> DistributorComponent::GetSpline() const
	{
		return FindFirstParentOfType<SplineComponent>();
	}

	void DistributorComponent::Repopulate(RepopulatedCallback&& callback)
	{
		const Optional<SplineComponent*> pSpline = GetSpline();
		if (pSpline.IsInvalid())
		{
			return;
		}

		if (!m_flags.TrySetFlags(Flags::IsRepopulationQueued))
		{
			m_flags |= Flags::ShouldRequeueRepopulation;
			return;
		}

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentSoftReference softReference{*this, sceneRegistry};

		System::
			Get<Threading::JobManager>()
				.QueueCallback(
					[softReference, &sceneRegistry, callback = Forward<RepopulatedCallback>(callback)](Threading::JobRunnerThread&) mutable
					{
						if (const Optional<DistributorComponent*> pDistributor = softReference.Find<DistributorComponent>(sceneRegistry))
						{
							Threading::UniqueLock lock(Threading::TryLock, pDistributor->m_mutex);
							while (!lock.IsLocked())
							{
								Threading::JobRunnerThread::GetCurrent()->DoRunNextJob();
								lock.TryLock();
							}
							Assert(!pDistributor->IsDestroying(sceneRegistry));

							using SplineType = SplineComponent::SplineType;

							auto distributeAssets =
								[&sceneRegistry, pDistributor, callback = Move(callback), lock = Move(lock)](Optional<Entity::Component3D*> pFirstComponent
				        ) mutable
							{
								if (LIKELY(pFirstComponent.IsValid()))
								{
									Math::BoundingBox boundingBox = pFirstComponent->GetRelativeBoundingBox();
									const float assetLength = boundingBox.GetSize().y + pDistributor->m_offset.GetMeters();
									const float offset = -boundingBox.GetMinimum().y;
									pDistributor->m_nextRadius = Math::Radiusf::FromMeters(boundingBox.GetSize().x * 0.5f);

									const Math::WorldTransform worldTransform = pDistributor->GetParent().GetWorldTransform();

									ChildIndex nextReusedChildIndex = 1;
									Optional<Entity::Component3D*> pNextComponent = pFirstComponent;

									const Optional<const Entity::SplineComponent*> pSplineComponent = pDistributor->GetSpline();
									pSplineComponent
										->VisitSpline(
											[&sceneRegistry,
						           assetLength,
						           offset,
						           pDistributor,
						           &pFirstComponent,
						           &pNextComponent,
						           &nextReusedChildIndex,
						           worldTransform](const Math::Splinef& spline)
											{
												spline
													.IterateAdjustedSplinePoints(
														[&sceneRegistry,
							               nextAssetStart = Math::Vector3f{Math::Zero},
							               assetLength,
							               offset,
							               pDistributor,
							               &templateComponent = *pFirstComponent,
							               &templateTypeInfo = *pFirstComponent->GetTypeInfo(),
							               &pNextComponent,
							               &nextReusedChildIndex,
							               &spline,
							               worldTransform](
															const SplineType::Spline::Point& point,
															const SplineType::Spline::Point& nextPoint,
															const Math::Vector3f currentBezierPoint,
															[[maybe_unused]] const Math::Vector3f nextBezierPoint,
															[[maybe_unused]] const Math::Vector3f direction,
															[[maybe_unused]] const Math::Vector3f normal
														) mutable
														{
															auto checkIntersection = [nextAssetStart, assetLength, lineStart = currentBezierPoint, nextBezierPoint](
																											 ) mutable -> Optional<Math::Vector3f>
															{
																const Math::Spheref assetSphere(nextAssetStart, Math::Radiusf::FromMeters(assetLength));
																const Math::Linef line{lineStart, nextBezierPoint};
																const Math::LineSphereIntersectionResult<Math::Vector3f> intersectionResult =
																	Math::Intersects<Math::Vector3f>(assetSphere, line);
																if (intersectionResult.m_type == Math::IntersectionType::Intersection && intersectionResult.m_exitIntersection.IsValid())
																{
																	lineStart = nextAssetStart = *intersectionResult.m_exitIntersection;
																	return *intersectionResult.m_exitIntersection;
																}

																return Invalid;
															};

															for (Optional<Math::Vector3f> intersectionPoint = checkIntersection(); intersectionPoint.IsValid();
								                   intersectionPoint = checkIntersection())
															{
																const Math::Linef line{currentBezierPoint, nextBezierPoint};
																const float usedRatio = line.GetClosestPointRatio(*intersectionPoint);
																const Math::Vector3f right = spline.GetBezierNormalBetweenPoints(point, nextPoint, usedRatio);
																const Math::Vector3f forward = spline.GetBezierDirectionBetweenPoints(point, nextPoint, usedRatio);
																const Math::Vector3f up = right.Cross(forward).GetNormalized();
																const Math::Vector3f forwardDirection = (*intersectionPoint - nextAssetStart).GetNormalized();
																Math::Matrix3x3f segmentMatrix(forwardDirection.Cross(up).GetNormalized(), forwardDirection, up);
																segmentMatrix.Orthonormalize();

																const Math::WorldRotation segmentRotation =
																	worldTransform.TransformRotation(Math::WorldQuaternion(segmentMatrix));
																const Math::Vector3f parentOffset = segmentRotation.TransformDirection(pDistributor->GetRelativeLocation());

																const Math::WorldTransform newWorldTransform = Math::WorldTransform(
																	segmentRotation,
																	worldTransform.TransformLocation(nextAssetStart + forwardDirection * offset + parentOffset)
																);

																if (LIKELY(pNextComponent != nullptr))
																{
																	pNextComponent->SetWorldTransform(newWorldTransform);
																}

																nextAssetStart = *intersectionPoint;

																if (nextReusedChildIndex < pDistributor->GetChildren().GetSize())
																{
																	pNextComponent = &pDistributor->GetChildren()[nextReusedChildIndex];
																	nextReusedChildIndex++;
																}
																else
																{
																	Threading::JobBatch jobBatch;
																	Optional<Entity::Component*> pClonedComponent = templateTypeInfo.CloneFromTemplateWithChildren(
																		Guid::Generate(),
																		templateComponent,
																		templateComponent.GetParent(),
																		*pDistributor,
																		System::Get<Entity::Manager>().GetRegistry(),
																		sceneRegistry,
																		templateComponent.GetSceneRegistry(),
																		jobBatch
																	);
																	if (jobBatch.IsValid())
																	{
																		Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
																	}

																	pNextComponent = static_cast<Entity::Component3D*>(pClonedComponent.operator ngine::Entity::Component*());
																	nextReusedChildIndex++;
																}
															}
														}
													);
												return false;
											}
										);

									for (; nextReusedChildIndex < pDistributor->GetChildren().GetSize();)
									{
										Entity::Component3D& child = pDistributor->GetChildren()[nextReusedChildIndex];
										child.Destroy(sceneRegistry);
									}

									callback();
									{
										[[maybe_unused]] const bool cleared =
											pDistributor->m_flags.FetchAnd(~Flags::IsRepopulationQueued).IsSet(Flags::IsRepopulationQueued);
										Assert(cleared);
									}

									if (pDistributor->m_flags.FetchAnd(~Flags::ShouldRequeueRepopulation).IsSet(Flags::ShouldRequeueRepopulation))
									{
										pDistributor->Repopulate(Move(callback));
									}
								}
							};

							if (pDistributor->GetChildren().HasElements())
							{
								Entity::Component3D& firstComponent = pDistributor->GetChildren()[0];
								distributeAssets(firstComponent);
							}
							else
							{
								const Asset::Manager& assetManager = System::Get<Asset::Manager>();
								Threading::Job* pComponentLoadJob = assetManager.RequestAsyncLoadAssetMetadata(
									pDistributor->m_asset,
									Threading::JobPriority::CreateProceduralMesh,
									[pDistributor, &sceneRegistry, distributeAssets = Move(distributeAssets)](const ConstByteView data) mutable
									{
										if (UNLIKELY(!data.HasElements()))
										{
											LogWarning("Asset data was empty when loading asset {0}!", pDistributor->m_asset.ToString());
											return;
										}

										Serialization::Data assetData(
											ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
										);
										if (UNLIKELY(!assetData.IsValid()))
										{
											LogWarning("Asset data was invalid when loading asset {0}!", pDistributor->m_asset.ToString());
											return;
										}

										Serialization::Reader reader(assetData);
										Threading::JobBatch loadComponentBatch;
										Entity::ComponentValue<Entity::Component3D> component;
										reader.SerializeInPlace(component, *pDistributor, sceneRegistry, loadComponentBatch);
										if (loadComponentBatch.IsValid())
										{
											loadComponentBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
												[distributeAssets = Move(distributeAssets), &component = *component](Threading::JobRunnerThread&) mutable
												{
													distributeAssets(component);
												},
												Threading::JobPriority::CreateProceduralMesh
											));

											Threading::JobRunnerThread::GetCurrent()->Queue(loadComponentBatch);
										}
										else
										{
											distributeAssets(component);
										}
									}
								);
								if (pComponentLoadJob != nullptr)
								{
									pComponentLoadJob->Queue(*Threading::JobRunnerThread::GetCurrent());
								}
							}
						}

						return Threading::CallbackResult::FinishedAndDelete;
					},
					Threading::JobPriority::CreateProceduralMesh
				);
	}

	void DistributorComponent::SetComponentType(const ComponentTypePicker asset)
	{
		m_asset = asset.GetAssetGuid();
		Repopulate(
			[]
			{
			}
		);
	}

	DistributorComponent::ComponentTypePicker DistributorComponent::GetComponentType() const
	{
		const Guid assetTypeGuid = System::Get<Asset::Manager>().GetAssetTypeGuid(m_asset);
		return ComponentTypePicker{
			Asset::Reference{m_asset, assetTypeGuid},
			Asset::Types{Array<const Asset::TypeGuid, 2>{
				Reflection::GetTypeGuid<Entity::SceneComponent>(),
				Reflection::GetTypeGuid<Entity::StaticMeshComponent>()
			}}
		};
	}

	bool DistributorComponent::
		CanApplyAtPoint(const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags>)
			const
	{
		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			constexpr Array<const Asset::TypeGuid, 2> supportedAssetTypes{
				Reflection::GetTypeGuid<Entity::SceneComponent>(),
				Reflection::GetTypeGuid<Entity::StaticMeshComponent>()
			};
			return supportedAssetTypes.GetView().Contains(pAssetReference->GetTypeGuid());
		}
		return false;
	}

	bool DistributorComponent::
		ApplyAtPoint(const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags>)
	{
		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			constexpr Array<const Asset::TypeGuid, 2> supportedAssetTypes{
				Reflection::GetTypeGuid<Entity::SceneComponent>(),
				Reflection::GetTypeGuid<Entity::StaticMeshComponent>()
			};
			if (supportedAssetTypes.GetView().Contains(pAssetReference->GetTypeGuid()))
			{
				SetComponentType(*pAssetReference);
				return true;
			}
		}
		return false;
	}

	void DistributorComponent::IterateAttachedItems(
		const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>& callback
	)
	{
		if (!allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
		{
			return;
		}

		Asset::Picker componentType = GetComponentType();
		if (callback(componentType.m_asset) == Memory::CallbackResult::Break)
		{
			return;
		}
	}

	[[maybe_unused]] const bool wasDistributorRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<DistributorComponent>>::Make());
	[[maybe_unused]] const bool wasDistributorTypeRegistered = Reflection::Registry::RegisterType<DistributorComponent>();
}
