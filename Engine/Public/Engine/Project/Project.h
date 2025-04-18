#pragma once

#include <Engine/DataSource/PropertySourceInterface.h>
#include <Engine/DataSource/PropertySourceIdentifier.h>

#include <Common/Project System/ProjectInfo.h>
#include <Common/System/SystemType.h>
#include <Common/Storage/Identifier.h>

namespace ngine
{
	struct Project final : public PropertySource::Interface
	{
		inline static constexpr System::Type SystemType = System::Type::Project;
		inline static constexpr Guid DataSourceGuid = "DECE5BDA-71F2-4966-B690-B250FE528D82"_guid;

		Project();
		virtual ~Project();

		[[nodiscard]] bool IsValid() const
		{
			return m_info.IsValid();
		}
		[[nodiscard]] Guid GetGuid() const
		{
			return m_info.GetGuid();
		}

		[[nodiscard]] Settings& GetSettings()
		{
			return m_info.GetSettings();
		}
		[[nodiscard]] const Settings& GetSettings() const
		{
			return m_info.GetSettings();
		}

		void SetName(UnicodeString&& name);
		void SetDescription(UnicodeString&& description);
		void SetThumbnail(const Asset::Guid thumbnailGuid);

		void SetInfo(ProjectInfo&& info);

		[[nodiscard]] Optional<ProjectInfo*> GetInfo()
		{
			return {m_info, IsValid()};
		}
		[[nodiscard]] Optional<const ProjectInfo*> GetInfo() const
		{
			return {m_info, IsValid()};
		}

		[[nodiscard]] Guid GetSessionGuid() const
		{
			return m_sessionGuid;
		}

		// PropertySource::Interface
		[[nodiscard]] virtual PropertyValue GetDataProperty(const PropertySource::PropertyIdentifier identifier) const override;
		// ~PropertySource::Interface
	protected:
		ProjectInfo m_info;
		Guid m_sessionGuid{Guid::Generate()};

		PropertySource::PropertyIdentifier m_namePropertyIdentifier;
		PropertySource::PropertyIdentifier m_descriptionPropertyIdentifier;
		PropertySource::PropertyIdentifier m_thumbnailPropertyIdentifier;
	};
}
