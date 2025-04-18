#include "Engine/Scripting/Interpreter/Environment.h"
#include "Engine/Scripting/Interpreter/Interpreter.h"

#include "Engine/Scripting/Interpreter/ScriptFunctionCache.h"
#include "Engine/Scripting/Interpreter/ScriptTableCache.h"
#include "Engine/Scripting/Interpreter/UserScriptFunction.h"
#include "Engine/Scripting/Parser/Token.h"
#include "Engine/Scripting/CoreFunctions.h"
#include "Common/Scripting/VirtualMachine/DynamicFunction/DynamicFunction.h"

#include <Common/Memory/Variant.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Reflection/GetType.h>
#include <Common/System/Query.h>

namespace ngine::Scripting
{
	Environment::Environment(ScriptFunctionCache& functions, ScriptTableCache& tables) noexcept
		: m_functions(functions)
		, m_tables(tables)
	{
	}

	Environment::Environment(SharedPtr<Environment> pEnclosing) noexcept
		: m_pEnclosing(Move(pEnclosing))
		, m_functions(m_pEnclosing->m_functions)
		, m_tables(m_pEnclosing->m_tables)
	{
	}

	Environment::Environment(Environment&& other) noexcept
		: m_pEnclosing(Move(other.m_pEnclosing))
		, m_functions(other.m_functions)
		, m_tables(other.m_tables)
		, m_values(Move(other.m_values))
	{
	}

	void Environment::SetValue(const Guid identifier, ScriptValue&& value)
	{
		auto valueIt = m_values.Find(identifier);
		if (valueIt != m_values.end())
		{
			valueIt->second = Forward<ScriptValue>(value);
		}
		else
		{
			m_values.Emplace(Guid{identifier}, Forward<ScriptValue>(value));
		}
	}

	void Environment::SetValueAt(int32 distance, const Guid identifier, ScriptValue&& value)
	{
		Environment* __restrict pEnvironment = this;
		while (distance-- && pEnvironment->m_pEnclosing.IsValid())
		{
			pEnvironment = &(*pEnvironment->m_pEnclosing);
		}
		pEnvironment->SetValue(identifier, Forward<ScriptValue>(value));
	}

	ScriptValue Environment::GetValue(const Guid identifier)
	{
		auto valueIt = m_values.Find(identifier);
		if (valueIt == m_values.end())
		{
			Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
			const FunctionIdentifier functionIdentifier = reflectionRegistry.FindFunctionIdentifier(identifier);
			if (functionIdentifier.IsValid())
			{
				return ScriptValue{functionIdentifier};
			}

			valueIt = m_values.Emplace(Guid{identifier}, ScriptValue(nullptr));
		}
		return valueIt->second;
	}

	ScriptValue Environment::GetValueAt(int32 distance, const Guid identifier)
	{
		Environment* __restrict pEnvironment = this;
		while (distance-- && pEnvironment->m_pEnclosing.IsValid())
		{
			pEnvironment = &(*pEnvironment->m_pEnclosing);
		}
		return pEnvironment->GetValue(identifier);
	}

	SharedPtr<Environment> Environment::Create(ScriptFunctionCache& functionCache, ScriptTableCache& tableCache)
	{
		SharedPtr<Environment> pEnvironment = SharedPtr<Environment>::Make(functionCache, tableCache);

		Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();

		pEnvironment->AddFunction(
			reflectionRegistry.FindFunctionIdentifier(Reflection::GetFunctionGuid<&Scripting::CoreFunctions::ReflectedAssert>()),
			SCRIPT_STRING_LITERAL("assert"),
			Token::GuidFromScriptString(SCRIPT_STRING_LITERAL("assert")),
			{}
		);

		if (Optional<ScriptTable*> pMathTable = pEnvironment->AddTable(SCRIPT_STRING_LITERAL("math"), Token::GuidFromScriptString(SCRIPT_STRING_LITERAL("math"))))
		{
			pMathTable->Set(ScriptValue(StringType(SCRIPT_STRING_LITERAL("pi"))), ScriptValue(Math::TConstants<FloatType>::PI));
		}
		return pEnvironment;
	}

	SharedPtr<Environment> Environment::GetGlobalEnvironment() const
	{
		SharedPtr<Environment> pGlobalEnvironment = m_pEnclosing;
		while (pGlobalEnvironment.IsValid() && pGlobalEnvironment->m_pEnclosing.IsValid())
		{
			pGlobalEnvironment = pGlobalEnvironment->m_pEnclosing;
		}
		return pGlobalEnvironment;
	}

	const Environment::ScriptValueMap& Environment::GetValues() const
	{
		return m_values;
	}

	Optional<ScriptFunction*> Environment::GetFunction(FunctionIdentifier identifier) const
	{
		return m_functions.GetFunction(identifier);
	}

	Optional<ScriptFunction*> Environment::GetFunction(StringType::ConstView name, const Guid identifier, Optional<ScriptTable*> pTable) const
	{
		if (pTable.IsValid())
		{
			ScriptValue functionValue = pTable->Get(ScriptValue(StringType(name)));
			if (functionValue.Get().Is<FunctionIdentifier>())
			{
				return GetFunction(functionValue.Get().GetExpected<FunctionIdentifier>());
			}
		}
		else
		{
			auto valueIt = m_values.Find(identifier);
			if (valueIt != m_values.end() && valueIt->second.Get().Is<FunctionIdentifier>())
			{
				return GetFunction(valueIt->second.Get().GetExpected<FunctionIdentifier>());
			}
		}
		return {};
	}

	Optional<ScriptTable*> Environment::GetTable(ScriptTableIdentifier identifier) const
	{
		return m_tables.GetTable(identifier);
	}

	Optional<ScriptTable*> Environment::GetTable(StringType::ConstView name, const Guid identifier, Optional<ScriptTable*> pTable) const
	{
		if (pTable.IsValid())
		{
			ScriptValue tableValue = pTable->Get(ScriptValue(StringType(name)));
			if (tableValue.Get().Is<ManagedScriptTableIdentifier>())
			{
				return GetTable(tableValue.Get().GetExpected<ManagedScriptTableIdentifier>());
			}
		}
		else
		{
			auto valueIt = m_values.Find(identifier);
			if (valueIt != m_values.end() && valueIt->second.Get().Is<ManagedScriptTableIdentifier>())
			{
				return GetTable(valueIt->second.Get().GetExpected<ManagedScriptTableIdentifier>());
			}
		}
		return {};
	}

	void Environment::AddFunction(const FunctionIdentifier identifier, UniquePtr<ScriptFunction>&& pScriptFunction)
	{
		m_functions.AddFunction(identifier, Forward<UniquePtr<ScriptFunction>>(pScriptFunction));
	}

	ManagedScriptTableIdentifier Environment::AddTable(UniquePtr<ScriptTable>&& pScriptTable)
	{
		ScriptTableIdentifier identifier = m_tables.AddTable(Forward<UniquePtr<ScriptTable>>(pScriptTable));
		return ManagedScriptTableIdentifier(identifier, m_tables);
	}

	void Environment::AddFunction(
		const FunctionIdentifier identifier,
		StringType::ConstView name,
		const Guid guid,
		UniquePtr<ScriptFunction>&& pScriptFunction,
		Optional<ScriptTable*> pTable
	)
	{
		if (pScriptFunction.IsValid())
		{
			AddFunction(identifier, Forward<UniquePtr<ScriptFunction>>(pScriptFunction));
		}
		if (pTable.IsValid())
		{
			pTable->Set(ScriptValue(StringType(name)), ScriptValue(identifier));
		}
		else
		{
			// Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
			SetValue(guid /*reflectionRegistry.FindFunctionGuid(identifier)*/, ScriptValue(identifier));
		}
	}

	Optional<ScriptTable*> Environment::AddTable(StringType::ConstView name, const Guid identifier, Optional<ScriptTable*> pTable)
	{
		ManagedScriptTableIdentifier managedTableIdentifier = AddTable(UniquePtr<ScriptTable>::Make());
		const ScriptTableIdentifier tableIdentifier = managedTableIdentifier.operator ngine::Scripting::ScriptTableIdentifier();
		if (pTable.IsValid())
		{
			pTable->Set(ScriptValue(StringType(name)), ScriptValue(Move(managedTableIdentifier)));
		}
		else
		{
			SetValue(identifier, ScriptValue(Move(managedTableIdentifier)));
		}
		return m_tables.GetTable(tableIdentifier);
	}
}
