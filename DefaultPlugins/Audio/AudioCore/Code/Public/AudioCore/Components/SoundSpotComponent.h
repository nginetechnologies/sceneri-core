#pragma once

#include <Common/Reflection/CoreTypes.h>

#include "AudioCore/VolumeType.h"
#include "AudioCore/AudioSource.h"
#include "AudioCore/AudioIdentifier.h"

#include <Common/Asset/Picker.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/EnumFlags.h>
#include <Common/Math/Radius.h>
#include <Common/Time/Duration.h>

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>
#include <Engine/Entity/Component3D.h>

namespace ngine::Audio
{
	struct Data;
	struct Source;

	struct SoundSpotComponent : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "ea45accb-89d5-4816-9831-f116045b90e4"_guid;

		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 8>;

		enum class Flags : uint8
		{
			Looping = 1 << 0,
			Autoplay = 1 << 1,
			HasRequestedLoad = 1 << 2,
			Default = Looping | Autoplay,
			Settings = Looping | Autoplay
		};

		struct Initializer : public BaseType::DynamicInitializer
		{
			using BaseType = Entity::Component3D::DynamicInitializer;
			using BaseType::BaseType;
			Initializer(
				BaseType&& initializer,
				EnumFlags<Flags> soundFlags = {Flags::Default},
				Asset::Guid asset = {},
				const Volume volume = 100_percent,
				const Math::Radiusf radius = 5_meters
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_soundFlags(soundFlags)
				, m_asset(asset)
				, m_volume(volume)
				, m_radius(radius)
			{
			}

			EnumFlags<Flags> m_soundFlags;
			Asset::Guid m_asset;
			Volume m_volume{100_percent};
			Math::Radiusf m_radius{5_meters};
		};

		SoundSpotComponent(Initializer&& initializer);
		SoundSpotComponent(const Deserializer& deserializer);
		SoundSpotComponent(const SoundSpotComponent& templateComponent, const Cloner& cloner);

		virtual ~SoundSpotComponent();

		void OnCreated();

		void SetAudioAsset(const Asset::Picker asset);
		Threading::JobBatch
		SetDeserializedSound(const Asset::Picker asset, const Serialization::Reader objectReader, const Serialization::Reader typeReader);
		Asset::Picker GetAudioAsset() const;

		void Play();
		void Pause();
		void Stop();
		[[nodiscard]] bool IsPlaying() const;
		void PlayDelayed(Time::Durationf delay);
		void OnDisable();

		void SetLooping(bool shouldLoop);
		[[nodiscard]] bool IsLooping() const
		{
			return m_flags.IsSet(Flags::Looping);
		}

		void SetAutoplay(bool shouldAutoplay)
		{
			m_flags.Set(Flags::Autoplay, shouldAutoplay);
		}

		[[nodiscard]] bool ShouldAutoplay() const
		{
			return m_flags.IsSet(Flags::Autoplay);
		}

		void SetVolume(Volume volume);
		[[nodiscard]] Volume GetVolume() const
		{
			return m_volume;
		}

		void SetRadius(Math::Radiusf radius);
		[[nodiscard]] Math::Radiusf GetRadius() const
		{
			return m_radius;
		}
	protected:
		friend struct Reflection::ReflectedType<Audio::SoundSpotComponent>;
		virtual void OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags) override;

		void OnLoaded();

		[[nodiscard]] bool SetAudio(Asset::Guid assetGuid);
		[[nodiscard]] bool SetAudio(const Identifier identifier);
		Threading::JobBatch LoadAudioSource();
	protected:
		Optional<Audio::Data*> m_pAudioData = nullptr;
		UniquePtr<Audio::Source> m_pAudioSource = nullptr;

		EnumFlags<Flags> m_flags{Flags::Default};
		Volume m_volume{100_percent};
		Math::Radiusf m_radius{5_meters};
	};

	ENUM_FLAG_OPERATORS(SoundSpotComponent::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Audio::SoundSpotComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Audio::SoundSpotComponent>(
			Audio::SoundSpotComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Sound Spot"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Sound"),
					"audio_asset",
					"{BDC437CA-C2E5-4B93-94B5-22514A462CA9}"_guid,
					MAKE_UNICODE_LITERAL("Sound"),
					&Audio::SoundSpotComponent::SetAudioAsset,
					&Audio::SoundSpotComponent::GetAudioAsset,
					&Audio::SoundSpotComponent::SetDeserializedSound
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Looping"),
					"looping",
					"{2CA61225-26C4-4B44-88ED-92E45BAEAAD3}"_guid,
					MAKE_UNICODE_LITERAL("Sound"),
					&Audio::SoundSpotComponent::SetLooping,
					&Audio::SoundSpotComponent::IsLooping
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Auto Play"),
					"autoplay",
					"64b3d69e-b766-47be-86b1-d8916bd43254"_guid,
					MAKE_UNICODE_LITERAL("Sound"),
					&Audio::SoundSpotComponent::SetAutoplay,
					&Audio::SoundSpotComponent::ShouldAutoplay
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Volume"),
					"volume",
					"{92AC2E5F-ACCC-4DC3-A506-57B3067148B8}"_guid,
					MAKE_UNICODE_LITERAL("Sound"),
					&Audio::SoundSpotComponent::SetVolume,
					&Audio::SoundSpotComponent::GetVolume
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"outer_radius",
					"{8CA559EE-562A-4536-A91E-4B807C182362}"_guid,
					MAKE_UNICODE_LITERAL("Sound"),
					&Audio::SoundSpotComponent::SetRadius,
					&Audio::SoundSpotComponent::GetRadius
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "b319197e-722f-4df3-8ff7-f31ad83bfd2b"_asset, "93c90110-9374-420f-97f5-30b252e081a8"_guid
				},
				Entity::IndicatorTypeExtension{
					"dd68cae6-25c7-4e91-b2be-87c4ee208820"_guid,
				}
			}
		);
	};
}
