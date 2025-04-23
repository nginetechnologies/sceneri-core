#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Common/Function/Event.h>
#include <Common/Reflection/CoreTypes.h>

namespace ngine
{
	namespace Entity
	{
		struct Component3D;
	}
}

namespace ngine::GameFramework
{
	struct KDCounter final : public Entity::Data::Component3D
	{
		inline static constexpr Guid TypeGuid = "7c95b5c9-a877-444d-b63b-b2169e889293"_guid;

		using BaseType = Entity::Data::Component3D;
		using ValueType = int32;

		Event<void(void*), 24> OnChanged;

		using Initializer = BaseType::DynamicInitializer;

		KDCounter(const Deserializer& deserializer);
		KDCounter(const KDCounter& templateComponent, const Cloner& cloner);
		KDCounter(Initializer&& initializer);
		KDCounter(DynamicInitializer& dynamicInitializer);

		void OnCreated()
		{
			m_kills = 0;
			m_assists = 0;
			m_deaths = 0;
			OnChanged();
		}

		[[nodiscard]] ValueType GetKills()
		{
			return m_kills;
		}

		[[nodiscard]] ValueType GetAssists()
		{
			return m_assists;
		}

		[[nodiscard]] ValueType GetDeaths()
		{
			return m_deaths;
		}

		void AddKill();
		void AddAssist();
		void AddDeath();

		[[nodiscard]] float GetRatio() const
		{
			return m_deaths > 0 ? static_cast<float>(m_kills) / static_cast<float>(m_deaths) : static_cast<float>(m_kills);
		}
	protected:
		friend struct Reflection::ReflectedType<GameFramework::KDCounter>;

		Entity::Component3D& m_owner;

		ValueType m_kills{0};
		ValueType m_assists{0};
		ValueType m_deaths{0};

		ValueType m_killScore{100};
		ValueType m_assistScore{50};
		ValueType m_deathScore{0};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::KDCounter>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::KDCounter>(
			GameFramework::KDCounter::TypeGuid,
			MAKE_UNICODE_LITERAL("KDCounter Component"),
			Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Kill Score"),
					"killScore",
					"{A894C510-B919-4CB0-9A8B-054954B2875A}"_guid,
					MAKE_UNICODE_LITERAL("KD Counter"),
					Reflection::PropertyFlags{},
					&GameFramework::KDCounter::m_killScore
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Assist Score"),
					"assitsScore",
					"{BB5D7FD6-B09D-4281-B69B-C0C9089C30AB}"_guid,
					MAKE_UNICODE_LITERAL("KD Counter"),
					Reflection::PropertyFlags{},
					&GameFramework::KDCounter::m_assistScore
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Death Score"),
					"deathScore",
					"{C20679E5-B6B3-4E72-A715-34942C313111}"_guid,
					MAKE_UNICODE_LITERAL("KD Counter"),
					Reflection::PropertyFlags{},
					&GameFramework::KDCounter::m_deathScore
				},
			}
		);
	};
}
