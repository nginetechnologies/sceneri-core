#pragma once

#include <Widgets/Documents/DocumentWidget.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneViewDrawer.h>

#include <Common/EnumFlags.h>

namespace ngine
{
	namespace Rendering
	{
		struct ToolWindow;
		struct Renderer;
	}
}

namespace ngine::Widgets::Document
{
	struct Scene3D : public Document::Widget, public Rendering::SceneViewDrawer
	{
		using InstanceIdentifier = TIdentifier<uint32, PLATFORM_DESKTOP || PLATFORM_APPLE_MACCATALYST || PLATFORM_WEB ? 3 : 1>;
		using BaseType = Document::Widget;

		enum class Flags : uint8
		{
			IsSceneEditable = 1 << 0,
			HasSceneFinishedLoading = 1 << 1,
			IsSceneOwned = 1 << 2
		};

		struct Initializer : public Widget::Initializer
		{
			using BaseType = Widget::Initializer;

			Initializer(BaseType&& initializer, const EnumFlags<Flags> flags = {})
				: BaseType(Forward<BaseType>(initializer))
				, m_sceneWidgetFlags(flags)
			{
			}

			EnumFlags<Flags> m_sceneWidgetFlags;
		};

		Scene3D(Initializer&& initializer);
		Scene3D(const Deserializer& initializer);
		virtual ~Scene3D();

		Scene3D(const Scene3D&) = delete;
		Scene3D& operator=(const Scene3D&) = delete;
		Scene3D(Scene3D&& other) = delete;
		Scene3D& operator=(Scene3D&&) = delete;

		void OnCreated();

		// Widget
		virtual void OnEnableFramegraph() override;
		virtual void OnDisableFramegraph() override;

		virtual void OnBeforeContentAreaChanged(const EnumFlags<ContentAreaChangeFlags> changeFlags) override;
		virtual void OnContentAreaChanged(const EnumFlags<ContentAreaChangeFlags> changeFlags) override;

		virtual Optional<Input::Monitor*> GetFocusedInputMonitorAtCoordinates(const WindowCoordinate) override;
		virtual Optional<Input::Monitor*> GetFocusedInputMonitor() override;

		virtual void OnBecomeVisible() override;
		virtual void OnBecomeHidden() override;
		virtual void OnSwitchToForeground() override;
		virtual void OnSwitchToBackground() override;
		// ~Widget

		// SceneViewDrawer
		[[nodiscard]] virtual Rendering::Framegraph& GetFramegraph() override final;
		virtual void DoRenderPass(
			const Rendering::CommandEncoderView commandEncoder,
			const Rendering::RenderPassView renderPass,
			const Rendering::FramebufferView framebuffer,
			const Math::Rectangleui extent,
			const ArrayView<const Rendering::ClearValue, uint8> clearValues,
			RenderPassCallback&& callback,
			const uint32 maximumPushConstantInstanceCount
		) override final;
		virtual void DoComputePass(ComputePassCallback&& callback) override final;

		[[nodiscard]] virtual Math::Vector2ui GetRenderResolution() const override final
		{
#if SPLIT_SCREEN_TEST
			return Math::Vector2ui{(uint32)GetContentArea().GetSize().x / 2u, (uint32)GetContentArea().GetSize().y};
#else
			return (Math::Vector2ui)GetContentArea().GetSize();
#endif
		}
		[[nodiscard]] virtual Math::Rectangleui GetFullRenderArea() const override
		{
			return Math::Rectangleui{Math::Zero, (Math::Vector2ui)GetContentArea().GetSize()};
		}
		[[nodiscard]] virtual Math::Matrix4x4f CreateProjectionMatrix(
			const Math::Anglef fieldOfView, const Math::Vector2f renderResolution, const Math::Lengthf nearPlane, const Math::Lengthf farPlane
		) const override;
		// ~SceneViewDrawer

		// Document::Widget
		[[nodiscard]] virtual ArrayView<const Guid, uint16> GetSupportedDocumentAssetTypeGuids() const override;

		[[nodiscard]] virtual ngine::DataSource::PropertyValue GetDataProperty(const ngine::DataSource::PropertyIdentifier identifier
		) const override;

		virtual Threading::JobBatch OpenDocumentAssetInternal(
			const DocumentData& document,
			const Serialization::Data& assetData,
			const Asset::DatabaseEntry& assetEntry,
			const EnumFlags<OpenDocumentFlags> openDocumentFlags
		) override;
		virtual void CloseDocument() override;
		// ~Document::Widget

		void AssignScene(UniquePtr<ngine::Scene3D>&& pScene);
		void AssignScene(ngine::Scene3D& scene);
		void UnloadScene();

		Rendering::SceneView& GetSceneView()
		{
			return m_sceneView;
		}
		const Rendering::SceneView& GetSceneView() const
		{
			return m_sceneView;
		}

		[[nodiscard]] bool IsSceneEditable() const
		{
			return m_flags.IsSet(Flags::IsSceneEditable);
		}
		void AllowSceneEditing()
		{
			m_flags |= Flags::IsSceneEditable;

			OnSceneEditingChanged();
		}
		void DisallowSceneEditing()
		{
			m_flags.Clear(Flags::IsSceneEditable);

			OnSceneEditingChanged();
		}

		[[nodiscard]] bool HasSceneFinishedLoading() const
		{
			return m_flags.IsSet(Flags::HasSceneFinishedLoading);
		}
		void OnSceneFinishedLoading(ngine::Scene3D& scene);
	protected:
		virtual void OnSceneChanged()
		{
		}
		void OnSceneUnloadedInternal();
		virtual void OnSceneUnloaded(ngine::Scene3D&)
		{
		}
		virtual void OnSceneEditingChanged()
		{
		}
	protected:
		Rendering::SceneView m_sceneView;
		EnumFlags<Flags> m_flags;
	};

	ENUM_FLAG_OPERATORS(Scene3D::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Document::Scene3D>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Document::Scene3D>(
			"91534ee3-8462-48c5-8c28-985e490deda8"_guid,
			MAKE_UNICODE_LITERAL("Scene3D Document"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning
		);
	};
}
