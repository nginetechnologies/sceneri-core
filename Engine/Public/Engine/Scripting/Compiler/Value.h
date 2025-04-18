#pragma once

#include "Engine/Scripting/Parser/StringType.h"

#include "Engine/Asset/Identifier.h"
#include "Engine/Tag/TagIdentifier.h"
#include "Engine/Entity/ComponentSoftReference.h"

#include <Renderer/Assets/Stage/SceneRenderStageIdentifier.h>
#include <Renderer/Assets/Stage/SceneRenderStageGuid.h>

#include <Common/Math/Vector2.h>
#include <Common/Math/Vector3.h>
#include <Common/Math/Vector4.h>
#include <Common/Math/Color.h>
#include <Common/Math/Angle3.h>
#include <Common/Math/Rotation2D.h>
#include <Common/Math/Quaternion.h>

#include <Common/Scripting/VirtualMachine/DynamicFunction/Register.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/DynamicInvoke.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/DynamicFunction.h>

namespace ngine::Entity
{
	struct HierarchyComponentBase;
}

namespace ngine::Scripting
{
	using Register = VM::Register;

	using IntegerType = int32;
	using FloatType = float;

	struct Object;

	enum class ValueType : uint8
	{
		Unknown,
		Null,
		Boolean,
		Boolean4,
		Integer,
		Integer4,
		Decimal,
		Decimal4,
		Object,
		Guid,
		NativeFunctionGuid,
		NativeFunctionPointer,
		TagGuid,
		TagIdentifier,
		AssetGuid,
		AssetIdentifier,
		ComponentSoftReference,
		ComponentPointer,
		RenderStageGuid,
		RenderStageIdentifier,
	};

	struct TRIVIAL_ABI RawValue
	{
#define SCRIPTING_RAW_VALUE_DEBUG 1

		RawValue() = default;
		explicit RawValue(const Register R0)
			: m_value(R0)
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Unknown)
#endif
		{
		}
		explicit RawValue(nullptr_type)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(nullptr)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Null)
#endif
		{
		}
		explicit RawValue(const bool value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Boolean)
#endif
		{
		}
		explicit RawValue(const IntegerType value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Integer)
#endif
		{
		}
		RawValue(const Math::Vector2i value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Integer4)
#endif
		{
		}
		RawValue(const Math::Vector3i value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Integer4)
#endif
		{
		}
		RawValue(const Math::Vector4i value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Integer4)
#endif
		{
		}
		RawValue(const Math::Vector4i::VectorizedType value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Integer4)
#endif
		{
		}
		RawValue(const Math::Vector4i::BoolType value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Boolean4)
#endif
		{
		}
		explicit RawValue(const FloatType value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Decimal)
#endif
		{
		}
		explicit RawValue(const Math::Anglef value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Decimal)
#endif
		{
		}
		explicit RawValue(const Math::Rotation2Df value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Decimal)
#endif
		{
		}
		RawValue(const Math::Vector2f value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Decimal4)
#endif
		{
		}
		RawValue(const Math::Vector3f value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Decimal4)
#endif
		{
		}
		RawValue(const Math::Vector4f value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Decimal4)
#endif
		{
		}
		RawValue(const Math::Vector4f::VectorizedType value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Decimal4)
#endif
		{
		}
		RawValue(const Math::Rotation3Df value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Decimal4)
#endif
		{
		}
		RawValue(const Math::Color value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Decimal4)
#endif
		{
		}
		RawValue(const Math::Vector4f::BoolType value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Boolean4)
#endif
		{
		}
		RawValue(const Math::Angle3f value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Decimal4)
#endif
		{
		}
		explicit RawValue(Object* const value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::Object)
#endif
		{
		}
		RawValue(const Guid value, [[maybe_unused]] const ValueType type = ValueType::Guid)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(type)
#endif
		{
		}
		explicit RawValue(const VM::DynamicFunction value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::NativeFunctionPointer)
#endif
		{
		}
		explicit RawValue(const Tag::Identifier value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::TagIdentifier)
#endif
		{
		}
		explicit RawValue(const Rendering::SceneRenderStageIdentifier value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::RenderStageIdentifier)
#endif
		{
		}
		explicit RawValue(const Entity::ComponentSoftReference value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::ComponentSoftReference)
#endif
		{
		}
		explicit RawValue(Entity::HierarchyComponentBase* pComponent)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(pComponent)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::ComponentPointer)
#endif
		{
		}
		explicit RawValue(const Asset::Identifier value)
			: m_value{VM::DynamicInvoke::LoadArgumentZeroed(value)}
#if SCRIPTING_RAW_VALUE_DEBUG
			, m_valueType(ValueType::AssetIdentifier)
#endif
		{
		}

		[[nodiscard]] operator Register() const
		{
			return m_value;
		}
		[[nodiscard]] FORCE_INLINE Object* GetObject() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Object);
#endif
			return VM::DynamicInvoke::ExtractArgument<Object*>(m_value);
		}
		[[nodiscard]] FORCE_INLINE bool GetBool() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Boolean || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<bool>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Vector2f::BoolType GetBool2f() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Boolean4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector2f::BoolType>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Vector3f::BoolType GetBool3f() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Boolean4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector3f::BoolType>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Vector2i::BoolType GetBool2i() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Boolean4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector2i::BoolType>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Vector3i::BoolType GetBool3i() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Boolean4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector3i::BoolType>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Vector4f::BoolType GetBool4f() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Boolean4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector4f::BoolType>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Vector4i::BoolType GetBool4i() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Boolean4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector4i::BoolType>(m_value);
		}
		[[nodiscard]] FORCE_INLINE bool AsBool() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(
				m_valueType == ValueType::Boolean || m_valueType == ValueType::Boolean4 || m_valueType == ValueType::Integer ||
				m_valueType == ValueType::Null || m_valueType == ValueType::Unknown
			);
#endif
			return VM::DynamicInvoke::ExtractArgument<IntegerType>(m_value) != 0;
		}
		[[nodiscard]] FORCE_INLINE Guid GetGuid() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(
				m_valueType == ValueType::Guid || m_valueType == ValueType::NativeFunctionGuid || m_valueType == ValueType::TagGuid ||
				m_valueType == ValueType::RenderStageGuid || m_valueType == ValueType::AssetGuid || m_valueType == ValueType::Unknown
			);
#endif
			return VM::DynamicInvoke::ExtractArgument<Guid>(m_value);
		}
		[[nodiscard]] FORCE_INLINE VM::DynamicFunction GetNativeFunctionPointer() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::NativeFunctionPointer || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Scripting::VM::DynamicFunction>(m_value);
		}
		[[nodiscard]] FORCE_INLINE IntegerType GetInteger() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Integer || m_valueType == ValueType::Integer4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<IntegerType>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Vector2i GetVector2i() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(
				m_valueType == ValueType::Integer4 || m_valueType == ValueType::Integer || m_valueType == ValueType::Boolean ||
				m_valueType == ValueType::Boolean4 || m_valueType == ValueType::Null || m_valueType == ValueType::Unknown
			);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector2i>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Vector3i GetVector3i() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(
				m_valueType == ValueType::Integer4 || m_valueType == ValueType::Integer || m_valueType == ValueType::Boolean ||
				m_valueType == ValueType::Boolean4 || m_valueType == ValueType::Null || m_valueType == ValueType::Unknown
			);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector3i>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Vector4i GetVector4i() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(
				m_valueType == ValueType::Integer4 || m_valueType == ValueType::Integer || m_valueType == ValueType::Boolean ||
				m_valueType == ValueType::Boolean4 || m_valueType == ValueType::Null || m_valueType == ValueType::Unknown
			);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector4i>(m_value);
		}
		[[nodiscard]] FORCE_INLINE FloatType GetDecimal() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Decimal || m_valueType == ValueType::Decimal4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<FloatType>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Rotation2Df GetRotation2Df() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Decimal || m_valueType == ValueType::Decimal4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Rotation2Df>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Vector2f GetVector2f() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Decimal || m_valueType == ValueType::Decimal4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector2f>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Vector3f GetVector3f() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Decimal || m_valueType == ValueType::Decimal4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector3f>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Vector4f GetVector4f() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Decimal || m_valueType == ValueType::Decimal4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector4f>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Rotation3Df GetRotation3Df() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Decimal || m_valueType == ValueType::Decimal4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Rotation3Df>(m_value);
		}
		[[nodiscard]] FORCE_INLINE explicit operator Math::Vector4f() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Decimal || m_valueType == ValueType::Decimal4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector4f>(m_value);
		}
		[[nodiscard]] FORCE_INLINE Math::Color GetColor() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Decimal || m_valueType == ValueType::Decimal4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Color>(m_value);
		}
		[[nodiscard]] FORCE_INLINE explicit operator Math::Color() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Decimal || m_valueType == ValueType::Decimal4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Color>(m_value);
		}
		[[nodiscard]] FORCE_INLINE explicit operator Math::Vector4i() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::Integer || m_valueType == ValueType::Integer4 || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Math::Vector4i>(m_value);
		}
		[[nodiscard]] bool ValidateIsNativeFunctionGuid() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			return m_valueType == ValueType::NativeFunctionGuid;
#else
			return false;
#endif
		}

		[[nodiscard]] Tag::Identifier GetTagIdentifier() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::TagIdentifier || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Tag::Identifier>(m_value);
		}
		[[nodiscard]] Rendering::SceneRenderStageIdentifier GetStageIdentifier() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::RenderStageIdentifier || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Rendering::SceneRenderStageIdentifier>(m_value);
		}
		[[nodiscard]] Entity::ComponentSoftReference GetComponentSoftReference() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::ComponentSoftReference || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Entity::ComponentSoftReference>(m_value);
		}
		[[nodiscard]] Asset::Identifier GetAssetIdentifier() const
		{
#if SCRIPTING_RAW_VALUE_DEBUG
			Assert(m_valueType == ValueType::AssetIdentifier || m_valueType == ValueType::Unknown);
#endif
			return VM::DynamicInvoke::ExtractArgument<Asset::Identifier>(m_value);
		}

		Register m_value;
#if SCRIPTING_RAW_VALUE_DEBUG
		ValueType m_valueType;
#endif

#undef SCRIPTING_RAW_VALUE_DEBUG
	};

	struct TRIVIAL_ABI Value : public RawValue
	{
		Value()
			: RawValue(nullptr)
			, type(ValueType::Null)
		{
		}
		Value(const nullptr_type)
			: RawValue(nullptr)
			, type(ValueType::Null)
		{
		}
		Value(const RawValue value, const ValueType type_)
			: RawValue(value)
			, type(type_)
		{
		}
		Value(const bool value)
			: RawValue{value}
			, type(ValueType::Boolean)
		{
		}
		Value(const Math::Vector4f::BoolType value)
			: RawValue{value}
			, type(ValueType::Boolean4)
		{
		}
		Value(const Math::Vector4i::BoolType value)
			: RawValue{value}
			, type(ValueType::Boolean4)
		{
		}
		Value(const IntegerType value)
			: RawValue{value}
			, type(ValueType::Integer)
		{
		}
		Value(const Math::Vector2i value)
			: RawValue{value}
			, type(ValueType::Integer4)
		{
		}
		Value(const Math::Vector3i value)
			: RawValue{value}
			, type(ValueType::Integer4)
		{
		}
		Value(const Math::Vector4i value)
			: RawValue{value}
			, type(ValueType::Integer4)
		{
		}
		Value(const FloatType value)
			: RawValue{value}
			, type(ValueType::Decimal)
		{
		}
		Value(const Math::Anglef value)
			: RawValue{value}
			, type(ValueType::Decimal)
		{
		}
		Value(const Math::Vector2f value)
			: RawValue{value}
			, type(ValueType::Decimal4)
		{
		}
		Value(const Math::Vector3f value)
			: RawValue{value}
			, type(ValueType::Decimal4)
		{
		}
		Value(const Math::Vector4f value)
			: RawValue{value}
			, type(ValueType::Decimal4)
		{
		}
		Value(const Math::Color value)
			: RawValue{value}
			, type(ValueType::Decimal4)
		{
		}
		Value(const Math::Angle3f value)
			: RawValue{value}
			, type(ValueType::Decimal4)
		{
		}
		Value(Object* const pObject)
			: RawValue{pObject}
			, type(ValueType::Object)
		{
		}
		Value(const Guid value)
			: RawValue{value}
			, type(ValueType::Guid)
		{
		}
		Value(const Entity::ComponentSoftReference value)
			: RawValue{value}
			, type(ValueType::ComponentSoftReference)
		{
		}

		[[nodiscard]] bool GetBool() const
		{
			Assert(type == ValueType::Boolean);
			return RawValue::GetBool();
		}
		[[nodiscard]] Math::Vector4f::BoolType GetBool4() const
		{
			Assert(type == ValueType::Boolean4);
			return RawValue::GetBool4f();
		}
		[[nodiscard]] IntegerType GetInteger() const
		{
			Assert(type == ValueType::Integer);
			return RawValue::GetInteger();
		}
		[[nodiscard]] Math::Vector4i GetVector4i() const
		{
			Assert(type == ValueType::Integer4);
			return RawValue::GetVector4i();
		}
		[[nodiscard]] FloatType GetDecimal() const
		{
			Assert(type == ValueType::Decimal);
			return RawValue::GetDecimal();
		}
		[[nodiscard]] Math::Vector4f GetVector4f() const
		{
			Assert(type == ValueType::Decimal4);
			return RawValue::GetVector4f();
		}
		[[nodiscard]] Math::Color GetColor() const
		{
			Assert(type == ValueType::Decimal4);
			return RawValue::GetColor();
		}
		[[nodiscard]] Object* GetObject() const
		{
			Assert(type == ValueType::Object);
			return RawValue::GetObject();
		}
		[[nodiscard]] Guid GetGuid() const
		{
			Assert(
				type == ValueType::Guid || type == ValueType::NativeFunctionGuid || type == ValueType::TagGuid || type == ValueType::AssetGuid ||
				type == ValueType::RenderStageGuid
			);
			return RawValue::GetGuid();
		}
		[[nodiscard]] Tag::Identifier GetTagIdentifier() const
		{
			Assert(type == ValueType::TagIdentifier);
			return RawValue::GetTagIdentifier();
		}
		[[nodiscard]] Rendering::SceneRenderStageIdentifier GetStageIdentifier() const
		{
			Assert(type == ValueType::RenderStageIdentifier);
			return RawValue::GetStageIdentifier();
		}
		[[nodiscard]] Entity::ComponentSoftReference GetComponentSoftReference() const
		{
			Assert(type == ValueType::ComponentSoftReference);
			return RawValue::GetComponentSoftReference();
		}
		[[nodiscard]] Asset::Identifier GetAssetIdentifier() const
		{
			Assert(type == ValueType::AssetIdentifier);
			return RawValue::GetAssetIdentifier();
		}

		ValueType type;
	};

	[[nodiscard]] StringType ValueToString(Value value);
	[[nodiscard]] bool ValueEquals(Value left, Value right);

	[[nodiscard]] inline ValueType GetValueType(Value value)
	{
		return value.type;
	}

	[[nodiscard]] inline bool IsNull(Value value)
	{
		return GetValueType(value) == ValueType::Null;
	}

	[[nodiscard]] inline bool IsBoolean(Value value)
	{
		return GetValueType(value) == ValueType::Boolean;
	}

	[[nodiscard]] inline bool IsInteger(Value value)
	{
		return GetValueType(value) == ValueType::Integer;
	}

	[[nodiscard]] inline bool IsDecimal(Value value)
	{
		return GetValueType(value) == ValueType::Decimal;
	}

	[[nodiscard]] inline bool IsObject(Value value)
	{
		return GetValueType(value) == ValueType::Object;
	}

	[[nodiscard]] inline bool IsGuid(Value value)
	{
		return value.type == ValueType::Guid || value.type == ValueType::NativeFunctionGuid;
	}

	[[nodiscard]] inline bool IsNumber(Value value)
	{
		return IsInteger(value) || IsDecimal(value);
	}

	[[nodiscard]] inline bool ValueToBoolean(Value value)
	{
		return IsBoolean(value) ? value.GetBool() : !IsNull(value);
	}
}
