#pragma once

#include "ModalTrigger.h"

namespace ngine::DataSource
{
	struct Dynamic;
}

namespace ngine::PropertySource
{
	struct Dynamic;
}

namespace ngine::Widgets::Data
{
	struct ModalFocusListener;

	struct ContextMenu : public ModalTrigger
	{
		using BaseType = ModalTrigger;
		using InstanceIdentifier = TIdentifier<uint32, 3>;

		struct Initializer : public ModalTrigger::Initializer
		{
			using BaseType = ModalTrigger::Initializer;

			Initializer(
				BaseType&& baseInitializer,
				const Optional<ngine::DataSource::Dynamic*> pDataSource,
				const Optional<ngine::PropertySource::Dynamic*> pPropertySource,
				const Asset::Guid entryAsset
			)
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_pDataSource(pDataSource)
				, m_pPropertySource(pPropertySource)
				, m_entryAsset(entryAsset)
			{
			}

			Optional<ngine::DataSource::Dynamic*> m_pDataSource;
			Optional<ngine::PropertySource::Dynamic*> m_pPropertySource;
			Asset::Guid m_entryAsset;
		};

		ContextMenu(Initializer&& initializer);
		virtual ~ContextMenu() = default;

		virtual void OnSpawnedModal(Widget& owner, Widget& modalWidget) override;
		virtual void OnModalClosed(Widget& owner, Widget& modalWidget) override;
	protected:
		Optional<ngine::DataSource::Dynamic*> m_pDataSource;
		Optional<ngine::PropertySource::Dynamic*> m_pPropertySource;
		Asset::Guid m_entryAsset;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::ContextMenu>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::ContextMenu>(
			"EE44678E-98C3-4386-9F75-358B010CC64C"_guid,
			MAKE_UNICODE_LITERAL("Context Menu"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning
		);
	};
}
