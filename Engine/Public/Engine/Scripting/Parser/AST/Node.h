#pragma once

#include <Common/Math/Vector2.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/Memory/UniquePtr.h>

namespace ngine::Scripting::AST
{
	struct Graph;

	enum class NodeType : uint8
	{
		Binary,
		Logical,
		Unary,
		Group,
		Literal,
		VariableDeclaration,
		Variable,
		Assignment,
		Call,
		Function,

		Expression,
		Block,
		If,
		While,
		Repeat,
		Break,
		Return,
		Count
	};

	namespace Statement
	{
		struct Base;
	}

	namespace Expression
	{
		struct Base;
	}

	struct Node
	{
		using Type = NodeType;
		using CoordinateType = Math::Vector2i;

		Node(const Type type)
			: m_type(type)
		{
		}
		~Node() = default;

		[[nodiscard]] Type GetType() const
		{
			return m_type;
		}
		[[nodiscard]] bool IsStatement() const;

		void SetCoordinate(const CoordinateType coordinate)
		{
			m_coordinate = coordinate;
		}
		[[nodiscard]] CoordinateType GetCoordinate() const
		{
			return m_coordinate;
		}

		bool Serialize(const Serialization::Reader reader, Graph& graph);
		bool Serialize(Serialization::Writer writer, const Graph& graph) const;

		[[nodiscard]] bool operator==(const Node& other) const;
		[[nodiscard]] bool operator!=(const Node& other) const
		{
			return !operator==(other);
		}
	private:
		Type m_type;
		CoordinateType m_coordinate{Math::Zero};
	};
}
