#pragma once

#include "HierarchyComponentBase.h"

namespace ngine::Entity
{
	template<typename _ChildType>
	struct HierarchyComponent : public HierarchyComponentBase
	{
		using BaseType = HierarchyComponentBase;
		using ChildType = _ChildType;

		using BaseType::BaseType;
		virtual ~HierarchyComponent() = default;

		template<typename DataComponentType>
		using DataComponentResult = Entity::DataComponentResult<DataComponentType>;

		struct ChildView : private HierarchyComponentBase::ChildView
		{
			using BaseType = HierarchyComponentBase::ChildView;
			using ConstViewType = typename BaseType::ConstView;
			using BaseType::IndexType;
			using BaseType::ConstPointerType;
			using BaseType::Iterator;

			ChildView(BaseType&& view)
				: BaseType(Forward<BaseType>(view))
			{
			}
			using BaseType::BaseType;
			using BaseType::operator=;

			using BaseType::Contains;
			using BaseType::ContainsIf;
			using BaseType::GetSize;
			using BaseType::IsEmpty;
			using BaseType::HasElements;
			using BaseType::Find;
			using BaseType::FindIf;
			using BaseType::GetIteratorIndex;
			using BaseType::ContainsAny;
			using BaseType::All;
			using BaseType::Any;
			using BaseType::IsValidIndex;
			using BaseType::operator++;
			using BaseType::operator--;

			[[nodiscard]] ArrayView<const ReferenceWrapper<ChildType>, IndexType>
			GetSubView(const SizeType index, const SizeType count) const LIFETIME_BOUND
			{
				const ConstViewType baseView = BaseType::GetSubView(index, count);
				const ReferenceWrapper<Entity::HierarchyComponentBase>* pBaseValue = baseView.begin();
				return {reinterpret_cast<const ReferenceWrapper<ChildType>*>(pBaseValue), count};
			}

			[[nodiscard]] operator ArrayView<const ReferenceWrapper<ChildType>, IndexType>() const LIFETIME_BOUND
			{
				BaseType::IteratorType baseBegin = BaseType::begin();
				const ReferenceWrapper<Entity::HierarchyComponentBase>* pBaseValue = baseBegin.Get();
				return {reinterpret_cast<const ReferenceWrapper<ChildType>*>(pBaseValue), GetSize()};
			}

			using IteratorType = Iterator<const ReferenceWrapper<ChildType>>;
			[[nodiscard]] IteratorType begin() const noexcept
			{
				BaseType::IteratorType baseBegin = BaseType::begin();
				const ReferenceWrapper<Entity::HierarchyComponentBase>* pBaseValue = baseBegin.Get();
				return IteratorType(reinterpret_cast<const ReferenceWrapper<ChildType>*>(pBaseValue));
			}
			[[nodiscard]] IteratorType end() const noexcept
			{
				BaseType::IteratorType baseEnd = BaseType::end();
				const ReferenceWrapper<Entity::HierarchyComponentBase>* pBaseValue = baseEnd.Get();
				return IteratorType(reinterpret_cast<const ReferenceWrapper<ChildType>*>(pBaseValue));
			}

			[[nodiscard]] constexpr ChildType& operator[](const BaseType::IndexType index) const
			{
				return static_cast<ChildType&>(*BaseType::operator[](index));
			}
			[[nodiscard]] PURE_STATICS ChildType& GetLastElement() const noexcept
			{
				return static_cast<ChildType&>(*BaseType::GetLastElement());
			}

			[[nodiscard]] PURE_STATICS constexpr IndexType GetIteratorIndex(const ReferenceWrapper<ChildType>* it) const noexcept
			{
				return BaseType::GetIteratorIndex(reinterpret_cast<ConstPointerType>(it));
			}
		};
		[[nodiscard]] ChildView GetChildren() const LIFETIME_BOUND
		{
			return ChildView(BaseType::GetChildren());
		}

		[[nodiscard]] ChildType& GetChild(const ChildIndex index) const LIFETIME_BOUND
		{
			return GetChildren()[index];
		}
	};
}
