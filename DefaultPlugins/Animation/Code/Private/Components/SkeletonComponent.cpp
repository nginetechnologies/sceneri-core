#include "Components/SkeletonComponent.h"
#include "Components/Controllers/LoopingAnimationController.h"
#include "Animation.h"
#include "SkeletonAssetType.h"
#include "AnimationAssetType.h"

#include "Plugin.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Asset/AssetManager.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include "SkeletonIdentifier.h"
#include <Engine/Asset/AssetType.h>

#include "Components/Controllers/AnimationController.h"

namespace ngine::Animation
{
	Controller::Controller(Initializer&& initializer)
		: m_skeletonComponent(initializer.GetParent().AsExpected<SkeletonComponent>(initializer.GetSceneRegistry()))
	{
		// There can only be one controller per skeleton component
		initializer.GetParent().RemoveFirstDataComponentImplementingType<Controller>(initializer.GetSceneRegistry());

		m_skeletonComponent.SetAnimationController(this);
	}
	Controller::Controller(const Controller&, const Cloner& cloner)
		: m_skeletonComponent(cloner.GetParent().AsExpected<SkeletonComponent>(cloner.GetSceneRegistry()))
	{
		// There can only be one controller per skeleton component
		cloner.GetParent().RemoveFirstDataComponentImplementingType<Controller>(cloner.GetSceneRegistry());

		m_skeletonComponent.SetAnimationController(this);
	}
	Controller::Controller(const Deserializer& deserializer)
		: m_skeletonComponent(deserializer.GetParent().AsExpected<SkeletonComponent>(deserializer.GetSceneRegistry()))
	{
		// There can only be one controller per skeleton component
		deserializer.GetParent().RemoveFirstDataComponentImplementingType<Controller>(deserializer.GetSceneRegistry());

		m_skeletonComponent.SetAnimationController(this);
	}
	Controller::~Controller()
	{
		m_skeletonComponent.SetAnimationController(nullptr);
	}

	bool Skeleton::Serialize(const Serialization::Reader reader)
	{
		if (const Optional<Serialization::Reader> jointsReader = reader.FindSerializer("joints"))
		{
			m_jointLookupMap.Reserve(jointsReader->GetValue().GetValue().MemberCount());
			const Serialization::Object& jointsObject = Serialization::Object::GetFromReference(jointsReader->GetValue());
			for (const Serialization::Object::ConstMember joint : jointsObject)
			{
				m_jointLookupMap
					.Emplace(Guid::TryParse(joint.name), JointIndex(*Serialization::Reader(joint.value, reader.GetData()).Read<JointIndex>("index")));
			}
			return true;
		}
		return false;
	}

	[[nodiscard]] SkeletonInstance CreateSkeletonInstance(const SkeletonIdentifier skeletonIdentifier)
	{
		SkeletonCache& skeletonCache = Plugin::GetInstance()->GetSkeletonCache();
		if (skeletonIdentifier.IsValid())
		{
			if (const Optional<const Skeleton*> pSkeleton = skeletonCache.GetSkeleton(skeletonIdentifier))
			{
				return SkeletonInstance{*pSkeleton};
			}
			else
			{
				return SkeletonInstance{};
			}
		}
		else
		{
			return SkeletonInstance{};
		}
	}

	SkeletonComponent::SkeletonComponent(const SkeletonComponent& templateComponent, const Cloner& cloner)
		: Component3D(templateComponent, cloner)
		, m_skeletonIdentifier(templateComponent.m_skeletonIdentifier)
		, m_skeletonInstance(templateComponent.m_skeletonInstance)
	{
	}

	SkeletonComponent::SkeletonComponent(const Deserializer& deserializer)
		: SkeletonComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<SkeletonComponent>().ToString().GetView())
			)
	{
	}

	SkeletonComponent::SkeletonComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer)
		: Component3D(deserializer)
		, m_skeletonIdentifier(Plugin::GetInstance()->GetSkeletonCache().FindOrRegisterAsset(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Guid>("skeleton", Guid{}) : Guid{}
			))
		, m_skeletonInstance(CreateSkeletonInstance(m_skeletonIdentifier))
	{
		DeserializeCustomData(componentSerializer);
	}

	SkeletonComponent::SkeletonComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer))
		, m_skeletonIdentifier(initializer.m_skeletonIdentifier)
		, m_skeletonInstance(CreateSkeletonInstance(initializer.m_skeletonIdentifier))
	{
	}

	void SkeletonComponent::OnCreated()
	{
		if (m_pAnimationController == nullptr)
		{
			m_pAnimationController = FindFirstDataComponentImplementingType<Controller>();
		}

		if (const Optional<const Skeleton*> pSkeleton = m_skeletonInstance.GetSkeleton())
		{
			pSkeleton->OnChanged.Add(
				*this,
				[](SkeletonComponent& component)
				{
					Optional<Controller*> pComponent = component.GetAnimationController();
					if (pComponent.IsValid())
					{
						pComponent->OnSkeletonChanged();
					}

					component.TryEnableUpdate();
				}
			);
		}

		TryEnableUpdate();
	}

	SkeletonComponent::~SkeletonComponent()
	{
		if (const Optional<const Skeleton*> pSkeleton = m_skeletonInstance.GetSkeleton())
		{
			pSkeleton->OnChanged.Remove(this);
		}

		bool expected = true;
		if (m_isUpdateEnabled.CompareExchangeStrong(expected, false))
		{
			Entity::ComponentTypeSceneData<SkeletonComponent>& typeSceneData =
				static_cast<Entity::ComponentTypeSceneData<SkeletonComponent>&>(*GetTypeSceneData());
			typeSceneData.DisableUpdate(*this);
		}
	}

	void SkeletonComponent::SetAnimationController(Controller* pAnimationController)
	{
		m_pAnimationController = pAnimationController;
		TryEnableUpdate();
	}

	void SkeletonComponent::SetSkeleton(const SkeletonReference asset)
	{
		SkeletonCache& skeletonCache = Plugin::GetInstance()->GetSkeletonCache();
		m_skeletonIdentifier = skeletonCache.FindOrRegisterAsset(asset.GetAssetGuid());
		if (const Optional<const Skeleton*> pSkeleton = skeletonCache.GetSkeleton(m_skeletonIdentifier))
		{
			pSkeleton->OnChanged.Add(
				*this,
				[](SkeletonComponent& component)
				{
					Optional<Controller*> pComponent = component.GetAnimationController();
					if (pComponent.IsValid())
					{
						pComponent->OnSkeletonChanged();
					}
				}
			);
			m_skeletonInstance = SkeletonInstance(*pSkeleton);

			Threading::JobBatch jobBatch = skeletonCache.TryLoadSkeleton(
				m_skeletonIdentifier,
				System::Get<Asset::Manager>(),
				[this](const SkeletonIdentifier)
				{
					TryEnableUpdate();
					if (const Optional<const Skeleton*> pSkeleton = m_skeletonInstance.GetSkeleton())
					{
						m_skeletonInstance.ProcessLocalToModelSpace(pSkeleton->GetJointBindPoses());
					}
				}
			);
			if (jobBatch.IsValid())
			{
				if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
				{
					pThread->Queue(jobBatch);
				}
				else
				{
					System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadSkeleton);
				}
			}
		}
	}

	Threading::JobBatch SkeletonComponent::SetDeserializedSkeleton(
		const SkeletonReference asset,
		[[maybe_unused]] const Serialization::Reader objectReader,
		[[maybe_unused]] const Serialization::Reader typeReader
	)
	{
		SkeletonCache& skeletonCache = Plugin::GetInstance()->GetSkeletonCache();
		m_skeletonIdentifier = skeletonCache.FindOrRegisterAsset(asset.GetAssetGuid());
		if (const Optional<const Skeleton*> pSkeleton = skeletonCache.GetSkeleton(m_skeletonIdentifier))
		{
			m_skeletonInstance = SkeletonInstance(*pSkeleton);

			Threading::JobBatch loadSkeletonBatch = skeletonCache.TryLoadSkeleton(
				m_skeletonIdentifier,
				System::Get<Asset::Manager>(),
				[this](const SkeletonIdentifier)
				{
					TryEnableUpdate();
					if (const Optional<const Skeleton*> pSkeleton = m_skeletonInstance.GetSkeleton())
					{
						m_skeletonInstance.ProcessLocalToModelSpace(pSkeleton->GetJointBindPoses());
					}
				}
			);
			return loadSkeletonBatch;
		}
		else
		{
			return {};
		}
	}

	SkeletonComponent::SkeletonReference SkeletonComponent::GetSkeleton() const
	{
		SkeletonCache& skeletonCache = Plugin::GetInstance()->GetSkeletonCache();
		const SkeletonIdentifier skeletonIdentifier = m_skeletonIdentifier;
		return SkeletonReference{
			skeletonIdentifier.IsValid() ? skeletonCache.GetAssetGuid(m_skeletonIdentifier) : Asset::Guid{},
			SkeletonAssetType::AssetFormat.assetTypeGuid
		};
	}

	bool SkeletonComponent::CanEnableUpdate() const
	{
		return IsEnabled() && m_pAnimationController != nullptr && m_pAnimationController->ShouldUpdate() &&
		       m_skeletonInstance.GetSkeleton().IsValid() && m_skeletonInstance.GetSkeleton()->IsValid();
	}

	void SkeletonComponent::TryEnableUpdate()
	{
		if (CanEnableUpdate())
		{
			bool expected = false;
			if (m_isUpdateEnabled.CompareExchangeStrong(expected, true))
			{
				Entity::ComponentTypeSceneData<SkeletonComponent>& typeSceneData =
					static_cast<Entity::ComponentTypeSceneData<SkeletonComponent>&>(*GetTypeSceneData());
				typeSceneData.EnableUpdate(*this);

				OnToggledUpdate();
			}
		}
		else
		{
			bool expected = true;
			if (m_isUpdateEnabled.CompareExchangeStrong(expected, false))
			{
				Entity::ComponentTypeSceneData<SkeletonComponent>& typeSceneData =
					static_cast<Entity::ComponentTypeSceneData<SkeletonComponent>&>(*GetTypeSceneData());
				typeSceneData.DisableUpdate(*this);

				OnToggledUpdate();
			}
		}
	}

	void SkeletonComponent::Update()
	{
		Assert(CanEnableUpdate());
		// TODO: Fix this possibility when parent is null
		if (UNLIKELY(!CanEnableUpdate()))
		{
			return;
		}

		m_pAnimationController->Update();
		// Converts from local space to model space matrices.
		m_skeletonInstance.ProcessLocalToModelSpace(m_skeletonInstance.GetSampledTransforms());
	}

	void SkeletonComponent::OnEnable()
	{
		TryEnableUpdate();
	}

	void SkeletonComponent::OnDisable()
	{
		bool expected = true;
		if (m_isUpdateEnabled.CompareExchangeStrong(expected, false))
		{
			Entity::ComponentTypeSceneData<SkeletonComponent>& typeSceneData =
				static_cast<Entity::ComponentTypeSceneData<SkeletonComponent>&>(*GetTypeSceneData());
			typeSceneData.DisableUpdate(*this);

			OnToggledUpdate();
		}
	}

	bool SkeletonComponent::
		CanApplyAtPoint(const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags>)
			const
	{
		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			static constexpr Array<const Guid, 2> compatibleAssetTypes = {
				SkeletonAssetType::AssetFormat.assetTypeGuid,
				AnimationAssetType::AssetFormat.assetTypeGuid
			};
			return compatibleAssetTypes.GetView().Contains(pAssetReference->GetTypeGuid());
		}
		return false;
	}

	bool SkeletonComponent::
		ApplyAtPoint(const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags>)
	{
		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			if (pAssetReference->GetTypeGuid() == SkeletonAssetType::AssetFormat.assetTypeGuid)
			{
				SetSkeleton(*pAssetReference);
				return true;
			}
			else if (pAssetReference->GetTypeGuid() == AnimationAssetType::AssetFormat.assetTypeGuid)
			{
				if (m_pAnimationController != nullptr)
				{
					m_pAnimationController->ApplyAnimation(pAssetReference->GetAssetGuid());
					return true;
				}
				else
				{
					AnimationCache& animationCache = Plugin::GetInstance()->GetAnimationCache();
					const AnimationIdentifier animationIdentifier = animationCache.FindOrRegisterAsset(pAssetReference->GetAssetGuid());
					CreateDataComponent<LoopingAnimationController>(LoopingAnimationController::Initializer{
						Entity::Data::Component3D::DynamicInitializer{*this, GetSceneRegistry()},
						animationIdentifier
					});
					return true;
				}
			}
		}
		return false;
	}

	void SkeletonComponent::IterateAttachedItems(
		[[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes,
		const Function<Memory::CallbackResult(ConstAnyView), 36>& callback
	)
	{
		if (!allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
		{
			return;
		}

		Asset::Reference skeleton = GetSkeleton().GetAssetReference();
		if (callback(skeleton) == Memory::CallbackResult::Break)
		{
			return;
		}

		if (m_pAnimationController != nullptr)
		{
			m_pAnimationController->IterateAnimations(callback);
		}
	}

	[[maybe_unused]] const bool wasSkeletonRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SkeletonComponent>>::Make());
	[[maybe_unused]] const bool wasSkeletonTypeRegistered = Reflection::Registry::RegisterType<SkeletonComponent>();
}
