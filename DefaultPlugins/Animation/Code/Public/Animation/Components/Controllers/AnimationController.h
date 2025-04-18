#pragma once

#include <Engine/Entity/Data/Component3D.h>

#include <Common/Asset/Guid.h>

#include <Common/Reflection/Type.h>
#include <Common/Memory/CallbackResult.h>

namespace ngine::Animation
{
	struct SkeletonComponent;

	struct Controller : public Entity::Data::Component3D
	{
		using InstanceIdentifier = TIdentifier<uint32, 9>;
		using BaseType = Entity::Data::Component3D;

		using Initializer = Entity::Data::Component3D::DynamicInitializer;

		Controller(Initializer&& initializer);
		Controller(const Controller&, const Cloner& cloner);
		Controller(const Deserializer& deserializer);
		virtual ~Controller();

		[[nodiscard]] virtual bool ShouldUpdate() const
		{
			return true;
		}
		virtual void Update() = 0;
		virtual void OnSkeletonChanged()
		{
		}
		virtual void ApplyAnimation(const Asset::Guid assetGuid) = 0;
		virtual void IterateAnimations(const Function<Memory::CallbackResult(ConstAnyView), 36>&) = 0;
	protected:
		SkeletonComponent& m_skeletonComponent;
	};
}

// TODO: Reflection should expose functionality for retrieving types that implement a class
// Then we can retrieve all controllers in UI easily.
namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Animation::Controller>
	{
		inline static constexpr auto Type = Reflection::Reflect<Animation::Controller>(
			"{79F28CC4-210F-4391-9189-F96A53CAC279}"_guid, MAKE_UNICODE_LITERAL("Animation Controller")
		);
	};
}
