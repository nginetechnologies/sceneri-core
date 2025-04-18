#pragma once

#include "Component3D.h"
#include "RenderItemIdentifier.h"

#include <Common/ForwardDeclarations/EnumFlags.h>
#include <Common/Storage/ForwardDeclarations/AtomicIdentifierMask.h>

#include <Renderer/Assets/Stage/SceneRenderStageIdentifier.h>
#include <Renderer/Stages/RenderItemStageMask.h>
#include <Renderer/Stages/RenderItemStagesProperty.h>

namespace ngine
{
	struct Scene3D;
	struct SceneOctreeNode;
	struct RootSceneOctreeNode;
};

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct RenderViewThreadJob;
	struct Window;

	// Mask indicating which rendering stages a render item will be used in
	using AtomicRenderItemStageMask = Threading::AtomicIdentifierMask<SceneRenderStageIdentifier>;
}

namespace ngine::Entity
{
	using AtomicRenderItemMask = Threading::AtomicIdentifierMask<RenderItemIdentifier>;

	struct RenderItemComponent : public Component3D
	{
		using BaseType = Component3D;

		struct Initializer : public Component3D::Initializer
		{
			using BaseType = Component3D::Initializer;
			using BaseType::BaseType;
			Initializer(BaseType&& initializer, const Rendering::RenderItemStageMask stageMask = {})
				: BaseType(Forward<BaseType>(initializer))
				, m_stageMask(stageMask)
			{
			}

			[[nodiscard]] Initializer operator|(const EnumFlags<Flags> flags) const
			{
				Initializer initializer = *this;
				initializer.m_flags |= flags;
				return initializer;
			}

			[[nodiscard]] Initializer operator|(const Math::BoundingBox boundingBox) const
			{
				Initializer initializer = *this;
				initializer.m_localBoundingBox = boundingBox;
				return initializer;
			}

			Rendering::RenderItemStageMask m_stageMask;
		};

		RenderItemComponent(const RenderItemComponent& templateComponent, const Cloner& cloner);
		RenderItemComponent(const Deserializer& deserializer);
		RenderItemComponent(Initializer&& initializer);
		virtual ~RenderItemComponent();

		[[nodiscard]] PURE_STATICS RenderItemIdentifier GetRenderItemIdentifier() const;
		[[nodiscard]] PURE_STATICS const Rendering::RenderItemStageMask& GetStageMask() const;

		void OnCreated();

		void EnableStages(const Rendering::RenderItemStageMask& newMask);
		void DisableStages(const Rendering::RenderItemStageMask& newMask);
		void ResetStages(const Rendering::RenderItemStageMask& resetStages);
		void EnableStage(const Rendering::SceneRenderStageIdentifier stageIdentifier);
		void DisableStage(const Rendering::SceneRenderStageIdentifier stageIdentifier);
		void ResetStage(const Rendering::SceneRenderStageIdentifier stageIdentifier);
		[[nodiscard]] bool IsStageEnabled(const Rendering::SceneRenderStageIdentifier stageIdentifier) const;
		[[nodiscard]] bool IsStageDisabled(const Rendering::SceneRenderStageIdentifier stageIdentifier) const
		{
			return !IsStageEnabled(stageIdentifier);
		}

		void OnEnable();
		void OnDisable();
	protected:
		RenderItemComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer);

		// Component3D
		virtual void OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags) override;
		// ~Component3D

		[[nodiscard]] Rendering::RenderItemStagesProperty GetRenderStages() const;
		void SetRenderStages(Rendering::RenderItemStagesProperty stages);
	protected:
		friend struct Reflection::ReflectedType<Entity::RenderItemComponent>;
		friend Rendering::RenderViewThreadJob;
		friend Rendering::Window;
		friend Scene3D;
		friend RootSceneOctreeNode;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::RenderItemComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::RenderItemComponent>(
			"9f750482-b822-4d4b-be6a-9c135c614328"_guid,
			MAKE_UNICODE_LITERAL("Render Item"),
			Reflection::TypeFlags::IsAbstract,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Render Stages"),
				"stages",
				"{A66ECCD3-56C4-4881-81DD-4749FC8DF79A}"_guid,
				MAKE_UNICODE_LITERAL("Render Stages"),
				PropertyFlags::VisibleToParentScope | PropertyFlags::HideFromUI,
				&Entity::RenderItemComponent::SetRenderStages,
				&Entity::RenderItemComponent::GetRenderStages
			)}
		);
	};
}
