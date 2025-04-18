#pragma once

#include <Animation/Components/Controllers/AnimationController.h>
#include <Animation/AnimationIdentifier.h>
#include <Animation/SamplingCache.h>
#include <Common/Asset/Picker.h>
#include <Common/Storage/Identifier.h>

namespace ngine
{
	namespace Asset
	{
		struct Guid;
	}
}

namespace ngine::Animation
{
	struct Animation;

	struct LoopingAnimationController final : public Controller
	{
		using BaseType = Controller;
		using InstanceIdentifier = TIdentifier<uint32, 10>;

		struct Initializer : public Controller::Initializer
		{
			using BaseType = Controller::Initializer;
			using BaseType::BaseType;

			Initializer(BaseType&& initializer, const AnimationIdentifier animationIdentifier = {})
				: BaseType(Forward<Controller::Initializer>(initializer))
				, m_animationIdentifier(animationIdentifier)
			{
			}

			AnimationIdentifier m_animationIdentifier;
		};

		LoopingAnimationController(const LoopingAnimationController& templateComponent, const Cloner& cloner);
		LoopingAnimationController(Initializer&& initializer);
		LoopingAnimationController(const Deserializer& __restrict deserializer);

		virtual ~LoopingAnimationController();

		using AnimationAssetPicker = Asset::Picker;
		void SetAnimationAsset(const AnimationAssetPicker asset);
		void SetAnimation(const AnimationIdentifier identifier);
		AnimationAssetPicker GetAnimation() const;
	protected:
		virtual bool ShouldUpdate() const override;

		virtual void Update() override;

		virtual void OnSkeletonChanged() override;

		virtual void ApplyAnimation(const Asset::Guid assetGuid) override;
		virtual void IterateAnimations(const Function<Memory::CallbackResult(ConstAnyView), 36>&) override;
	protected:
		AnimationIdentifier m_animationIdentifier;
		Animation* m_pAnimation = nullptr;
		float m_timeRatio = 0.f;
		SamplingCache m_samplingCache;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Animation::LoopingAnimationController>
	{
		inline static constexpr auto Type = Reflection::Reflect<Animation::LoopingAnimationController>(
			"{ADA8DE9B-70B3-4E23-8CC8-60FEBAC91165}"_guid,
			MAKE_UNICODE_LITERAL("Looping Animation Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Animation"),
				"animation",
				"{98D6027D-D325-4310-BF75-514E1E1EE8F3}"_guid,
				MAKE_UNICODE_LITERAL("Animation"),
				&Animation::LoopingAnimationController::SetAnimationAsset,
				&Animation::LoopingAnimationController::GetAnimation,
				(Reflection::Internal::DummyFunction) nullptr,
				Reflection::PropertyFlags::VisibleToParentScope
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "2d002e95-6d4f-48be-9c62-8cea8f1e7ca9"_asset, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_guid
			}}
		);
	};
}
