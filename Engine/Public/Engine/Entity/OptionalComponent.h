#pragma once

namespace ngine::Entity
{
	template<typename Type>
	struct OptionalComponent
	{
		OptionalComponent()
		{
			Type& component = *GetValuePtr();
			component.FlagAsDestroyed();
		}

		~OptionalComponent()
		{
			if (GetValuePtr()->FlagAsDestroyed())
			{
				GetValuePtr()->~Type();
			}
		}

		template<typename... Args>
		void CreateInPlace(Args&&... args)
		{
			Assert(IsInvalid());
			new (GetValuePtr()) Type(Forward<Args>(args)...);
			Assert(IsValid());
		}

		[[nodiscard]] bool IsValid() const noexcept
		{
			const Type& component = *GetValuePtr();
			return !component.IsDestroying();
		}

		[[nodiscard]] bool IsInvalid() const noexcept
		{
			const Type& component = *GetValuePtr();
			return component.IsDestroying();
		}

		[[nodiscard]] Type& Get()
		{
			Type& component = *GetValuePtr();
			Assert(!component.IsDestroying());
			return component;
		}

		[[nodiscard]] Type& GetUnsafe()
		{
			return *GetValuePtr();
		}
	protected:
		[[nodiscard]] Type* GetValuePtr() noexcept
		{
			return reinterpret_cast<Type*>(&m_data[0]);
		}

		[[nodiscard]] const Type* GetValuePtr() const noexcept
		{
			return reinterpret_cast<const Type*>(&m_data[0]);
		}
	private:
		alignas(Type) ByteType m_data[sizeof(Type)];
	};
}
