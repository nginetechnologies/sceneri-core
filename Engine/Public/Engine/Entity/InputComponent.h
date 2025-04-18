#pragma once

#include "Engine/Input/ActionHandle.h"
#include <Engine/Entity/Data/HierarchyComponent.h>

#include <Common/Asset/Picker.h>
#include <Common/Function/Event.h>
#include <Common/Function/Function.h>
#include <Common/Function/ThreadSafeEvent.h>

#include <Engine/Input/ActionMap.h>

namespace ngine::Input
{
	struct ActionMonitor;
}

namespace ngine::Entity
{
	using InputComponentEvent = ThreadSafe::Event<void(void*, Input::ActionMonitor&), 24, false>;

	struct InputComponent : public Data::HierarchyComponent
	{
	public:
		struct Initializer
		{
			Initializer(const Asset::Guid actionMapAsset)
				: m_actionMapAsset(actionMapAsset)
			{
			}
			Initializer(DynamicInitializer&&)
			{
			}

			Asset::Guid m_actionMapAsset;
		};

		using BaseType = Data::HierarchyComponent;
		using InstanceIdentifier = TIdentifier<uint32, 4>;
		using BaseType::BaseType;

		InputComponent(const InputComponent& templateComponent, const Cloner& cloner);
		InputComponent(const Deserializer& deserializer);
		InputComponent(Initializer&& initializer);

		InputComponent(const InputComponent&) = delete;
		InputComponent& operator=(const InputComponent&) = delete;

		void OnDestroying();
		void OnEnable();
		void OnDisable();

		InputComponentEvent OnAssignedActionMonitor;
		InputComponentEvent OnInputEnabled;
		InputComponentEvent OnInputDisabled;

		void UnassignMonitor();
		void AssignMonitor(Input::ActionMonitor& actionMonitor);
		[[nodiscard]] Optional<Input::ActionMonitor*> GetMonitor() const
		{
			return m_pActionMonitor;
		}

		Optional<Input::ActionMap*> GetActionMap() const;

		void SetActionMapAsset(const Asset::Picker asset);
		[[nodiscard]] Asset::Picker GetActionMapAsset() const;
	protected:
		void NotifyAssignMonitor();
		void NotifyEnable();
		void NotifyDisable();
	private:
		UniquePtr<Input::ActionMap> m_pActionMap;
		Optional<Input::ActionMonitor*> m_pActionMonitor;
		Asset::Guid m_actionMapAsset;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::InputComponent>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::InputComponent>(
			"be33f822-02f0-4d8b-9553-8f6200b43f83"_guid,
			MAKE_UNICODE_LITERAL("Input"),
			TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Action Map"),
				"actionmap",
				"{DCA59DB3-E296-4B78-B96D-622187F4FF11}"_guid,
				MAKE_UNICODE_LITERAL("Input"),
				&Entity::InputComponent::SetActionMapAsset,
				&Entity::InputComponent::GetActionMapAsset
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "45f77367-73b4-0984-573a-52cf388e0344"_asset, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_guid
			}}
		);
	};
}
