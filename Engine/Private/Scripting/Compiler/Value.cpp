#include "Engine/Scripting/Compiler/Value.h"
#include "Engine/Scripting/Compiler/Object.h"
#include "Engine/Scripting/Parser/Token.h"

#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/StringView.h>

#include <Common/Math/Vector4/SignNonZero.h>

#include <Common/Memory/Containers/Format/String.h>
#include <Common/Reflection/Registry.h>
#include <Common/System/Query.h>
#include <Common/Format/Guid.h>

namespace ngine::Scripting
{
	[[nodiscard]] StringType ValueToString(Value value)
	{
		StringType valueString;
		if (IsNull(value))
		{
			valueString += SCRIPT_STRING_LITERAL("nil");
		}
		else if (IsBoolean(value))
		{
			valueString += StringType(
				value.GetBool() ? StringType::ConstView(SCRIPT_STRING_LITERAL("true")) : StringType::ConstView(SCRIPT_STRING_LITERAL("false"))
			);
		}
		else if (IsInteger(value))
		{
			valueString.Format("{}", value.GetInteger());
		}
		else if (IsDecimal(value))
		{
			valueString.Format("{}", value.GetDecimal());
		}
		else if (IsObject(value))
		{
			if (IsStringObject(value))
			{
				valueString = AsStringObject(value)->string;
			}
			else if (IsFunctionObject(value))
			{
				if (AsFunctionObject(value)->id == FunctionObjectScriptId)
				{
					valueString += SCRIPT_STRING_LITERAL("script");
				}
				else
				{
					StringType functionString;
					functionString.Format("fn: {}", AsFunctionObject(value)->id);
					valueString += functionString;
				}
			}
			else if (IsUpvalueObject(value))
			{
				valueString += SCRIPT_STRING_LITERAL("upvalue");
			}
			else if (IsClosureObject(value))
			{
				if (AsClosureObject(value)->pFunction->id == FunctionObjectScriptId)
				{
					valueString += SCRIPT_STRING_LITERAL("script");
				}
				else
				{
					StringType functionString;
					functionString.Format("fn: {}", AsClosureObject(value)->pFunction->id);
					valueString += functionString;
				}
			}
		}
		else if (IsGuid(value))
		{
			StringType nativeString;
			nativeString.Format("guid: {}", value.GetGuid());
			valueString += nativeString;
		}
		return valueString;
	}

	bool ValueEquals(Value left, Value right)
	{
		const ValueType typeLeft = GetValueType(left);
		if (typeLeft == GetValueType(right))
		{
			switch (typeLeft)
			{
				case ValueType::Unknown:
					ExpectUnreachable();
				case ValueType::Null:
					return true;
				case ValueType::Boolean:
					return left.GetBool() == right.GetBool();
				case ValueType::Boolean4:
					return bool{left.GetBool4() == right.GetBool4()};
				case ValueType::Integer:
					return left.GetInteger() == right.GetInteger() && Math::SignNonZero(left.GetInteger()) == Math::SignNonZero(right.GetInteger());
				case ValueType::Integer4:
					return bool{left.GetVector4i() == right.GetVector4i()} && Math::SignNonZero(left.GetVector4i()) == Math::SignNonZero(right.GetVector4i());
				case ValueType::Decimal:
                    return left.GetDecimal() == right.GetDecimal() && Math::SignNonZero(left.GetDecimal()) == Math::SignNonZero(right.GetDecimal());
				case ValueType::Decimal4:
					return bool{left.GetVector4f() == right.GetVector4f()} && Math::SignNonZero(left.GetVector4f()) == Math::SignNonZero(right.GetVector4f());
				case ValueType::Object:
				{
					Object* pLeft = left.GetObject();
					Object* pRight = right.GetObject();
					const ObjectType objectTypeLeft = pLeft->type;
					if (objectTypeLeft == pRight->type)
					{
						switch (objectTypeLeft)
						{
							case ObjectType::String:
								return AsStringObject(pLeft) == AsStringObject(pRight) ||
								       AsStringObject(pLeft)->string.operator StringType::ConstView().EqualsCaseSensitive(
												 AsStringObject(pRight)->string.operator StringType::ConstView()
											 );
							case ObjectType::Function:
							case ObjectType::Closure:
								return pLeft == pRight;
							case ObjectType::Upvalue:
								return AsUpvalueObject(pLeft)->pLocation == AsUpvalueObject(pRight)->pLocation;
						}
						ExpectUnreachable();
					}
					else
					{
						return false;
					}
				}
				case ValueType::Guid:
				case ValueType::NativeFunctionGuid:
				case ValueType::TagGuid:
				case ValueType::RenderStageGuid:
				case ValueType::AssetGuid:
					return left.GetGuid() == right.GetGuid();
				case ValueType::ComponentSoftReference:
					return left.GetComponentSoftReference() == right.GetComponentSoftReference();
				case ValueType::NativeFunctionPointer:
				case ValueType::TagIdentifier:
				case ValueType::AssetIdentifier:
				case ValueType::RenderStageIdentifier:
				case ValueType::ComponentPointer:
					ExpectUnreachable();
			}
		}
		return false;
	}
}
