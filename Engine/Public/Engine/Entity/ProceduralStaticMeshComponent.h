#pragma once

#include <Engine/Entity/StaticMeshComponent.h>

#include <Renderer/Index.h>
#include <Renderer/Assets/StaticMesh/StaticObject.h>

#include <Common/Threading/Mutexes/Mutex.h>

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Entity
{
	struct ProceduralStaticMeshComponent : public StaticMeshComponent
	{
		using BaseType = StaticMeshComponent;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using SegmentCountType = uint16;
		using SideSegmentCountType = uint16;

		using GenerateMeshFunction = Function<void(Rendering::StaticObject& staticObject), 24>;

		struct Initializer : public RenderItemComponent::Initializer
		{
			using BaseType = RenderItemComponent::Initializer;
			using BaseType::BaseType;
			Initializer(BaseType&& initializer, Rendering::MaterialInstanceIdentifier materialInstanceIdentifier)
				: BaseType(Forward<BaseType>(initializer))
				, m_materialInstanceIdentifier(materialInstanceIdentifier)
			{
			}

			Rendering::MaterialInstanceIdentifier m_materialInstanceIdentifier;
		};

		ProceduralStaticMeshComponent(const ProceduralStaticMeshComponent& templateComponent, const Cloner& cloner);
		ProceduralStaticMeshComponent(const Deserializer& deserializer);
		ProceduralStaticMeshComponent(Initializer&& initializer);
		virtual ~ProceduralStaticMeshComponent();

		void SetMeshGeneration(GenerateMeshFunction&& mesh);

		void RecreateMesh();
	protected:
		friend struct Reflection::ReflectedType<Entity::ProceduralStaticMeshComponent>;
		[[nodiscard]] Rendering::StaticMeshIdentifier CreateMesh();
		[[nodiscard]] Threading::JobBatch GenerateMesh();
	private:
		ProceduralStaticMeshComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
	private:
		inline static constexpr Rendering::Index PrecachedVertexCount = 20000;
		inline static constexpr Rendering::Index PrecachedIndexCount = 20000;

		GenerateMeshFunction m_generateMeshCallback;
		Threading::Mutex m_meshGenerationMutex;
		Array<Rendering::StaticObject, 2> m_staticObjectCache{
			Rendering::StaticObject{Memory::Reserve, PrecachedVertexCount, PrecachedIndexCount},
			Rendering::StaticObject{Memory::Reserve, PrecachedVertexCount, PrecachedIndexCount}
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::ProceduralStaticMeshComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::ProceduralStaticMeshComponent>(
			"4e51098f-bd53-48bb-97b6-63de23a591fc"_guid, MAKE_UNICODE_LITERAL("Procedural Static Mesh")
		);
	};
}
