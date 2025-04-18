#include "Project/Project.h"

#include <Engine/DataSource/DataSourceCache.h>

#include <Common/System/Query.h>
#include <Engine/DataSource/PropertyValue.h>

namespace ngine
{
	Project::Project()
		: Interface(System::Get<DataSource::Cache>().GetPropertySourceCache().FindOrRegister(DataSourceGuid))
	{
		System::Query::GetInstance().RegisterSystem(*this);

		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.OnCreated(m_identifier, *this);

		m_namePropertyIdentifier = dataSourceCache.RegisterProperty("project_name");
		m_descriptionPropertyIdentifier = dataSourceCache.RegisterProperty("project_description");
		m_thumbnailPropertyIdentifier = dataSourceCache.RegisterProperty("project_thumbnail");
	}

	Project::~Project()
	{
		System::Query::GetInstance().DeregisterSystem<Project>();

		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		const PropertySource::Identifier propertySourceIdentifier = propertySourceCache.Find(DataSourceGuid);
		propertySourceCache.Deregister(propertySourceIdentifier, propertySourceCache.FindGuid(m_identifier));

		dataSourceCache.DeregisterProperty(m_namePropertyIdentifier, "project_name");
		dataSourceCache.DeregisterProperty(m_descriptionPropertyIdentifier, "project_description");
		dataSourceCache.DeregisterProperty(m_thumbnailPropertyIdentifier, "project_thumbnail");
	}

	void Project::SetName(UnicodeString&& name)
	{
		m_info.SetName(Forward<UnicodeString>(name));
		OnDataChanged();
	}
	void Project::SetDescription(UnicodeString&& description)
	{
		m_info.SetDescription(Forward<UnicodeString>(description));
		OnDataChanged();
	}
	void Project::SetThumbnail(const Asset::Guid thumbnailGuid)
	{
		m_info.SetThumbnail(thumbnailGuid);
		OnDataChanged();
	}

	void Project::SetInfo(ProjectInfo&& info)
	{
		m_info = Forward<ProjectInfo>(info);
		OnDataChanged();
	}

	PropertySource::PropertyValue Project::GetDataProperty(const PropertySource::PropertyIdentifier propertyIdentifier) const
	{
		if (propertyIdentifier == m_namePropertyIdentifier)
		{
			return m_info.GetName();
		}
		else if (propertyIdentifier == m_descriptionPropertyIdentifier)
		{
			return m_info.GetDescription();
		}
		else if (propertyIdentifier == m_thumbnailPropertyIdentifier)
		{
			return m_info.GetThumbnailGuid();
		}

		return {};
	}
}
