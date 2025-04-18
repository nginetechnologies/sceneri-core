#pragma once

#include "SceneBase.h"

#include <Renderer/Scene/SceneData.h>
#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Entity/Scene/ComponentTemplateIdentifier.h>
#include <Common/Asset/Guid.h>
#include <Common/Memory/Containers/ForwardDeclarations/StringView.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Math/ForwardDeclarations/Radius.h>
#include <Common/Math/ForwardDeclarations/WorldCoordinate.h>
#include <Common/Reflection/Type.h>

namespace ngine
{
	namespace Math
	{
		template<typename T>
		struct TVector2;

		using Vector2i = TVector2<int>;
		using Vector2ui = TVector2<uint32>;
	}

	namespace Rendering
	{
		struct SceneView;
		struct Renderer;
		struct RenderViewThreadJob;
	}

	namespace Asset
	{
		struct MeshCache;
		struct TextureCache;
	}

	namespace Threading
	{
		struct Job;
		struct JobBatch;
	}

	namespace Entity
	{
		struct SceneComponent;
		struct RootSceneComponent;
	}

	struct Scene3D final : public SceneBase
	{
		enum class Version : uint8
		{
			None,
			//! Version where the directional gravity component was introduced
			//! Lower versions will spawn one automatically for backwards compatibility
			DirectionalGravityComponent,
			Latest = DirectionalGravityComponent
		};

		Scene3D(
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Entity::HierarchyComponentBase*> pRootParent,
			const Math::Radius<Math::WorldCoordinateUnitType> radius,
			const Asset::Guid assetGuid,
			const EnumFlags<Flags> flags = {},
			const uint16 maximumUpdateRate = Math::NumericLimits<uint16>::Max
		);
		Scene3D(
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Entity::HierarchyComponentBase*> pRootParent,
			const Serialization::Reader reader,
			const Asset::Guid assetGuid,
			Threading::JobBatch& jobBatchOut,
			const EnumFlags<Flags> flags = {},
			const uint16 maximumUpdateRate = Math::NumericLimits<uint16>::Max
		);
		Scene3D(
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Entity::HierarchyComponentBase*> pRootParent,
			const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier,
			Threading::JobBatch& jobBatchOut,
			const EnumFlags<Flags> flags = {},
			const uint16 maximumUpdateRate = Math::NumericLimits<uint16>::Max
		);
		Scene3D(const Scene3D& other) = delete;
		Scene3D& operator=(const Scene3D&) = delete;
		Scene3D(Scene3D&&) = delete;
		Scene3D& operator=(Scene3D&&) = delete;
		~Scene3D();

		// SceneBase
		virtual void ProcessDestroyedComponentsQueueInternal(const ArrayView<ReferenceWrapper<Entity::HierarchyComponentBase>> components
		) override final;
		// ~SceneBase

		[[nodiscard]] PURE_LOCALS_AND_POINTERS Rendering::SceneData& GetRenderData()
		{
			return m_renderData;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const Rendering::SceneData& GetRenderData() const
		{
			return m_renderData;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS Entity::RootSceneComponent& GetRootComponent() const;

		void Remix(const Asset::Guid newGuid);

		bool Serialize(Serialization::Writer serializer) const;

		[[nodiscard]] PURE_LOCALS_AND_POINTERS uint16 GetMaximumUpdateRate() const
		{
			return m_maximumUpdateRate;
		}
		void SetMaximumUpdateRate(const uint16 maximumUpdateRate)
		{
			m_maximumUpdateRate = maximumUpdateRate;
		}
	protected:
		void OnEnabledInternal();
		void OnDisabledInternal();
	protected:
		//! Maximum rate at which the scene can update
		uint16 m_maximumUpdateRate{Math::NumericLimits<uint16>::Max};

		friend Rendering::SceneData;
		Rendering::SceneData m_renderData;
	};

	// TODO: Remove this and only refer to Scene3D
	using Scene = Scene3D;

	bool Serialize(Serialization::Reader serializer, Scene3D& scene);

	ENUM_FLAG_OPERATORS(Scene3D::Version);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Scene3D>
	{
		inline static constexpr auto Type = Reflection::Reflect<Scene3D>(
			"58ecb737-e57c-4242-ad18-59fbc491aab7"_guid,
			MAKE_UNICODE_LITERAL("3D Scene"),
			TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{Function{
				"b3f54530-2e45-4c86-a75b-db1a28c19ae1"_guid,
				MAKE_UNICODE_LITERAL("Get Root Component"),
				&Scene3D::GetRootComponent,
				FunctionFlags::VisibleToUI,
				Argument{"80ee73e0-918f-469f-9044-4bc8171c7ea0"_guid, MAKE_UNICODE_LITERAL("3D Root Component")}
			}}
		);
	};
}
