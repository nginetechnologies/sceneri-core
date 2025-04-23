#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentPicker.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>
#include <Engine/Tag/TagMask.h>

#include <Common/Asset/Picker.h>
#include <Common/Reflection/CoreTypes.h>
#include <Common/Time/Duration.h>
#include <Common/Function/Event.h>

namespace ngine::Entity
{
	struct CameraComponent;
}

namespace ngine::GameFramework
{
	struct SplineMovementComponent;
	namespace Signal
	{
		struct Transmitter;
	}

	struct CinematicComponent final : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "b01764f6-5e1c-43e6-8b0d-b6aa3a21c35f"_guid;
		using InstanceIdentifier = TIdentifier<uint32, 8>;
		using BaseType = Entity::Component3D;
		using BaseType::BaseType;

		CinematicComponent(const CinematicComponent& templateComponent, const Cloner& cloner);
		CinematicComponent(const Deserializer& deserializer);
		CinematicComponent(Initializer&& initializer);
		virtual ~CinematicComponent();

		void OnCreated();
		void OnSimulationResumed();
		void OnSimulationPaused();
		void OnEnabled();
		void OnDisabled();
		void OnSequenceFinished();

		bool StartCameraSequence(uint8 index);

		[[nodiscard]] Optional<Entity::CameraComponent*> FindCamera(uint8 index);

		void SwitchToCamera(Entity::CameraComponent& camera);
		void RegisterFinishCondition(Time::Durationf duration = Time::Durationf::FromSeconds(0));
		void RestartSequence();
	protected:
		friend struct Reflection::ReflectedType<CinematicComponent>;

		void StartSequence();

		void SetCameraPicker(uint8 cameraIndex, const Entity::Component3DPicker cameraPicker);
		Entity::Component3DPicker GetCameraPicker(uint8 cameraIndex) const;

		void SetCameraDuration(uint8 cameraIndex, Time::Durationf time);
		Time::Durationf GetCameraDuration(uint8 cameraIndex) const;
	private:
		// TODO: Replace this when we support arrays in UI
#define CameraGetterAndSetter(index) \
	void SetCameraPicker##index(const Entity::Component3DPicker cameraPicker); \
	Entity::Component3DPicker GetCameraPicker##index() const; \
	void SetCameraDuration##index(float time); \
	float GetCameraDuration##index() const

		CameraGetterAndSetter(0);
		CameraGetterAndSetter(1);
		CameraGetterAndSetter(2);
		CameraGetterAndSetter(3);
		CameraGetterAndSetter(4);
		CameraGetterAndSetter(5);
		CameraGetterAndSetter(6);
		CameraGetterAndSetter(7);
		CameraGetterAndSetter(8);
		CameraGetterAndSetter(9);
		CameraGetterAndSetter(10);
		CameraGetterAndSetter(11);
		CameraGetterAndSetter(12);
		CameraGetterAndSetter(13);
		CameraGetterAndSetter(14);
		CameraGetterAndSetter(15);
#undef CameraGetterAndSetter
	private:
		static constexpr uint8 MaximumSlotCount = 16;
		Array<Entity::ComponentSoftReference, MaximumSlotCount> m_cameras;
		Array<Time::Durationf, MaximumSlotCount> m_cameraDurations;
		bool m_isRunning{false};
		bool m_shouldLoop{false};
		uint8 m_currentIndex{0};

		Optional<Signal::Transmitter*> m_pTransmitter;
	};
}

namespace ngine::Reflection
{
#define CameraProperties(index, guid1, guid2) \
	Reflection::MakeDynamicProperty( \
		u"Camera " #index, \
		"camera" #index, \
		guid2, \
		u"Cinematic", \
		&GameFramework::CinematicComponent::SetCameraPicker##index, \
		&GameFramework::CinematicComponent::GetCameraPicker##index \
	), \
		Reflection::MakeDynamicProperty( \
			u"Duration " #index, \
			"delay" #index, \
			guid2, \
			u"Cinematic", \
			&GameFramework::CinematicComponent::SetCameraDuration##index, \
			&GameFramework::CinematicComponent::GetCameraDuration##index \
		)

	template<>
	struct ReflectedType<GameFramework::CinematicComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::CinematicComponent>(
			GameFramework::CinematicComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Cinematic Component"),
			Reflection::TypeFlags::DisableDynamicInstantiation,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Loop"),
					"loop",
					"{37DC253D-F09E-4AEE-A8B9-1B4CD570FA7B}"_guid,
					MAKE_UNICODE_LITERAL("Cinematic"),
					&GameFramework::CinematicComponent::m_shouldLoop
				),
				CameraProperties(0, "{1FF84519-0070-49AD-AE63-136B4B027328}"_guid, "de0108ef-48a8-4e1a-8be7-c85b151040ae"_guid),
				CameraProperties(1, "{7D08622A-33D3-42AA-83CC-378E27E1E538}"_guid, "1d00df26-8d95-42a7-99ac-5dcd94c84ea0"_guid),
				CameraProperties(2, "{F3250594-B4F9-4A7D-B2F9-A5F7D00EA96D}"_guid, "2478ecf2-a321-4be2-9f8b-b4deb6e18764"_guid),
				CameraProperties(3, "{160ED60F-4B34-4CC0-8CE7-F3ED758ED05E}"_guid, "7e2afcb3-5fe0-4a75-b599-1395a18fe304"_guid),
				CameraProperties(4, "{F3C0B652-7D95-48B7-AA3D-187EDE175242}"_guid, "3e8cec07-b9b9-4738-ab1f-27ed276275d4"_guid),
				CameraProperties(5, "{304E6ECA-CA32-4CF2-AFD6-973676BFEE55}"_guid, "8cef9e26-acdf-4c40-9ef8-89f9674548ec"_guid),
				CameraProperties(6, "{842F40D3-300B-415B-B5F6-8CD171016777}"_guid, "474dd585-2d21-4a05-b178-374cad2f1579"_guid),
				CameraProperties(7, "{3B85E0D6-34B1-4AF6-947C-08DFE9A22819}"_guid, "356af686-78eb-4ffd-ba61-5cb0d2a3aa54"_guid),
				CameraProperties(8, "{C2D7D204-D79D-492F-924B-D4751B37F866}"_guid, "72d20257-2448-46bc-9874-31f056015f46"_guid),
				CameraProperties(9, "{9C8C5DD0-7B9B-4554-AC81-6DC3867459C1}"_guid, "aea0e882-2f69-48c4-b4b1-99c835288f7b"_guid),
				CameraProperties(10, "{7B8054B5-DEE1-4E10-82C0-C0507ED5B614}"_guid, "c750ba70-44ef-4189-9ce8-9c53c60fa159"_guid),
				CameraProperties(11, "78176397-7d42-4202-9152-e4b99833d5d8"_guid, "ef46221e-9f91-41ce-aec4-1fd84ea4c358"_guid),
				CameraProperties(12, "f434b2fb-90f2-48cc-9528-9b64c1c83c7a"_guid, "2b48d566-4deb-4ecd-ad08-e9dc3dbc3f2a"_guid),
				CameraProperties(13, "28130a9f-779e-4e62-b73f-c811fa3f6e77"_guid, "494fea24-0b60-4833-b5b9-26354f1403a0"_guid),
				CameraProperties(14, "ffd686af-c899-4cc8-9a7e-f72bb0dd035d"_guid, "c9cba795-ccf2-4684-9857-6e3b615048a6"_guid),
				CameraProperties(15, "e437d9a5-ab16-4432-b637-ae5e5eca5bd6"_guid, "64d5824d-c357-43c0-95a6-7391f8d09a22"_guid),
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{Entity::ComponentTypeFlags(), "53d6094a-ac64-c162-1034-e0c8d0f5544a"_asset, {}},
				Entity::IndicatorTypeExtension{
					"c6742677-54b6-41ae-8b39-495721b423e2"_guid,
					Guid{},
					EnumFlags<Entity::IndicatorTypeExtension::Flags>{Entity::IndicatorTypeExtension::Flags::RequiresGhost}
				}
			}
		);
	};

#undef CameraProperties
}
