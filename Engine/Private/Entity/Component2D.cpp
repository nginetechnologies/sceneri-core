#include <Engine/Entity/Component2D.h>
#include <Engine/Entity/RootSceneComponent2D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Data/WorldTransform2D.h>
#include <Engine/Entity/Data/LocalTransform2D.h>
#include <Engine/Entity/Data/ParentComponent.h>
#include <Engine/Entity/Data/QuadtreeNode.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/Data/Component.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Scene/Scene2D.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity
{
	Component2D::Component2D(
		const ComponentIdentifier identifier,
		HierarchyComponentBase& parent,
		RootSceneComponent2D& rootSceneComponent,
		SceneRegistry& sceneRegistry,
		const Math::WorldTransform2D worldTransform,
		const Guid instanceGuid,
		const EnumFlags<Flags> flags
	)
		: HierarchyComponent(identifier, parent, sceneRegistry, flags | Flags::Is2D, instanceGuid)
		, m_rootSceneComponent(rootSceneComponent)
	{
		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::WorldTransform2D>();
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::LocalTransform2D>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform2D parentWorldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		[[maybe_unused]] const Optional<Data::WorldTransform2D*> pWorldTransform =
			CreateDataComponent<Data::WorldTransform2D>(worldTransformSceneData, worldTransform);
		Assert(pWorldTransform.IsValid());
		[[maybe_unused]] const Optional<Data::LocalTransform2D*> pLocalTransform = CreateDataComponent<Data::LocalTransform2D>(
			localTransformSceneData,
			Math::Transform2Df{
				parentWorldTransform.InverseTransformRotation(worldTransform.GetRotation()),
				parentWorldTransform.InverseTransformLocationWithoutScale(worldTransform.GetLocation())
			}
		);
		Assert(pLocalTransform.IsValid());
	}

	Component2D::Component2D(
		RootSceneComponentType,
		const ComponentIdentifier identifier,
		const Optional<HierarchyComponentBase*> pParent,
		SceneRegistry& sceneRegistry,
		const Guid instanceGuid,
		const EnumFlags<Flags> flags
	)
		: HierarchyComponent(identifier, pParent, sceneRegistry, flags | Flags::Is2D, instanceGuid)
		, m_rootSceneComponent(static_cast<RootSceneComponent2D&>(*this))
	{
		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::WorldTransform2D>();
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::LocalTransform2D>();

		[[maybe_unused]] const Optional<Data::WorldTransform2D*> pWorldTransform =
			CreateDataComponent<Data::WorldTransform2D>(worldTransformSceneData, Math::Identity);
		Assert(pWorldTransform.IsValid());
		[[maybe_unused]] const Optional<Data::LocalTransform2D*> pLocalTransform =
			CreateDataComponent<Data::LocalTransform2D>(localTransformSceneData, Math::Identity);
		Assert(pLocalTransform.IsValid());
	}

	Component2D::Component2D(const Component2D& templateComponent, const Cloner& cloner)
		: HierarchyComponent(templateComponent, cloner)
		, m_rootSceneComponent(cloner.GetParent()->GetRootSceneComponent())
	{
		Assert(GetFlags(cloner.GetSceneRegistry()).IsSet(Flags::Is2D));
		SceneRegistry& sceneRegistry = cloner.GetSceneRegistry();

		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::WorldTransform2D>();
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::LocalTransform2D>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform2D parentWorldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		const Math::Transform2Df localTransform = templateComponent.GetRelativeTransform(cloner.GetTemplateSceneRegistry());
		const Math::WorldTransform2D templateWorldTransform = templateComponent.GetWorldTransform(cloner.GetTemplateSceneRegistry());
		[[maybe_unused]] const Optional<Data::LocalTransform2D*> pLocalTransform =
			CreateDataComponent<Data::LocalTransform2D>(localTransformSceneData, localTransform);

		[[maybe_unused]] const Optional<Data::WorldTransform2D*> pWorldTransform = CreateDataComponent<Data::WorldTransform2D>(
			worldTransformSceneData,
			Math::WorldTransform2D{
				parentWorldTransform.TransformRotation(localTransform.GetRotation()),
				parentWorldTransform.TransformLocationWithoutScale(localTransform.GetLocation()),
				templateWorldTransform.GetScale()
			}
		);
		Assert(pWorldTransform.IsValid());
		// here
		Assert(pLocalTransform.IsValid());
	}

	Component2D::Component2D(Initializer&& __restrict initializer)
		: HierarchyComponent(
				initializer.GetSceneRegistry().AcquireNewComponentIdentifier(),
				*initializer.GetParent(),
				initializer.GetSceneRegistry(),
				initializer.GetFlags() | Flags::Is2D,
				initializer.GetInstanceGuid()
			)
		, m_rootSceneComponent(initializer.GetParent()->GetRootSceneComponent())
	{
		Entity::SceneRegistry& sceneRegistry = initializer.GetSceneRegistry();

		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::WorldTransform2D>();
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::LocalTransform2D>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform2D parentWorldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		[[maybe_unused]] const Optional<Data::WorldTransform2D*> pWorldTransform =
			CreateDataComponent<Data::WorldTransform2D>(worldTransformSceneData, initializer.m_worldTransform);
		Assert(pWorldTransform.IsValid());
		[[maybe_unused]] const Optional<Data::LocalTransform2D*> pLocalTransform = CreateDataComponent<Data::LocalTransform2D>(
			localTransformSceneData,
			Math::Transform2Df{
				parentWorldTransform.InverseTransformRotation(initializer.m_worldTransform.GetRotation()),
				parentWorldTransform.InverseTransformLocationWithoutScale(initializer.m_worldTransform.GetLocation())
			}
		);
		Assert(pLocalTransform.IsValid());
	}

	Component2D::Component2D(const Deserializer& deserializer)
		: Component2D(Initializer{
				*deserializer.GetParent(),
				Math::WorldTransform2D{Math::Identity},
				deserializer.GetFlags() | Flags::SaveToDisk,
				deserializer.m_reader.ReadWithDefaultValue<Guid>("instanceGuid", Guid::Generate()),
			})
	{
		Tag::Mask tagMask = Tag::Mask();
		deserializer.m_reader.Serialize("tags", tagMask, System::Get<Tag::Registry>());

		if (tagMask.AreAnySet())
		{
			Entity::ComponentTypeSceneData<Data::Tags>& tagsSceneData = deserializer.GetSceneRegistry().GetCachedSceneData<Data::Tags>();
			[[maybe_unused]] const Optional<Data::Tags*> pTags = CreateDataComponent<Data::Tags>(
				tagsSceneData,
				Data::Tags::Initializer{Data::Tags::BaseType::DynamicInitializer{*this, deserializer.GetSceneRegistry()}, tagMask}
			);
			Assert(pTags.IsValid());
		}
	}

	bool Component2D::Destroy(SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		const Optional<Data::Flags*> pFlags = flagsSceneData.GetComponentImplementation(GetIdentifier());
		if (LIKELY(pFlags.IsValid()))
		{
			AtomicEnumFlags<Flags>& flags = *pFlags;

			if (flags.TrySetFlags(Flags::IsDestroying))
			{
				{
					const EnumFlags<ComponentFlags> previousFlags = flags.FetchOr(ComponentFlags::IsDisabledWithChildren);
					if (!previousFlags.AreAnySet(Flags::IsDisabledFromAnySource))
					{
						DisableInternal(sceneRegistry);
					}
				}

				{
					const EnumFlags<ComponentFlags> previousFlags = flags.FetchOr(ComponentFlags::IsDetachedFromTree);

					if (!previousFlags.AreAnySet(Flags::IsDetachedFromTreeFromAnySource))
					{
						Optional<ComponentTypeSceneDataInterface*> pSceneData = GetTypeSceneData(sceneRegistry);
						pSceneData->DetachInstanceFromTree(*this, GetParentSafe());
					}

					if (const Optional<Component2D*> pParent = GetParentSafe(); pParent.IsValid())
					{
						if (previousFlags.IsNotSet(ComponentFlags::IsDetachedFromTree))
						{
							OnBeforeDetachFromParent();
							pParent->RemoveChildAndClearParent(*this, sceneRegistry);
							pParent->OnChildDetached(*this);
						}
						Assert(!pParent->GetChildren().Contains(*this));
					}
				}

				while (HasChildren())
				{
					Component2D& child = GetChild(0);
					Assert(child.GetParentSafe() == this);
					child.Destroy(sceneRegistry);
				}

				GetRootScene().QueueComponentDestruction(*this);
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	void Component2D::OnAttachedToTree([[maybe_unused]] const Optional<Component2D*> pParent)
	{
		Assert(false, "TODO");
		// GetRootSceneComponent().AddComponent(*this);
	}

	void Component2D::OnDetachedFromTree([[maybe_unused]] const Optional<Component2D*> pParent)
	{
		Assert(false, "TODO");
		// GetRootSceneComponent().RemoveComponent(*this);
	}

	PURE_STATICS Entity::SceneRegistry& Component2D::GetSceneRegistry() const
	{
		return GetRootScene().GetEntitySceneRegistry();
	}

	PURE_STATICS Scene2D& Component2D::GetRootScene() const
	{
		return GetRootSceneComponent().GetScene();
	}

	FrameTime Component2D::GetCurrentFrameTime() const
	{
		return GetRootScene().GetCurrentFrameTime();
	}

	[[nodiscard]] Math::WorldTransform2D Component2D::GetWorldTransform(const SceneRegistry& sceneRegistry) const
	{
		const Optional<Data::WorldTransform2D*> pWorldTransform =
			DataComponentOwner::FindDataComponentOfType<Data::WorldTransform2D>(sceneRegistry);
		if (LIKELY(pWorldTransform.IsValid()))
		{
			return pWorldTransform->GetTransform();
		}
		else
		{
			return Math::Identity;
		}
	}
	void Component2D::SetWorldTransform(const Math::WorldTransform2D transform, const SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::WorldTransform2D>(
		);
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::LocalTransform2D>(
		);
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform2D parentWorldTransform = (Math::WorldTransform2D
		)worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		Data::WorldTransform2D& worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Data::LocalTransform2D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		worldTransform.SetTransform(transform);

		const Math::WorldTransform2D newWorldTransform = worldTransform;
		// localTransform.SetTransform(parentWorldTransform.InverseTransform(transform));
		localTransform.SetTransform(Math::Transform2Df{
			parentWorldTransform.InverseTransformRotation(newWorldTransform.GetRotation()),
			parentWorldTransform.InverseTransformLocationWithoutScale(newWorldTransform.GetLocation())
		});

		OnWorldTransformChangedInternal(transform, worldTransformSceneData, localTransformSceneData);
	}

	[[nodiscard]] Math::Transform2Df Component2D::GetRelativeTransform(const SceneRegistry& sceneRegistry) const
	{
		const Optional<Data::LocalTransform2D*> pLocalTransform =
			DataComponentOwner::FindDataComponentOfType<Data::LocalTransform2D>(sceneRegistry);
		Assert(pLocalTransform.IsValid());
		if (LIKELY(pLocalTransform.IsValid()))
		{
			return pLocalTransform->GetTransform();
		}
		else
		{
			return Math::Identity;
		}
	}
	void Component2D::SetRelativeTransform(const Math::Transform2Df transform, const SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::WorldTransform2D>(
		);
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::LocalTransform2D>(
		);
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform2D parentWorldTransform = (Math::WorldTransform2D
		)worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		Data::WorldTransform2D& worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Data::LocalTransform2D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		// const Math::WorldTransform2D newWorldTransform = parentWorldTransform.Transform(transform);
		const Math::WorldTransform2D newWorldTransform = Math::WorldTransform2D{
			parentWorldTransform.TransformRotation(transform.GetRotation()),
			parentWorldTransform.TransformLocationWithoutScale(transform.GetLocation()),
			worldTransform.GetScale()
		};
		worldTransform.SetTransform(newWorldTransform);
		localTransform.SetTransform(transform);

		OnWorldTransformChangedInternal(worldTransform, worldTransformSceneData, localTransformSceneData);
	}

	[[nodiscard]] Math::WorldCoordinate2D Component2D::GetWorldLocation(const SceneRegistry& sceneRegistry) const
	{
		const Optional<Data::WorldTransform2D*> pWorldTransform =
			DataComponentOwner::FindDataComponentOfType<Data::WorldTransform2D>(sceneRegistry);
		Assert(pWorldTransform.IsValid());
		if (LIKELY(pWorldTransform.IsValid()))
		{
			return pWorldTransform->GetLocation();
		}
		else
		{
			return Math::Zero;
		}
	}
	void Component2D::SetWorldLocation(const Math::WorldCoordinate2D location, const SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::WorldTransform2D>(
		);
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::LocalTransform2D>(
		);
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform2D parentWorldTransform = (Math::WorldTransform2D
		)worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		Data::WorldTransform2D& worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Data::LocalTransform2D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		worldTransform.SetLocation(location);

		// localTransform.SetTransform(parentWorldTransform.InverseTransform(worldTransform));
		const Math::WorldTransform2D newWorldTransform = worldTransform;
		localTransform.SetTransform(Math::Transform2Df{
			parentWorldTransform.InverseTransformRotation(newWorldTransform.GetRotation()),
			parentWorldTransform.InverseTransformLocationWithoutScale(newWorldTransform.GetLocation())
		});

		OnWorldTransformChangedInternal(worldTransform, worldTransformSceneData, localTransformSceneData);
	}

	[[nodiscard]] Math::Vector2f Component2D::GetRelativeLocation(const SceneRegistry& sceneRegistry) const
	{
		const Optional<Data::LocalTransform2D*> pLocalTransform =
			DataComponentOwner::FindDataComponentOfType<Data::LocalTransform2D>(sceneRegistry);
		Assert(pLocalTransform.IsValid());
		if (LIKELY(pLocalTransform.IsValid()))
		{
			return pLocalTransform->GetLocation();
		}
		else
		{
			return Math::Zero;
		}
	}
	void Component2D::SetRelativeLocation(const Math::Vector2f location, const SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::WorldTransform2D>(
		);
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::LocalTransform2D>(
		);
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform2D parentWorldTransform = (Math::WorldTransform2D
		)worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		Data::WorldTransform2D& worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Data::LocalTransform2D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		localTransform.SetLocation(location);

		// const Math::WorldTransform2D newWorldTransform = parentWorldTransform.Transform(transform);
		const Math::WorldTransform2D newWorldTransform = Math::WorldTransform2D{
			parentWorldTransform.TransformRotation(localTransform.GetRotation()),
			parentWorldTransform.TransformLocationWithoutScale(localTransform.GetLocation()),
			worldTransform.GetScale()
		};
		worldTransform.SetTransform(newWorldTransform);

		OnWorldTransformChangedInternal(worldTransform, worldTransformSceneData, localTransformSceneData);
	}

	[[nodiscard]] Math::WorldScale2D Component2D::GetWorldScale(const SceneRegistry& sceneRegistry) const
	{
		const Optional<Data::WorldTransform2D*> pWorldTransform =
			DataComponentOwner::FindDataComponentOfType<Data::WorldTransform2D>(sceneRegistry);
		Assert(pWorldTransform.IsValid());
		if (LIKELY(pWorldTransform.IsValid()))
		{
			return pWorldTransform->GetScale();
		}
		else
		{
			return Math::Zero;
		}
	}
	void Component2D::SetWorldScale(const Math::WorldScale2D scale, const SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::WorldTransform2D>(
		);
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::LocalTransform2D>(
		);
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform2D parentWorldTransform = (Math::WorldTransform2D
		)worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		Data::WorldTransform2D& worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Data::LocalTransform2D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		worldTransform.SetScale(scale);
		// localTransform.SetTransform(parentWorldTransform.InverseTransform(worldTransform));
		const Math::WorldTransform2D newWorldTransform = worldTransform;
		localTransform.SetTransform(Math::Transform2Df{
			parentWorldTransform.InverseTransformRotation(newWorldTransform.GetRotation()),
			parentWorldTransform.InverseTransformLocationWithoutScale(newWorldTransform.GetLocation())
		});

		OnWorldTransformChangedInternal(worldTransform, worldTransformSceneData, localTransformSceneData);
	}

	[[nodiscard]] Math::Vector2f Component2D::GetRelativeScale(const SceneRegistry& sceneRegistry) const
	{
		const Optional<Data::LocalTransform2D*> pLocalTransform =
			DataComponentOwner::FindDataComponentOfType<Data::LocalTransform2D>(sceneRegistry);
		Assert(pLocalTransform.IsValid());
		if (LIKELY(pLocalTransform.IsValid()))
		{
			return pLocalTransform->GetScale();
		}
		else
		{
			return Math::Zero;
		}
	}
	void Component2D::SetRelativeScale(const Math::Vector2f scale, const SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::WorldTransform2D>(
		);
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::LocalTransform2D>(
		);
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform2D parentWorldTransform = (Math::WorldTransform2D
		)worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		Data::WorldTransform2D& worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Data::LocalTransform2D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		localTransform.SetScale(scale);

		// const Math::WorldTransform2D newWorldTransform = parentWorldTransform.Transform(transform);
		const Math::WorldTransform2D newWorldTransform = Math::WorldTransform2D{
			parentWorldTransform.TransformRotation(localTransform.GetRotation()),
			parentWorldTransform.TransformLocationWithoutScale(localTransform.GetLocation()),
			worldTransform.GetScale()
		};
		worldTransform.SetTransform(newWorldTransform);

		OnWorldTransformChangedInternal(worldTransform, worldTransformSceneData, localTransformSceneData);
	}

	[[nodiscard]] Math::WorldRotation2D Component2D::GetWorldRotation(const SceneRegistry& sceneRegistry) const
	{
		const Optional<Data::WorldTransform2D*> pWorldTransform =
			DataComponentOwner::FindDataComponentOfType<Data::WorldTransform2D>(sceneRegistry);
		Assert(pWorldTransform.IsValid());
		if (LIKELY(pWorldTransform.IsValid()))
		{
			return pWorldTransform->GetRotation();
		}
		else
		{
			return 0_degrees;
		}
	}

	void Component2D::SetWorldRotation(const Math::WorldRotation2D rotation, const SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::WorldTransform2D>(
		);
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::LocalTransform2D>(
		);
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform2D parentWorldTransform = (Math::WorldTransform2D
		)worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		Data::WorldTransform2D& worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Data::LocalTransform2D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		worldTransform.SetRotation(rotation);
		const Math::WorldTransform2D newWorldTransform = worldTransform;

		// localTransform.SetTransform(parentWorldTransform.InverseTransform(worldTransform));
		localTransform.SetTransform(Math::Transform2Df{
			parentWorldTransform.InverseTransformRotation(newWorldTransform.GetRotation()),
			parentWorldTransform.InverseTransformLocationWithoutScale(newWorldTransform.GetLocation())
		});

		OnWorldTransformChangedInternal(worldTransform, worldTransformSceneData, localTransformSceneData);
	}

	[[nodiscard]] Math::Rotation2Df Component2D::GetRelativeRotation(const SceneRegistry& sceneRegistry) const
	{
		const Optional<Data::LocalTransform2D*> pLocalTransform =
			DataComponentOwner::FindDataComponentOfType<Data::LocalTransform2D>(sceneRegistry);
		Assert(pLocalTransform.IsValid());
		if (LIKELY(pLocalTransform.IsValid()))
		{
			return pLocalTransform->GetRotation();
		}
		else
		{
			return 0_degrees;
		}
	}

	void Component2D::SetRelativeRotation(const Math::Rotation2Df rotation, const SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::WorldTransform2D>(
		);
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData = *sceneRegistry.FindComponentTypeData<Data::LocalTransform2D>(
		);
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform2D parentWorldTransform = (Math::WorldTransform2D
		)worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		Data::WorldTransform2D& worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Data::LocalTransform2D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		localTransform.SetRotation(rotation);

		// const Math::WorldTransform2D newWorldTransform = parentWorldTransform.Transform(transform);
		const Math::WorldTransform2D newWorldTransform = Math::WorldTransform2D{
			parentWorldTransform.TransformRotation(localTransform.GetRotation()),
			parentWorldTransform.TransformLocationWithoutScale(localTransform.GetLocation()),
			worldTransform.GetScale()
		};
		worldTransform.SetTransform(newWorldTransform);

		OnWorldTransformChangedInternal(worldTransform, worldTransformSceneData, localTransformSceneData);
	}

	[[nodiscard]] Math::Vector2f Component2D::GetWorldForwardDirection(const SceneRegistry& sceneRegistry) const
	{
		const Optional<Data::WorldTransform2D*> pWorldTransform =
			DataComponentOwner::FindDataComponentOfType<Data::WorldTransform2D>(sceneRegistry);
		Assert(pWorldTransform.IsValid());
		if (LIKELY(pWorldTransform.IsValid()))
		{
			return pWorldTransform->GetForwardColumn();
		}
		else
		{
			return Math::Forward;
		}
	}
	[[nodiscard]] Math::Vector2f Component2D::GetWorldUpDirection(const SceneRegistry& sceneRegistry) const
	{
		const Optional<Data::WorldTransform2D*> pWorldTransform =
			DataComponentOwner::FindDataComponentOfType<Data::WorldTransform2D>(sceneRegistry);
		Assert(pWorldTransform.IsValid());
		if (LIKELY(pWorldTransform.IsValid()))
		{
			return pWorldTransform->GetUpColumn();
		}
		else
		{
			return Math::Up;
		}
	}

	void Component2D::OnWorldTransformChangedInternal(
		const Math::WorldTransform2D worldTransform,
		ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData,
		ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData
	)
	{
		for (Component2D& child : GetChildren())
		{
			Data::WorldTransform2D& __restrict childWorldTransform =
				worldTransformSceneData.GetComponentImplementationUnchecked(child.GetIdentifier());
			Data::LocalTransform2D& __restrict childLocalTransform =
				localTransformSceneData.GetComponentImplementationUnchecked(child.GetIdentifier());
			// childWorldTransform = worldTransform.Transform(childLocalTransform);
			childWorldTransform = Math::Transform2Df{
				worldTransform.TransformRotation(childLocalTransform.GetRotation()),
				worldTransform.TransformLocationWithoutScale(childLocalTransform.GetLocation()),
				childWorldTransform.GetScale()
			};

			child.OnWorldTransformChangedInternal(childWorldTransform, worldTransformSceneData, localTransformSceneData);
		}
	}

	void Component2D::DeserializeCustomData(const Optional<Serialization::Reader> serializer)
	{
		SceneRegistry& sceneRegistry = GetSceneRegistry();

		AtomicEnumFlags<Flags>& flags = *FindDataComponentOfType<Data::Flags>(sceneRegistry.GetCachedSceneData<Data::Flags>());

		if (serializer.IsValid())
		{
			ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform2D>();

			const ComponentIdentifier identifier = GetIdentifier();
			Data::LocalTransform2D& __restrict relativeTransform = localTransformSceneData.GetComponentImplementationUnchecked(identifier);
			const Math::Transform2Df previousLocalTransform = relativeTransform;
			Math::Transform2Df newTransform = previousLocalTransform;
			if (serializer->HasSerializer("position") || serializer->HasSerializer("rotation") || serializer->HasSerializer("scale"))
			{
				serializer.Get().SerializeInPlace(newTransform);
			}
			if (!newTransform.IsEquivalentTo(previousLocalTransform))
			{
				if (flags.IsSet(ComponentFlags::IsRootScene))
					;
				if (flags.AreAnySet(ComponentFlags::IsDetachedFromTreeFromAnySource))
				{
					relativeTransform = newTransform;

					ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData =
						sceneRegistry.GetCachedSceneData<Data::WorldTransform2D>();
					ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

					Data::WorldTransform2D& worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(identifier);

					const Entity::ComponentIdentifier parentIdentifier =
						Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
					const Math::WorldTransform2D parentWorldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

					worldTransform = parentWorldTransform.Transform(newTransform);
					// OnWorldTransformChanged(EnumFlags<TransformChangeFlags>{});
				}
				else
				{
					SetRelativeTransform(newTransform, sceneRegistry);
				}
			}
		}

		BaseType::DeserializeCustomData(serializer);
	}

	bool Component2D::SerializeCustomData(Serialization::Writer serializer) const
	{
		return serializer.SerializeInPlace(GetRelativeTransform(GetSceneRegistry())) | BaseType::SerializeCustomData(serializer);
	}

	bool Component2D::CanApplyAtPoint(const ApplicableData&, const Math::WorldCoordinate2D, const EnumFlags<ApplyAssetFlags>) const
	{
		return false;
	}

	bool Component2D::ApplyAtPoint(const ApplicableData&, const Math::WorldCoordinate2D, const EnumFlags<ApplyAssetFlags>)
	{
		return false;
	}

	template struct HierarchyComponent<Component2D>;

	[[maybe_unused]] const bool wasComponent2DTypeRegistered = Reflection::Registry::RegisterType<Component2D>();
	[[maybe_unused]] const bool wasComponent2DRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<Data::Component2D>>::Make());

	[[maybe_unused]] const bool wasQuadtreeNodeTypeRegistered = Reflection::Registry::RegisterType<Data::QuadtreeNode>();
	[[maybe_unused]] const bool wasQuadtreeNodeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<Data::QuadtreeNode>>::Make());

	[[maybe_unused]] const bool wasWorldTransform2DTypeRegistered = Reflection::Registry::RegisterType<Data::WorldTransform2D>();
	[[maybe_unused]] const bool wasWorldTransform2DRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<Data::WorldTransform2D>>::Make());
	[[maybe_unused]] const bool wasLocalTransform2DTypeRegistered = Reflection::Registry::RegisterType<Data::LocalTransform2D>();
	[[maybe_unused]] const bool wasLocalTransform2DRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<Data::LocalTransform2D>>::Make());
}
