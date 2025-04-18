#pragma once

#include <Engine/Asset/Identifier.h>
#include <Common/Asset/Reference.h>
#include <Engine/Entity/Component2D.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentSoftReference.h>
#include <Engine/Tag/TagIdentifier.h>
#include <Engine/Scripting/Parser/ScriptValue.h>

#include <Renderer/Assets/Stage/SceneRenderStageIdentifier.h>
#include <Renderer/Assets/Stage/SceneRenderStageGuid.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Assets/Texture/TextureGuid.h>

#include <Common/Tag/TagGuid.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Vector3.h>
#include <Common/Math/WorldCoordinate.h>
#include <Common/Math/Vector4.h>
#include <Common/Math/Color.h>
#include <Common/Math/Rotation2D.h>
#include <Common/Math/Quaternion.h>
#include <Common/Math/Transform2D.h>
#include <Common/Math/Transform.h>
#include <Common/Memory/Containers/Array.h>
#include <Common/Reflection/TypeDefinition.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/DynamicFunction.h>

namespace ngine::Scripting
{
	[[nodiscard]] constexpr Reflection::TypeDefinition GetTokenTypeDefinition(TokenType tokenType)
	{
		switch (tokenType)
		{
			case TokenType::Null:
				return Reflection::TypeDefinition::Get<nullptr_type>();
			case TokenType::Function:
				return Reflection::TypeDefinition::Get<VM::DynamicFunction>();
			case TokenType::String:
				return Reflection::TypeDefinition::Get<StringType>();
			case TokenType::Integer:
				return Reflection::TypeDefinition::Get<IntegerType>();
			case TokenType::Float:
			case TokenType::Number:
				//! Note: not following Teal rules here, 'number' is only float in our case, but for Teal is either
				return Reflection::TypeDefinition::Get<FloatType>();
			case TokenType::Boolean:
				return Reflection::TypeDefinition::Get<bool>();
			case TokenType::Component2D:
				return Reflection::TypeDefinition::Get<Entity::Component2D>();
			case TokenType::Component3D:
				return Reflection::TypeDefinition::Get<Entity::Component3D>();
			case TokenType::ComponentSoftReference:
				return Reflection::TypeDefinition::Get<Entity::ComponentSoftReference>();
			case TokenType::Vec2i:
				return Reflection::TypeDefinition::Get<Math::Vector2i>();
			case TokenType::Vec2f:
				return Reflection::TypeDefinition::Get<Math::Vector2f>();
			case TokenType::Vec2b:
				return Reflection::TypeDefinition::Get<Math::Vector2i::BoolType>();
			case TokenType::Vec3i:
				return Reflection::TypeDefinition::Get<Math::Vector3i>();
			case TokenType::Vec3f:
				return Reflection::TypeDefinition::Get<Math::Vector3f>();
			case TokenType::Vec3b:
				return Reflection::TypeDefinition::Get<Math::Vector3i::BoolType>();
			case TokenType::Vec4i:
				return Reflection::TypeDefinition::Get<Math::Vector4i>();
			case TokenType::Vec4f:
				return Reflection::TypeDefinition::Get<Math::Vector4f>();
			case TokenType::Vec4b:
				return Reflection::TypeDefinition::Get<Math::Vector4i::BoolType>();
			case TokenType::Color:
				return Reflection::TypeDefinition::Get<Math::Color>();
			case TokenType::Rotation2D:
				return Reflection::TypeDefinition::Get<Math::Rotation2Df>();
			case TokenType::Rotation3D:
				return Reflection::TypeDefinition::Get<Math::Rotation3Df>();
			case TokenType::Transform2D:
				return Reflection::TypeDefinition::Get<Math::Transform2Df>();
			case TokenType::Transform3D:
				return Reflection::TypeDefinition::Get<Math::Transform3Df>();
			case TokenType::Asset:
				return Reflection::TypeDefinition::Get<Asset::Identifier>();
			case TokenType::TextureAsset:
				return Reflection::TypeDefinition::Get<Rendering::TextureIdentifier>();
			case TokenType::Tag:
				return Reflection::TypeDefinition::Get<Tag::Identifier>();
			case TokenType::RenderStage:
				return Reflection::TypeDefinition::Get<Rendering::SceneRenderStageIdentifier>();
			default:
				return {};
		};
	}

	[[nodiscard]] constexpr TokenType GetTokenType(const Reflection::TypeDefinition typeDefinition)
	{
		if (typeDefinition.Is<nullptr_type>())
		{
			return TokenType::Null;
		}
		else if (typeDefinition.Is<VM::DynamicFunction>())
		{
			return TokenType::Function;
		}
		else if (typeDefinition.Is<StringType>())
		{
			return TokenType::String;
		}
		else if (typeDefinition.Is<IntegerType>())
		{
			return TokenType::Integer;
		}
		else if (typeDefinition.Is<FloatType>())
		{
			return TokenType::Float;
		}
		else if (typeDefinition.Is<bool>())
		{
			return TokenType::Boolean;
		}
		else if (typeDefinition.Is<Entity::Component2D>() || typeDefinition.Is<ReferenceWrapper<Entity::Component2D>>())
		{
			return TokenType::Component2D;
		}
		else if (typeDefinition.Is<Entity::Component3D>() || typeDefinition.Is<ReferenceWrapper<Entity::Component3D>>())
		{
			return TokenType::Component3D;
		}
		else if (typeDefinition.Is<Entity::ComponentSoftReference>())
		{
			return TokenType::ComponentSoftReference;
		}
		else if (typeDefinition.Is<Math::Vector2i>())
		{
			return TokenType::Vec2i;
		}
		else if (typeDefinition.Is<Math::Vector2f>())
		{
			return TokenType::Vec2f;
		}
		else if (typeDefinition.Is<Math::Vector2i::BoolType>())
		{
			return TokenType::Vec2b;
		}
		else if (typeDefinition.Is<Math::Vector3i>())
		{
			return TokenType::Vec3i;
		}
		else if (typeDefinition.Is<Math::Vector3f>() || typeDefinition.Is<Math::WorldCoordinate>())
		{
			return TokenType::Vec3f;
		}
		else if (typeDefinition.Is<Math::Vector3i::BoolType>())
		{
			return TokenType::Vec3b;
		}
		else if (typeDefinition.Is<Math::Vector4i>())
		{
			return TokenType::Vec4i;
		}
		else if (typeDefinition.Is<Math::Vector4f>())
		{
			return TokenType::Vec4f;
		}
		else if (typeDefinition.Is<Math::Vector4i::BoolType>())
		{
			return TokenType::Vec4b;
		}
		else if (typeDefinition.Is<Math::Color>())
		{
			return TokenType::Color;
		}
		else if (typeDefinition.Is<Math::Rotation2Df>())
		{
			return TokenType::Rotation2D;
		}
		else if (typeDefinition.Is<Math::Rotation3Df>())
		{
			return TokenType::Rotation3D;
		}
		else if (typeDefinition.Is<Math::Transform2Df>())
		{
			return TokenType::Transform2D;
		}
		else if (typeDefinition.Is<Math::Transform3Df>())
		{
			return TokenType::Transform3D;
		}
		else if (typeDefinition.Is<Asset::Identifier>() || typeDefinition.Is<Asset::Guid>() || typeDefinition.Is<Asset::Reference>())
		{
			return TokenType::Asset;
		}
		else if (typeDefinition.Is<Rendering::TextureIdentifier>() || typeDefinition.Is<Rendering::TextureGuid>())
		{
			return TokenType::Asset;
		}
		else if (typeDefinition.Is<Tag::Identifier>() || typeDefinition.Is<Tag::Guid>())
		{
			return TokenType::Tag;
		}
		else if (typeDefinition.Is<Rendering::SceneRenderStageIdentifier>() || typeDefinition.Is<Rendering::StageGuid>())
		{
			return TokenType::RenderStage;
		}
		else
		{
			return TokenType::Invalid;
		}
	}
}
