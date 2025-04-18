#pragma once

#include <Engine/Entity/Component3D.h>

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Math/Primitives/Spline.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Threading/Mutexes/SharedMutex.h>

namespace ngine::Entity
{
	struct SplineComponent : public Component3D
	{
		static constexpr Guid TypeGuid = "304ec37e-bce4-48b6-9c2e-6108738270ff"_guid;

		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using SplineType = Math::Splinef;
		using ConstViewType = SplineType::ConstView;
		using ViewType = SplineType::View;

		SplineComponent(const SplineComponent& templateComponent, const Cloner& cloner);
		SplineComponent(const Deserializer& deserializer);
		SplineComponent(Initializer&& initializer);

		bool SerializeCustomData(Serialization::Writer) const;

		void EmplacePoint(const Math::Vector3f relativeCoordinate, const Math::Vector3f relativeUpDirection)
		{
			{
				Threading::UniqueLock lock(m_splineMutex);
				Assert(!(m_spline.GetPoints().GetLastElement().position - relativeCoordinate).IsZero());
				m_spline.EmplacePoint(relativeCoordinate, relativeUpDirection);
			}
			OnChanged();
		}

		void InsertPoint(const uint32 index, const Math::Vector3f relativeCoordinate, const Math::Vector3f relativeUpDirection)
		{
			{
				Threading::UniqueLock lock(m_splineMutex);
				m_spline.InsertPoint(m_spline.GetPoints().begin() + index, relativeCoordinate, relativeUpDirection);
			}
			OnChanged();
		}

		void RemovePoint(const uint32 index)
		{
			{
				Threading::UniqueLock lock(m_splineMutex);
				Assert(m_spline.GetPointCount() > 2);

				m_spline.RemovePoint(m_spline.GetPoints().begin() + index);
			}

			OnChanged();
		}

		void UpdatePoint(const uint32 index, const Math::Vector3f relativeCoordinate, const Math::Vector3f relativeUpDirection)
		{
			{
				Threading::UniqueLock lock(m_splineMutex);
				Assert(index < m_spline.GetPointCount());
				if (index > m_spline.GetPointCount())
				{
					return;
				}

				m_spline.UpdatePoint(m_spline.GetPoints().begin() + index, relativeCoordinate, relativeUpDirection);
			}

			OnChanged();
		}

		void UpdateLastPoint(const Math::Vector3f relativeCoordinate, const Math::Vector3f relativeUpDirection)
		{
			{
				Threading::UniqueLock lock(m_splineMutex);
				m_spline.UpdateLastPoint(relativeCoordinate, relativeUpDirection);
			}

			OnChanged();
		}

		void SetClosed(bool isClosed)
		{
			{
				Threading::UniqueLock lock(m_splineMutex);
				m_spline.SetClosed(isClosed);
			}

			OnChanged();
		}

		template<typename Callback>
		void ModifySpline(Callback&& callback)
		{
			Threading::UniqueLock lock(m_splineMutex);
			if (callback(m_spline))
			{
				lock.Unlock();
				OnChanged();
			}
		}
		template<typename Callback>
		auto VisitSpline(Callback&& callback) const
		{
			Threading::SharedLock lock(m_splineMutex);
			return callback(m_spline);
		}

		[[nodiscard]] uint32 GetPointCount() const
		{
			Threading::SharedLock lock(m_splineMutex);
			return m_spline.GetPointCount();
		}

		[[nodiscard]] bool IsDefaultSpline() const
		{
			Threading::SharedLock lock(m_splineMutex);
			return m_spline.GetPointCount() == 2 && m_spline.GetPoints()[0].position.IsEquivalentTo(Math::Zero) &&
			       m_spline.GetPoints()[1].position.IsEquivalentTo(Math::Forward);
		}
		[[nodiscard]] bool IsClosed() const
		{
			Threading::SharedLock lock(m_splineMutex);
			return m_spline.IsClosed();
		}

		[[nodiscard]] float CalculateSplineLength(const uint32 numBezierSubdivisions = 32) const
		{
			Threading::SharedLock lock(m_splineMutex);
			return m_spline.CalculateSplineLength(numBezierSubdivisions);
		}

		ThreadSafe::Event<void(void*), 24> OnChanged;
	protected:
		SplineComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer);
	protected:
		mutable Threading::SharedMutex m_splineMutex;
		SplineType m_spline;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::SplineComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::SplineComponent>(
			Entity::SplineComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Spline"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "53d6094a-ac64-c162-1034-e0c8d0f5544a"_asset, "5fc365eb-e4ae-4d1b-aaa3-4d4a66d5ab69"_guid
				},
				Entity::IndicatorTypeExtension{"47a649aa-fcde-4b0b-8f3b-928f1b26a47f"_guid, "8f644d4c-7ec9-f9b5-7d22-d751d9b94b85"_asset}
			}
		);
	};
}
