#pragma once

#include <Engine/Asset/Identifier.h>
#include <Engine/Entity/Component3D.h>

#include <Common/Math/Ratio.h>
#include <Common/Math/Radius.h>

namespace ngine::Audio
{
	void PlaySoundSpot(Entity::Component3D& component);
	void PauseSoundSpot(Entity::Component3D& component);
	void StopSoundSpot(Entity::Component3D& component);
	[[nodiscard]] bool IsSoundSpotPlaying(Entity::Component3D& component);
	void SetSoundSpotAsset(Entity::Component3D& component, const Asset::Identifier audioAssetIdentifier);
	void SetSoundSpotLooping(Entity::Component3D& component, const bool isLooping);
	void SetSoundSpotAutoPlay(Entity::Component3D& component, const bool shouldAutoPlay);
	void SetSoundSpotVolume(Entity::Component3D& component, const Math::Ratiof volume);
	void SetSoundSpotRadius(Entity::Component3D& component, const Math::Radiusf radius);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedFunction<&Audio::PlaySoundSpot>
	{
		static constexpr auto Function = Reflection::Function{
			"8e73014c-a2c8-4e40-bed3-203fb8afbf73"_guid,
			MAKE_UNICODE_LITERAL("Play Sound Spot"),
			&Audio::PlaySoundSpot,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};
	template<>
	struct ReflectedFunction<&Audio::PauseSoundSpot>
	{
		static constexpr auto Function = Reflection::Function{
			"58209fc3-5ae1-4d74-adfe-c37caae965d1"_guid,
			MAKE_UNICODE_LITERAL("Pause Sound Spot"),
			&Audio::PauseSoundSpot,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};
	template<>
	struct ReflectedFunction<&Audio::StopSoundSpot>
	{
		static constexpr auto Function = Reflection::Function{
			"f24a72ca-5a63-460b-a17c-12a86c11436e"_guid,
			MAKE_UNICODE_LITERAL("Stop Sound Spot"),
			&Audio::StopSoundSpot,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};
	template<>
	struct ReflectedFunction<&Audio::IsSoundSpotPlaying>
	{
		static constexpr auto Function = Reflection::Function{
			"c2dafc9f-4570-4a93-a8a0-77abc1bbb547"_guid,
			MAKE_UNICODE_LITERAL("Is Sound Spot Playing"),
			&Audio::IsSoundSpotPlaying,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{"dc4eee93-2aa8-4a6f-a489-b3dc92a336c4"_guid, MAKE_UNICODE_LITERAL("Playing")},
			Reflection::Argument{
				"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};
	template<>
	struct ReflectedFunction<&Audio::SetSoundSpotAsset>
	{
		static constexpr auto Function = Reflection::Function{
			"2b6a5bef-2c5f-420b-b739-480e767ea3f3"_guid,
			MAKE_UNICODE_LITERAL("Set Sound Spot Asset"),
			&Audio::SetSoundSpotAsset,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Sound")}
		};
	};
	template<>
	struct ReflectedFunction<&Audio::SetSoundSpotLooping>
	{
		static constexpr auto Function = Reflection::Function{
			"bcdf83d9-f595-4776-81b4-f9846c774a80"_guid,
			MAKE_UNICODE_LITERAL("Set Sound Spot Looping"),
			&Audio::SetSoundSpotLooping,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Enable")}
		};
	};
	template<>
	struct ReflectedFunction<&Audio::SetSoundSpotAutoPlay>
	{
		static constexpr auto Function = Reflection::Function{
			"1a1094d0-8b6f-42a7-b454-0d2c6d7482c7"_guid,
			MAKE_UNICODE_LITERAL("Set Sound Spot Auto Play"),
			&Audio::SetSoundSpotAutoPlay,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Enable")}
		};
	};
	template<>
	struct ReflectedFunction<&Audio::SetSoundSpotVolume>
	{
		static constexpr auto Function = Reflection::Function{
			"3476c423-c476-48a4-b5d9-02f8cfa7804c"_guid,
			MAKE_UNICODE_LITERAL("Set Sound Spot Volume"),
			&Audio::SetSoundSpotVolume,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Volume")}
		};
	};
	template<>
	struct ReflectedFunction<&Audio::SetSoundSpotRadius>
	{
		static constexpr auto Function = Reflection::Function{
			"af8e2780-fa40-45e3-80f9-da4199fa1d77"_guid,
			MAKE_UNICODE_LITERAL("Set Sound Spot Radius"),
			&Audio::SetSoundSpotRadius,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Radius")}
		};
	};
}
