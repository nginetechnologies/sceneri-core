#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Engine/Scripting/ScriptIdentifier.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Asset/Picker.h>
#include <Common/Reflection/PropertyBinder.h>
#include <Common/Reflection/Type.h>

#define SCRIPTING_COMPONENT_USE_INTERPRETER 0

#include <Engine/Scripting/Interpreter/Environment.h>
#include <Engine/Scripting/Compiler/Object.h>

namespace ngine::Input
{
	struct ActionMonitor;
}

namespace ngine::Entity
{
	struct HierarchyComponentBase;
}

namespace ngine::Scripting
{
	struct ScriptFunction;
	struct Interpreter;

	struct ClosureObject;
	struct VirtualMachine;

	namespace AST::Expression
	{
		struct Function;
	}
}

namespace ngine::Entity::Data
{
	struct Logic final : public Entity::Data::HierarchyComponent
	{
		inline static constexpr Guid BeginEntryPointTypeGuid = "84242efc-1b31-4756-8a9b-bd5dba34882e"_guid;
		inline static constexpr Guid TickEntryPointTypeGuid = "586f2399-0ca7-4f1c-b5d1-8be3b9285d96"_guid;

		using BaseType = Entity::Data::HierarchyComponent;
		struct Initializer : public DynamicInitializer
		{
			using DynamicInitializer::DynamicInitializer;

			Initializer(DynamicInitializer&& baseInitializer, const Scripting::ScriptIdentifier scriptIdentifier)
				: DynamicInitializer(Forward<DynamicInitializer>(baseInitializer))
				, m_scriptIdentifier(scriptIdentifier)
			{
			}

			Scripting::ScriptIdentifier m_scriptIdentifier;
		};

		Logic(const Logic& templateComponent, const Cloner& cloner);
		Logic(const Deserializer& deserializer);
		Logic(Initializer&& initializer);
		~Logic();

		void OnCreated(Entity::HierarchyComponentBase& parent);
		void OnDestroying(Entity::HierarchyComponentBase& owner);
		void OnEnable(Entity::HierarchyComponentBase& owner);
		void OnDisable(Entity::HierarchyComponentBase& owner);
		void OnSimulationResumed(Entity::HierarchyComponentBase& owner);
		void OnSimulationPaused(Entity::HierarchyComponentBase& owner);

		void Update();

		[[nodiscard]] Scripting::ScriptIdentifier GetScriptIdentifier() const
		{
			return m_scriptIdentifier;
		}
		void SetScript(Entity::HierarchyComponentBase& owner, const Scripting::ScriptIdentifier scriptIdentifier);
		void OnScriptChanged(Entity::HierarchyComponentBase& owner);
	protected:
		void SetLogicAsset(const Asset::Picker asset);
		Asset::Picker GetLogicAsset() const;
		Threading::JobBatch SetDeserializedLogicAsset(
			const Asset::Picker asset,
			[[maybe_unused]] const Serialization::Reader objectReader,
			[[maybe_unused]] const Serialization::Reader typeReader
		);

		[[nodiscard]] Threading::JobBatch LoadScriptInternal(Entity::HierarchyComponentBase& owner);
		[[nodiscard]] Optional<Threading::Job*> ExecuteScriptInternal(Entity::HierarchyComponentBase& owner);

		void ScriptPropertiesChanged();

		void RegisterForUpdate(Entity::HierarchyComponentBase& owner);
		void DeregisterUpdate(Entity::HierarchyComponentBase& owner);
	private:
		void ExecuteBegin(Entity::HierarchyComponentBase& owner);

		friend struct Reflection::ReflectedType<Logic>;
	private:
		Entity::HierarchyComponentBase& m_owner;
		Scripting::ScriptIdentifier m_scriptIdentifier;

		struct InterpreterData
		{
			SharedPtr<Scripting::Environment> m_pEnvironment;
			UniquePtr<Scripting::Interpreter> m_pInterpreter;

			Optional<const Scripting::AST::Expression::Function*> m_pBeginFunctionExpression;
			Optional<const Scripting::AST::Expression::Function*> m_pTickFunctionExpression;
		};
		struct VirtualMachineData
		{
			UniquePtr<Scripting::VirtualMachine> m_pVirtualMachine;

			Optional<const Scripting::FunctionObject*> m_pBeginFunctionObject;
			Optional<const Scripting::FunctionObject*> m_pTickFunctionObject;

			Threading::Mutex m_eventFunctionObjectsMutex;
			UnorderedMap<Guid, ReferenceWrapper<const Scripting::FunctionObject>, Guid::Hash> m_eventFunctionObjects;
		};

		union
		{
			InterpreterData m_interpreterData;
			VirtualMachineData m_virtualMachineData;
		};
		Reflection::PropertyBinder m_scriptProperties;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::Logic>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::Data::Logic>(
			"b4924268-895a-49b7-b1e9-e1b3969394d1"_guid,
			MAKE_UNICODE_LITERAL("Logic"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Asset"),
					"asset",
					"{AAE75E61-837C-4984-873A-78BF1ED10F51}"_guid,
					MAKE_UNICODE_LITERAL("Logic"),
					&Entity::Data::Logic::SetLogicAsset,
					&Entity::Data::Logic::GetLogicAsset,
					&Entity::Data::Logic::SetDeserializedLogicAsset
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Properties"),
					"properties",
					"{01437FDF-A570-44EA-AF3C-0EB37D284E82}"_guid,
					MAKE_UNICODE_LITERAL("Logic"),
					&Entity::Data::Logic::m_scriptProperties,
					&Entity::Data::Logic::ScriptPropertiesChanged
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "bc0ced52-0a3f-d6d4-fe89-c2d733476f3d"_asset, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_guid
				},
				Entity::IndicatorTypeExtension{EnumFlags<Entity::IndicatorTypeExtension::Flags>{Entity::IndicatorTypeExtension::Flags::RequiresGhost
		    }}
			}
		);
	};
}
