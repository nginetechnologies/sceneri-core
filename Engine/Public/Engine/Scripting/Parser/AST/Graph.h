#pragma once

#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/Function/ForwardDeclarations/Function.h>

namespace ngine::Scripting::AST
{
	struct Node;

	namespace Expression
	{
		struct Base;
		struct Function;
	}

	struct Graph
	{
		Graph() = default;
		Graph(const Graph&) = delete;
		Graph& operator=(const Graph&) = delete;
		Graph(Graph&&);
		Graph& operator=(Graph&&);
		~Graph();

		bool Serialize(const Serialization::Reader reader);
		bool Serialize(Serialization::Writer writer) const;

		[[nodiscard]] bool IsValid() const
		{
			return m_nodes.HasElements();
		}
		[[nodiscard]] bool IsInvalid() const
		{
			return m_nodes.IsEmpty();
		}

		template<typename NodeType>
		ReferenceWrapper<NodeType>& EmplaceNode(UniquePtr<NodeType>&& pNode)
		{
			UniquePtr<NodeType>& pEmplacedNode = m_nodes.EmplaceBack(Forward<UniquePtr<NodeType>>(pNode));
			return reinterpret_cast<ReferenceWrapper<NodeType>&>(pEmplacedNode);
		}
		template<typename NodeType, typename... Arguments>
		NodeType& EmplaceNode(Arguments&&... arguments)
		{
			UniquePtr<Node>& pEmplacedNode = m_nodes.EmplaceBack(UniquePtr<NodeType>::Make(Forward<Arguments>(arguments)...));
			return static_cast<NodeType&>(*pEmplacedNode);
		}

		using NodeIndexType = uint32;
		[[nodiscard]] Optional<Node*> GetNode(const NodeIndexType index) const
		{
			return m_nodes.IsValidIndex(index) ? m_nodes[index].Get() : Optional<Node*>{};
		}
		[[nodiscard]] Optional<NodeIndexType> GetNodeIndex(const Node& node) const
		{
			const auto it = m_nodes.FindIf(
				[&node](const UniquePtr<Node>& pNode)
				{
					return pNode.Get() == &node;
				}
			);
			return {m_nodes.GetIteratorIndex(it), it != m_nodes.end()};
		}

		[[nodiscard]] ArrayView<const ReferenceWrapper<const Node>> GetNodes() const
		{
			const ArrayView<const UniquePtr<Node>> nodes{m_nodes};
			return {reinterpret_cast<const ReferenceWrapper<const Node>*>(nodes.GetData()), nodes.GetSize()};
		}

		[[nodiscard]] Optional<const Expression::Function*> FindFunction(const Guid functionGuid) const;
		using FunctionCallback = Function<void(const Expression::Base& variable, const Expression::Function& functionExpression), 24>;
		void IterateFunctions(FunctionCallback&& callback) const;
	protected:
		Vector<UniquePtr<Node>, NodeIndexType> m_nodes;
	};
}
