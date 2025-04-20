#pragma once

#include <Widgets/Documents/DocumentWidget.h>
#include <Renderer/Scene/SceneView2D.h>
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
	struct Scene2D : public Document::Widget, public Rendering::SceneViewDrawer
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

		Scene2D(Initializer&& initializer);
		Scene2D(const Deserializer& initializer);
		virtual ~Scene2D();

		Scene2D(const Scene2D&) = delete;
		Scene2D& operator=(const Scene2D&) = delete;
		Scene2D(Scene2D&& other) = delete;
		Scene2D& operator=(Scene2D&&) = delete;

		void OnCreated();

		// Widget
		virtual void OnEnableFramegraph() override;
		virtual void OnDisableFramegraph() override;
		virtual void OnContentAreaChanged(const EnumFlags<ContentAreaChangeFlags> changeFlags) override;

		virtual Optional<Input::Monitor*> GetFocusedInputMonitorAtCoordinates(const WindowCoordinate) override;
		virtual Optional<Input::Monitor*> GetFocusedInputMonitor() override;

		virtual void OnBecomeVisible() override;
		virtual void OnBecomeHidden() override;
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
			return (Math::Vector2ui)GetContentArea().GetSize();
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

		virtual Threading::JobBatch OpenDocumentAssetInternal(
			const DocumentData& document,
			const Serialization::Data& assetData,
			const Asset::DatabaseEntry& assetEntry,
			const EnumFlags<OpenDocumentFlags> openDocumentFlags
		) override;
		virtual void CloseDocument() override;
		// ~Document::Widget

		void AssignScene(UniquePtr<ngine::Scene2D>&& pScene);
		void AssignScene(ngine::Scene2D& scene);
		void UnloadScene();

		Rendering::SceneView2D& GetSceneView()
		{
			return m_sceneView;
		}
		const Rendering::SceneView2D& GetSceneView() const
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
		void OnSceneFinishedLoading(ngine::Scene2D& scene);
	protected:
		virtual void OnSceneChanged()
		{
		}
		void OnSceneUnloadedInternal();
		virtual void OnSceneUnloaded(ngine::Scene2D&)
		{
		}
		virtual void OnSceneEditingChanged()
		{
		}
	protected:
		Rendering::SceneView2D m_sceneView;
		EnumFlags<Flags> m_flags;
	};

	ENUM_FLAG_OPERATORS(Scene2D::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Document::Scene2D>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Document::Scene2D>(
			"{EBFBF2F0-A923-4EF4-A0BF-ADF43584B174}"_guid,
			MAKE_UNICODE_LITERAL("2D Scene Document"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning
		);
	};
}
