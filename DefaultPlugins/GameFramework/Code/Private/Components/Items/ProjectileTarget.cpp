#include <GameFramework/Components/Items/ProjectileTarget.h>

#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Common/Reflection/Registry.inl>
#include <PhysicsCore/Components/Data/BodyComponent.h>
#include <PhysicsCore/Components/Data/SceneComponent.h>

#include <GameFramework/Components/SceneRules/SceneRules.h>
#include <GameFramework/Components/SceneRules/Modules/ScoreModule.h>
#include <GameFramework/Components/SceneRules/Modules/HealthModule.h>
#include <GameFramework/Components/Player/Player.h>
#include <GameFramework/Components/Player/KDCounter.h>
#include <GameFramework/Reset/ResetComponent.h>

// #include <VisualDebug/Plugin.h>

namespace ngine::GameFramework
{

	ProjectileTarget::ProjectileTarget(const Deserializer& deserializer)
		: m_owner{deserializer.GetParent()}
	{
	}

	ProjectileTarget::ProjectileTarget(const ProjectileTarget&, const Cloner& cloner)
		: m_owner{cloner.GetParent()}
	{
	}

	ProjectileTarget::ProjectileTarget(Initializer&& initializer)
		: m_owner{initializer.GetParent()}
	{
	}

	void ProjectileTarget::HitTarget(const HitSettings& hitSettings)
	{
		if (m_isEnabled)
		{
			OnHit(hitSettings);
		}
	}

	[[maybe_unused]] const bool wasProjectileTargetRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ProjectileTarget>>::Make());
	[[maybe_unused]] const bool wasProjectileTargetTypeRegistered = Reflection::Registry::RegisterType<ProjectileTarget>();

	// PROJECTILE TARGET SCORE ATTACHMENT

	ProjectileTargetScoreAttachment::ProjectileTargetScoreAttachment(const Deserializer&)
	{
	}

	ProjectileTargetScoreAttachment::ProjectileTargetScoreAttachment(const ProjectileTargetScoreAttachment& templateComponent, const Cloner&)
		: m_score{templateComponent.m_score}
	{
	}

	ProjectileTargetScoreAttachment::ProjectileTargetScoreAttachment(Initializer&&)
	{
	}

	void ProjectileTargetScoreAttachment::OnParentCreated(Entity::Component3D& parent)
	{
		if (Optional<ProjectileTarget*> pProjectileTarget = parent.FindDataComponentOfType<ProjectileTarget>())
		{
			pProjectileTarget->OnHit.Add(
				*this,
				[this, &parent](ProjectileTargetScoreAttachment&, const ProjectileTarget::HitSettings& hitSettings)
				{
					const Entity::DataComponentResult<ScoreModule> scoreModuleQueryResult =
						SceneRules::FindModule<ScoreModule>(parent.GetRootSceneComponent());
					Assert(scoreModuleQueryResult.IsValid());
					if (LIKELY(scoreModuleQueryResult.IsValid()))
					{
						scoreModuleQueryResult->AddScore(*scoreModuleQueryResult.m_pDataComponentOwner, hitSettings.shooterClientIdentifier, m_score);
					}
				}
			);
		}
	}

	[[maybe_unused]] const bool wasProjectileTargetScoreAttachmentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ProjectileTargetScoreAttachment>>::Make());
	[[maybe_unused]] const bool wasProjectileTargetScoreAttachmentTypeRegistered =
		Reflection::Registry::RegisterType<ProjectileTargetScoreAttachment>();

	// PROJECTILE TARGET DAMAGE ATTACHMENT

	ProjectileTargetDamageAttachment::ProjectileTargetDamageAttachment(const Deserializer&)
	{
	}
	ProjectileTargetDamageAttachment::
		ProjectileTargetDamageAttachment(const ProjectileTargetDamageAttachment& templateComponent, const Cloner&)
		: m_currentDamage{templateComponent.m_currentDamage}
		, m_minimumDamage{templateComponent.m_minimumDamage}
		, m_maximumDamage{templateComponent.m_maximumDamage}
		, m_damageMultiplier{templateComponent.m_damageMultiplier}
		, m_shouldRemoveTarget{templateComponent.m_shouldRemoveTarget}
		, m_shouldCountAsKill{templateComponent.m_shouldCountAsKill}
	{
	}
	ProjectileTargetDamageAttachment::ProjectileTargetDamageAttachment(Initializer&&)
	{
	}

	void ProjectileTargetDamageAttachment::OnParentCreated(Entity::Component3D& parent)
	{
		m_currentDamage = m_minimumDamage;

		if (Optional<ProjectileTarget*> pProjectileTarget = parent.FindDataComponentOfType<ProjectileTarget>())
		{
			pProjectileTarget->OnHit.Add(
				*this,
				[this, &parent, pProjectileTarget](ProjectileTargetDamageAttachment&, const ProjectileTarget::HitSettings& hitSettings)
				{
					m_currentDamage += hitSettings.damage * m_damageMultiplier;
					if (m_currentDamage >= m_maximumDamage)
					{

						// Setup Reset
						Entity::SceneRegistry& sceneRegistry = parent.GetSceneRegistry();
						if (!parent.HasAnyDataComponentsImplementingType<Data::Reset>(sceneRegistry))
						{
							parent.CreateDataComponent<Data::Reset>(Data::Reset::Initializer{
								Entity::Data::Component3D::DynamicInitializer{parent, sceneRegistry},
								[this, pProjectileTarget](Entity::Component3D& owner)
								{
									if (owner.IsDisabled())
									{
										owner.EnableWithChildren();
									}
									m_currentDamage = m_minimumDamage;
									pProjectileTarget->Enable();
								},
								parent
							});
						}

						/*if (m_shouldRemoveTarget)
					  {
					    parent.DisableWithChildren();
					  }

					  pProjectileTarget->Disable();*/

						if (m_shouldCountAsKill)
						{
							if (Optional<KDCounter*> pKDCounterComponent = parent.GetRootSceneComponent().FindFirstDataComponentOfTypeInChildrenRecursive<KDCounter>())
							{
								pKDCounterComponent->AddKill();
							}
						}
					}
				}
			);
		}
	}

	[[maybe_unused]] const bool wasProjectileTargetDamageAttachmentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ProjectileTargetDamageAttachment>>::Make());
	[[maybe_unused]] const bool wasProjectileTargetDamageAttachmentTypeRegistered =
		Reflection::Registry::RegisterType<ProjectileTargetDamageAttachment>();

	// PROJECTILE TARGET HEALTH ATTACHMENT

	ProjectileTargetHealthAttachment::ProjectileTargetHealthAttachment(const Deserializer&)
	{
	}

	ProjectileTargetHealthAttachment::
		ProjectileTargetHealthAttachment(const ProjectileTargetHealthAttachment& templateComponent, const Cloner&)
		: m_healthReductionMultiplier(templateComponent.m_healthReductionMultiplier)
	{
	}

	ProjectileTargetHealthAttachment::ProjectileTargetHealthAttachment(Initializer&&)
	{
	}

	void ProjectileTargetHealthAttachment::OnParentCreated(Entity::Component3D& parent)
	{
		if (Optional<ProjectileTarget*> pProjectileTarget = parent.FindDataComponentOfType<ProjectileTarget>())
		{
			pProjectileTarget->OnHit.Add(
				*this,
				[this, &parent](ProjectileTargetHealthAttachment&, const ProjectileTarget::HitSettings& hitSettings)
				{
					const Entity::DataComponentResult<HealthModule> healthModuleQueryResult =
						SceneRules::FindModule<HealthModule>(parent.GetRootSceneComponent());
					Assert(healthModuleQueryResult.IsValid());
					if (LIKELY(healthModuleQueryResult.IsValid()))
					{
						if (const Optional<Player*> pPlayer = parent.GetParentSceneComponent()->FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Player>(parent.GetSceneRegistry()))
						{
							healthModuleQueryResult->AddHealth(
								*healthModuleQueryResult.m_pDataComponentOwner,
								pPlayer->GetClientIdentifier(),
								hitSettings.damage * -m_healthReductionMultiplier
							);
						}
					}
				}
			);
		}
	}

	[[maybe_unused]] const bool wasProjectileTargetHealthAttachmentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ProjectileTargetHealthAttachment>>::Make());
	[[maybe_unused]] const bool wasProjectileTargetHealthAttachmentTypeRegistered =
		Reflection::Registry::RegisterType<ProjectileTargetHealthAttachment>();
}
