#include <Engine/Entity/ComponentSoftReference.h>
#include <Common/Asset/Guid.h>
#include <Common/Tag/TagGuid.h>
#include <Renderer/Assets/Stage/SceneRenderStageGuid.h>

#include <Common/Reflection/Type.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::ComponentSoftReference>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentSoftReference>(
			"595f5ca2-4ae3-4e98-bdaf-45fa114ad142"_guid, MAKE_UNICODE_LITERAL("Component Soft Reference")
		);
	};
	[[maybe_unused]] const bool wasComponentSoftReferenceTypeRegistered = Reflection::Registry::RegisterType<Entity::ComponentSoftReference>(
	);

	template<>
	struct ReflectedType<Asset::Guid>
	{
		static constexpr auto Type =
			Reflection::Reflect<Asset::Guid>("98c9559e-7a11-4000-8376-61c4038a774e"_guid, MAKE_UNICODE_LITERAL("Asset Guid"));
	};
	[[maybe_unused]] const bool wasAssetGuidTypeRegistered = Reflection::Registry::RegisterType<Asset::Guid>();

	template<>
	struct ReflectedType<Tag::Guid>
	{
		static constexpr auto Type =
			Reflection::Reflect<Tag::Guid>("d1e3abc8-bafe-4087-a482-ecae3fd37aeb"_guid, MAKE_UNICODE_LITERAL("Tag Guid"));
	};
	[[maybe_unused]] const bool wasTagGuidTypeRegistered = Reflection::Registry::RegisterType<Tag::Guid>();

	template<>
	struct ReflectedType<Rendering::StageGuid>
	{
		static constexpr auto Type =
			Reflection::Reflect<Rendering::StageGuid>("a05f7920-60bf-4af8-9814-19199d018f1d"_guid, MAKE_UNICODE_LITERAL("Stage Guid"));
	};
	[[maybe_unused]] const bool wasStageGuidTypeRegistered = Reflection::Registry::RegisterType<Rendering::StageGuid>();
}
