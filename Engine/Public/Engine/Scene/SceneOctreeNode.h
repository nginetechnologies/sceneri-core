#pragma once

#include <Engine/Entity/ComponentIdentifier.h>
#include <Engine/Tag/TagMask.h>

#include <Common/EnumFlags.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Storage/Identifier.h>
#include <Common/Threading/AtomicPtr.h>
#include <Common/Threading/AtomicEnum.h>
#include <Common/Threading/Atomics/Load.h>
#include <Common/Threading/Atomics/FetchAdd.h>
#include <Common/Threading/Atomics/FetchSubtract.h>
#include <Common/TypeTraits/ReturnType.h>
#include <Common/TypeTraits/IsSame.h>
#include <Common/EnumFlagOperators.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/Math/Vector4.h>
#include <Common/Math/WorldCoordinate/Min.h>
#include <Common/Math/WorldCoordinate/Max.h>
#include <Common/Math/Radius.h>
#include <Common/Math/Primitives/WorldBoundingBox.h>
#include <Common/Math/IsEquivalentTo.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Threading/Mutexes/SharedMutex.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct Window;
	struct PerInstanceUniformBufferObject;
	struct RenderViewThreadJob;
}

namespace ngine
{
	namespace Entity
	{
		struct CameraComponent;
		struct Component3D;
		struct RootSceneComponent;
		struct DestroyEmptyOctreeNodesJob;
	}

	struct SceneOctreeNode
	{
		using ContainerType = Array<Threading::Atomic<SceneOctreeNode*>, 8>;
		using ChildView = typename ContainerType::View;
		using ConstChildView = typename ContainerType::ConstView;
	protected:
		using ComponentContainer = Vector<ReferenceWrapper<Entity::Component3D>, Entity::ComponentIdentifier::InternalType>;
	public:
		SceneOctreeNode(
			SceneOctreeNode* const pParentNode, const uint8 nodeIndex, const Math::WorldCoordinate centerCoordinate, const Math::Radiusf radius
		)
			: m_childBoundingBox(CalculateStaticBoundingBox(centerCoordinate, radius))
			, m_centerCoordinateAndRadiusSquared{centerCoordinate.x, centerCoordinate.y, centerCoordinate.z, radius.GetMeters() * radius.GetMeters()}
			, m_pParentNode(pParentNode)
			, m_packedInfo(nodeIndex)
		{
		}
		~SceneOctreeNode()
		{
			m_flags |= Flags::Deleted;
		}

		void Resize(const Math::Vector3f centerCoordinate, const Math::Radiusf radius)
		{
			const float radiusSquared = radius.GetMeters() * radius.GetMeters();
			Assert(!GetCenterCoordinate().IsEquivalentTo(centerCoordinate) || !Math::IsEquivalentTo(GetRadiusSquared(), radiusSquared));
			const Math::WorldBoundingBox newArea = CalculateStaticBoundingBox(centerCoordinate, radius);

			m_centerCoordinateAndRadiusSquared = Math::Vector4f{centerCoordinate.x, centerCoordinate.y, centerCoordinate.z, radiusSquared};
			m_childBoundingBox.Expand(newArea);

			for (uint8 childIndex = 0; childIndex < 8; ++childIndex)
			{
				SceneOctreeNode* pChildNode = m_children[childIndex];
				if ((pChildNode == nullptr) | (pChildNode == reinterpret_cast<SceneOctreeNode*>(static_cast<uintptr>(0xDEADBEEF))))
				{
					continue;
				}
				const Math::WorldBoundingBox childArea = GetChildBoundingBox(childIndex, newArea);
				pChildNode->Resize(childArea.GetCenter(), radius * 0.5f);
			}
		}

		struct ComponentsView : private Threading::SharedLock<Threading::SharedMutex>, public ComponentContainer::ConstView
		{
			ComponentsView(Threading::SharedMutex& mutex, const ComponentContainer& container)
				: Threading::SharedLock<Threading::SharedMutex>(mutex)
				, ComponentContainer::ConstView(container.GetView())
			{
			}
		};

		[[nodiscard]] ComponentsView GetComponentsView() const
		{
			return {m_componentsMutex, m_components};
		}
		[[nodiscard]] bool HasChildren() const
		{
			return m_packedInfo.GetChildCount() > 0;
		}
		[[nodiscard]] bool HasComponents() const
		{
			return !m_components.IsEmpty();
		}
		[[nodiscard]] PURE_STATICS bool IsEmpty() const
		{
			return m_components.IsEmpty() & (m_packedInfo.GetChildCount() == 0);
		}
		[[nodiscard]] bool IsFlaggedForDeletion() const
		{
			return m_flags.IsSet(Flags::FlaggedForDeletion);
		}
		[[nodiscard]] bool WasDeleted() const
		{
			return m_flags.IsSet(Flags::Deleted);
		}
		[[nodiscard]] float GetRadiusSquared() const
		{
			return m_centerCoordinateAndRadiusSquared.w;
		}
		[[nodiscard]] Math::WorldCoordinate GetCenterCoordinate() const
		{
			return {m_centerCoordinateAndRadiusSquared.x, m_centerCoordinateAndRadiusSquared.y, m_centerCoordinateAndRadiusSquared.z};
		}
		[[nodiscard]] static Math::WorldBoundingBox
		CalculateStaticBoundingBox(const Math::WorldCoordinate centerCoordinate, const Math::Radiusf nodeRadius)
		{
			return {centerCoordinate - Math::Vector3f(nodeRadius.GetMeters()), centerCoordinate + Math::Vector3f(nodeRadius.GetMeters())};
		}
		[[nodiscard]] Math::WorldBoundingBox GetStaticBoundingBox() const
		{
			return CalculateStaticBoundingBox(GetCenterCoordinate(), Math::Radiusf::FromMeters(Math::Sqrt(GetRadiusSquared())));
		}
		[[nodiscard]] Math::WorldBoundingBox GetChildBoundingBox() const
		{
			return m_childBoundingBox;
		}
		[[nodiscard]] SceneOctreeNode* GetParent() const
		{
			return m_pParentNode;
		}
		[[nodiscard]] uint8 GetIndex() const
		{
			return m_packedInfo.GetNodeIndex();
		}

		void ExpandChildBoundingBox(const Math::WorldBoundingBox boundingBox)
		{
			m_childBoundingBox.Expand(boundingBox);
		}

		template<typename Callback>
		void IterateChildren(Callback&& callback)
		{
			for (SceneOctreeNode* pNode : m_children)
			{
				if ((pNode == nullptr) | (pNode == reinterpret_cast<SceneOctreeNode*>(static_cast<uintptr>(0xDEADBEEF))))
				{
					continue;
				}

				if constexpr (TypeTraits::IsSame<TypeTraits::ReturnType<Callback>, void>)
				{
					callback(*pNode);
				}
				else if (callback(*pNode) == Memory::CallbackResult::Break)
				{
					break;
				}
			}
		}

		struct CheckedChild
		{
			CheckedChild(SceneOctreeNode* pChild)
				: m_pChild(pChild)
			{
			}

			[[nodiscard]] operator Optional<SceneOctreeNode*>() const
			{
				if (m_pChild != reinterpret_cast<SceneOctreeNode*>(static_cast<uintptr>(0xDEADBEEF)))
				{
					return m_pChild;
				}
				else
				{
					return nullptr;
				}
			}
		protected:
			SceneOctreeNode* m_pChild;
		};

		[[nodiscard]] FixedArrayView<CheckedChild, 8> GetChildren() const
		{
			const FixedArrayView<const Threading::Atomic<SceneOctreeNode*>, 8> children = m_children.GetView();
			return reinterpret_cast<const FixedArrayView<CheckedChild, 8>&>(children);
		}

		[[nodiscard]] bool ReserveChild(const uint8 index, SceneOctreeNode*& pExpected)
		{
			return m_children[index].CompareExchangeStrong(pExpected, reinterpret_cast<SceneOctreeNode*>(static_cast<uintptr>(0xDEADBEEF)));
		}

		template<typename... Args>
		void AddChild(const uint8 index, SceneOctreeNode& node)
		{
			Assert(!IsFlaggedForDeletion());

			SceneOctreeNode* pExpectedChild = reinterpret_cast<SceneOctreeNode*>(static_cast<uintptr>(0xDEADBEEF));
			[[maybe_unused]] const bool wasExchanged = m_children[index].CompareExchangeStrong(pExpectedChild, &node);
			Assert(wasExchanged);
			m_packedInfo.FetchAddChildCount();
		}

		enum class RemovalResult
		{
			Done,
			RemovedLastElement
		};

		[[nodiscard]] RemovalResult RemoveChild(const uint8 index, SceneOctreeNode& expectedChild)
		{
			Assert(!WasDeleted());
			Assert(!IsFlaggedForDeletion());
			Assert(expectedChild.IsFlaggedForDeletion());

			SceneOctreeNode* pExpectedChild = &expectedChild;
			const bool wasExchanged = m_children[index].CompareExchangeStrong(pExpectedChild, nullptr);
			Expect(wasExchanged);
			const uint8 remainingChildCount = m_packedInfo.FetchSubChildCount();
			return remainingChildCount == 1 ? RemovalResult::RemovedLastElement : RemovalResult::Done;
		}

		[[nodiscard]] uint8 GetChildIndexAtCoordinate(const Math::WorldCoordinate coordinate) const
		{
			const Math::WorldCoordinate centerCoordinate = GetCenterCoordinate();
			return ((coordinate.x > centerCoordinate.x) * 4) | ((coordinate.y > centerCoordinate.y) * 2) | (coordinate.z > centerCoordinate.z);
		}
		[[nodiscard]] PURE_STATICS static Math::TVector3<uint8> GetChildRelativeCoordinate(const uint8 childIndex)
		{
			const uint8 x = childIndex / 4u;
			const uint8 y = (childIndex - x * 4u) / 2u;
			const uint8 z = (childIndex - x * 4u - y * 2u);
			return {x, y, z};
		}
		[[nodiscard]] PURE_STATICS static Math::WorldBoundingBox
		GetChildBoundingBox(const uint8 childIndex, const Math::BoundingBox boundingBox)
		{
			const Math::Vector3f childSize = boundingBox.GetSize() * 0.5f;
			const Math::Vector3f newPosition = boundingBox.GetMinimum() + childSize * (Math::Vector3f)GetChildRelativeCoordinate(childIndex);
			return Math::WorldBoundingBox{newPosition, newPosition + childSize};
		}
		[[nodiscard]] PURE_STATICS Math::WorldBoundingBox GetChildBoundingBox(const uint8 childIndex) const
		{
			const Math::BoundingBox boundingBox = GetStaticBoundingBox();
			return GetChildBoundingBox(childIndex, boundingBox);
		}

		enum class Flags : uint8
		{
			FlaggedForDeletion = 1 << 0,
			Deleted = 1 << 1
		};

		[[nodiscard]] bool FlagForDeletion()
		{
			Assert(!HasChildren());
			Assert(!HasComponents());

			const EnumFlags<SceneOctreeNode::Flags> previousFlags = m_flags.FetchOr(SceneOctreeNode::Flags::FlaggedForDeletion);
			return !previousFlags.IsSet(SceneOctreeNode::Flags::FlaggedForDeletion);
		}

		void UpdateMask(const Tag::Mask mask)
		{
			Tag::Mask previousMask;
			if (m_pTagMask != nullptr)
			{
				previousMask = *m_pTagMask |= mask;
			}
			else
			{
				m_pTagMask.CreateInPlace(mask);
			}

			OnMaskChanged(mask);
		}

		void AddComponent(Entity::Component3D& component, const Tag::Mask mask)
		{
			Assert(!WasDeleted());
			Assert(!IsFlaggedForDeletion());
			{
				Threading::UniqueLock lock(m_componentsMutex);
				m_components.EmplaceBack(component);
			}
			UpdateMask(mask);
		}

		void OnMaskChanged(Tag::Mask newlySetBits)
		{
			if (newlySetBits.AreAnySet() && m_pParentNode != nullptr)
			{
				SceneOctreeNode& parentNode = static_cast<SceneOctreeNode&>(*m_pParentNode);
				if (parentNode.m_pTagMask == nullptr)
				{
					parentNode.m_pTagMask.CreateInPlace();
				}

				const Tag::Mask previousParentTagMask = parentNode.m_pTagMask->operator|=(newlySetBits);
				newlySetBits = (previousParentTagMask ^ newlySetBits) & newlySetBits;
				if (newlySetBits.AreAnySet())
				{
					parentNode.OnMaskChanged(newlySetBits);
				}
			}
		}

		[[nodiscard]] RemovalResult RemoveComponent(Entity::Component3D& component)
		{
			Assert(!WasDeleted());
			Assert(!IsFlaggedForDeletion());
			Threading::UniqueLock lock(m_componentsMutex);
			const bool wasRemoved = m_components.RemoveFirstOccurrence(component);

			return wasRemoved && m_components.IsEmpty() ? RemovalResult::RemovedLastElement : RemovalResult::Done;
		}

		[[nodiscard]] bool ContainsAnyTags(const Tag::Mask mask) const
		{
			return m_pTagMask.IsValid() && m_pTagMask->AreAnySet(mask);
		}
		[[nodiscard]] bool ContainsTag(const Tag::Identifier identifier) const
		{
			return m_pTagMask.IsValid() && m_pTagMask->IsSet(identifier);
		}
	protected:
		struct PackedInfo
		{
			PackedInfo(const uint8 nodeIndex)
				: m_value(uint8(nodeIndex << (uint8)4))
			{
			}

			[[nodiscard]] uint8 GetChildCount() const
			{
				return Threading::Atomics::Load(m_value) & 0b00001111;
			}

			[[nodiscard]] uint8 GetNodeIndex() const
			{
				return uint8(m_value >> 4);
			}

			uint8 FetchAddChildCount()
			{
				const uint8 previousValue = Threading::Atomics::FetchAdd(m_value, (uint8)1) & 0b00001111;
				Assert(previousValue <= 8);
				return previousValue;
			}

			uint8 FetchSubChildCount()
			{
				const uint8 previousValue = Threading::Atomics::FetchSubtract(m_value, (uint8)1) & 0b00001111;
				Assert(previousValue > 0);
				return previousValue;
			}
		protected:
			uint8 m_value;
		};
	protected:
		Math::WorldBoundingBox m_childBoundingBox;
		Math::TVector4<Math::WorldCoordinateUnitType> m_centerCoordinateAndRadiusSquared;
		ContainerType m_children = {};

		mutable Threading::SharedMutex m_componentsMutex;
		ComponentContainer m_components;

		SceneOctreeNode* const m_pParentNode;

		UniquePtr<Threading::AtomicIdentifierMask<Tag::Identifier>> m_pTagMask;
		PackedInfo m_packedInfo;

		AtomicEnumFlags<Flags> m_flags;
	};

	ENUM_FLAG_OPERATORS(SceneOctreeNode::Flags);
}
