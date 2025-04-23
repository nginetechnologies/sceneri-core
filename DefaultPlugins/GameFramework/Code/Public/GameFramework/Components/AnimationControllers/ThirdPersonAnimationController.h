#pragma once

#include <Animation/Components/Controllers/AnimationController.h>
#include <Animation/Blendspace1D.h>

namespace ngine::GameFramework::Animation::Controller
{
	struct ThirdPerson : public ngine::Animation::Controller
	{
		using BaseType = ngine::Animation::Controller;

		enum class State : uint8
		{
			None,
			StartJump,
			InAir,
			Landing,
			OnGround
		};

		ThirdPerson(Initializer&& initializer);
		ThirdPerson(const ThirdPerson& templateComponent, const Cloner&);
		ThirdPerson(const Deserializer& deserializer);

		void SetMoveBlendRatio(const float ratio);
		[[nodiscard]] uint8 GetMoveBlendEntryCount() const
		{
			return m_moveBlendspace.GetEntryCount();
		}
		void SetTimeInAir(const Time::Durationf timeInAir);

		void DeserializeCustomData(const Optional<Serialization::Reader>, Entity::Component3D& parent);
		bool SerializeCustomData(Serialization::Writer, const Entity::Component3D& parent) const;
	protected:
		virtual bool ShouldUpdate() const override;
		virtual void Update() override;

		virtual void OnSkeletonChanged() override;

		virtual void ApplyAnimation(const Asset::Guid) override
		{
		}

		virtual void IterateAnimations(const Function<Memory::CallbackResult(ConstAnyView), 36>&) override
		{
		}
	protected:
		ngine::Animation::Blendspace1D m_moveBlendspace;
		ngine::Animation::Blendspace1D m_jumpBlendspace;
		// Positive if we're in air, negative if on ground
		Time::Durationf m_timeInAir = 0_seconds;
		State m_state = State::None;
		ngine::Animation::Blendspace1D m_moveJumpBlendspace;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Animation::Controller::ThirdPerson>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Animation::Controller::ThirdPerson>(
			"{E5E2C08B-0844-422A-9EF8-83629288CC54}"_guid, MAKE_UNICODE_LITERAL("Third Person Animation Controller")
		);
	};
}
