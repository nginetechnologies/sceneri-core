#include "Components/SceneRules/Modules/LeaderboardModule.h"
#include "Components/SceneRules/Modules/ScoreModule.h"
#include "Components/SceneRules/Modules/CountdownModule.h"
#include "Components/SceneRules/Modules/HealthModule.h"
#include "Components/SceneRules/Modules/KDCounterModule.h"
#include "Components/SceneRules/Modules/FinishModule.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/ComponentSoftReference.inl>

#include <Engine/Threading/JobManager.h>

#include <Common/Reflection/Registry.inl>

#include <Engine/Context/Utils.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertySourceCache.h>
#include <Engine/DataSource/PropertyValue.h>

#include <GameFramework/Components/SceneRules/SceneRules.h>
#include <GameFramework/Components/Player/KDCounter.h>
#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Backend/Plugin.h>
#include <Backend/Leaderboard.h>

namespace ngine::GameFramework
{
	LeaderboardModule::LeaderboardModule(const LeaderboardModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
	{
	}

	LeaderboardModule::LeaderboardModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
	{
	}

	LeaderboardModule::LeaderboardModule(Initializer&& initializer)
		: SceneRulesModule(Forward<Initializer>(initializer))
	{
	}

	LeaderboardModule::~LeaderboardModule()
	{
	}

	void LeaderboardModule::OnParentCreated(SceneRules& sceneRules)
	{
		if (const Optional<FinishModule*> pFinishModule = sceneRules.FindDataComponentOfType<FinishModule>())
		{
			pFinishModule->OnPlayerFinished.Add(*this, &LeaderboardModule::OnPlayerFinishedInternal);
		}
	}

	struct Result
	{
		enum class Component : uint8
		{
			Score = 1 << 0,
			Health = 1 << 1,
			Kills = 1 << 2,
			Assists = 1 << 3,
			Deaths = 1 << 4,
			RemainingTime = 1 << 5
		};

		Result(SceneRules& sceneRules, const ClientIdentifier clientIdentifier)
		{
			if (const Optional<ScoreModule*> pScoreModule = sceneRules.FindDataComponentOfType<ScoreModule>())
			{
				m_score = (uint32)Math::Max(pScoreModule->GetScore(sceneRules, clientIdentifier), 0);
				m_components |= Component::Score;
			}
			if (const Optional<HealthModule*> pHealthModule = sceneRules.FindDataComponentOfType<HealthModule>())
			{
				m_health = (uint32)Math::Max(pHealthModule->GetHealth(sceneRules, clientIdentifier), 0.f);
				m_components |= Component::Health;
			}
			if (const Optional<KDCounterModule*> pKDCounterModule = sceneRules.FindDataComponentOfType<KDCounterModule>();
			    pKDCounterModule.IsValid() && pKDCounterModule->GetKDCounter().IsValid())
			{
				m_kills = pKDCounterModule->GetKDCounter()->GetKills();
				m_assists = pKDCounterModule->GetKDCounter()->GetAssists();
				m_deaths = pKDCounterModule->GetKDCounter()->GetDeaths();
				m_components |= Component::Kills;
				m_components |= Component::Assists;
				m_components |= Component::Deaths;
			}
			if (const Optional<CountdownModule*> pCountdownModule = sceneRules.FindDataComponentOfType<CountdownModule>();
			    pCountdownModule.IsValid())
			{
				m_remainingTime = pCountdownModule->GetRemainingTime();
				m_components |= Component::RemainingTime;
			}
		}

		bool SerializeComponent(Serialization::Writer writer, const Component component) const
		{
			Assert(m_components.IsSet(component));
			switch (component)
			{
				case Component::Score:
					if (m_score > 0)
					{
						writer.Serialize("score", m_score);
						return true;
					}
					break;
				case Component::Health:
					writer.Serialize("health", m_health);
					return true;
				case Component::Kills:
					if (m_kills > 0)
					{
						writer.Serialize("kills", m_kills);
						return true;
					}
					break;
				case Component::Assists:
					if (m_assists > 0)
					{
						writer.Serialize("assists", m_assists);
						return true;
					}
					break;
				case Component::Deaths:
					if (m_deaths > 0)
					{
						writer.Serialize("deaths", m_deaths);
						return true;
					}
					break;
				case Component::RemainingTime:
					if (m_remainingTime > 0_seconds)
					{
						writer.Serialize("remaining_time", m_remainingTime.GetSeconds());
						return true;
					}
					break;
			}
			return false;
		}

		bool Serialize(Serialization::Writer writer) const
		{
			EnumFlags<Component> writtenComponents{m_components};
			// writtenComponents.Clear(GetPrimaryComponent());

			bool wroteAny{false};
			for (const Component component : writtenComponents)
			{
				wroteAny |= SerializeComponent(writer, component);
			}
			return wroteAny;
		}

		[[nodiscard]] bool HasMetadata() const
		{
			EnumFlags<Component> writtenComponents{m_components};
			// writtenComponents.Clear(GetPrimaryComponent());
			return writtenComponents.AreAnySet();
		}

		[[nodiscard]] String GetMetadata() const
		{
			Serialization::Data data(rapidjson::Type::kObjectType);
			Serialization::Writer writer(data);
			if (Serialize(writer))
			{
				return data.SaveToBuffer<String>(Serialization::SavingFlags{});
			}
			else
			{
				return {};
			}
		}

		[[nodiscard]] Component GetPrimaryComponent() const
		{
			if (m_components.IsSet(Component::RemainingTime))
			{
				return Component::RemainingTime;
			}
			else if (m_components.IsSet(Component::Kills))
			{
				return Component::Kills;
			}
			else if (m_components.IsSet(Component::Score))
			{
				return Component::Score;
			}
			else
			{
				return Component{0};
			}
		}

		[[nodiscard]] uint64 GetPoints() const
		{
			uint64 points{0};
			const EnumFlags<Component> components{m_components};
			for (const Component component : components)
			{
				switch (component)
				{
					case Component::Score:
					{
						constexpr uint64 weight = 100;
						points += m_score * weight;
					}
					break;
					case Component::Health:
						break;
					case Component::Kills:
					{
						constexpr uint64 weight = 150;
						points += m_kills * weight;
					}
					break;
					case Component::Assists:
					{
						constexpr uint64 weight = 95;
						points += m_assists * weight;
					}
					break;
					case Component::Deaths:
					{
						constexpr uint64 weight = 110;
						points -= m_deaths * weight;
					}
					break;
					case Component::RemainingTime:
					{
						constexpr float weight = 4.f;
						points += uint64(m_remainingTime.GetSeconds() * weight);
					}
					break;
				}
			}

			return points;
		}

		[[nodiscard]] uint64 GetComponent(const Component component) const
		{
			switch (component)
			{
				case Component::Score:
					return m_score;
				case Component::Health:
					return m_health;
				case Component::Kills:
					return m_kills;
				case Component::Assists:
					return m_assists;
				case Component::Deaths:
					return m_assists;
				case Component::RemainingTime:
					return (uint64)m_remainingTime.GetMilliseconds();
				default:
					return 0;
			}
		}
	protected:
		uint32 m_score{0};
		uint32 m_health{0};
		uint32 m_kills{0};
		uint32 m_assists{0};
		uint32 m_deaths{0};
		Time::Durationf m_remainingTime{0_seconds};
		EnumFlags<Component> m_components;
	};

	void LeaderboardModule::OnPlayerFinishedInternal(
		SceneRules& sceneRules, const ClientIdentifier clientIdentifier, const GameRulesFinishResult finishResult
	)
	{
		if (!sceneRules.IsHost())
		{
			return;
		}
		// TODO: Reset leaderboard on (re-)publish

		if (sceneRules.GetRootScene().IsEditing())
		{
			// Don't submit leaderboard scores while editing
			return;
		}

		switch (finishResult)
		{
			case GameRulesFinishResult::Success:
			{
				if (const Optional<Networking::Backend::Plugin*> pBackendPlugin = System::FindPlugin<Networking::Backend::Plugin>())
				{
					const Result result{sceneRules, clientIdentifier};

					if (m_pLeaderboard.IsInvalid())
					{
						using Networking::Backend::Leaderboard;

						const Guid sceneGuid = sceneRules.GetRootScene().GetGuid();
						m_pLeaderboard.CreateInPlace(*pBackendPlugin, sceneGuid, Leaderboard::Flags::HasMetadata);
					}

					// Request the top 25 entries
					m_pLeaderboard->Get(
						Math::Range<uint32>::Make(0, 25),
						[]([[maybe_unused]] const bool success)
						{
						}
					);

					m_pLeaderboard->SubmitScoreOrCreate(result.GetPoints(), result.GetMetadata());
				}
			}
			break;
			case GameRulesFinishResult::Failure:
				break;
		}
	}

	[[maybe_unused]] const bool wasLeaderboardModuleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<LeaderboardModule>>::Make());
	[[maybe_unused]] const bool wasLeaderboardModuleTypeRegistered = Reflection::Registry::RegisterType<LeaderboardModule>();
}
