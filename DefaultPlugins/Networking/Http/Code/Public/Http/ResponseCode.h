#pragma once

namespace ngine::Networking::HTTP
{
	enum class ResponseCodeType : long
	{
		Ok = 200,
		Created = 201,
		Accepted = 202,
		BadRequest = 400,
		Unauthorized = 401,
		Forbidden = 403,
		NotFound = 404
	};

	struct ResponseCode
	{
		[[nodiscard]] bool IsConnectionFailure() const
		{
			return m_code == 0;
		}

		[[nodiscard]] bool IsInformational() const
		{
			return (m_code >= 100) & (m_code <= 199);
		}

		[[nodiscard]] bool IsSuccessful() const
		{
			return (m_code >= 200) & (m_code <= 299);
		}

		[[nodiscard]] bool IsRedirect() const
		{
			return (m_code >= 300) & (m_code <= 399);
		}

		[[nodiscard]] bool IsClientError() const
		{
			return (m_code >= 400) & (m_code <= 499);
		}

		[[nodiscard]] bool IsServerError() const
		{
			return (m_code >= 500) & (m_code <= 599);
		}

		[[nodiscard]] bool IsError() const
		{
			return IsClientError() | IsServerError();
		}

		[[nodiscard]] bool operator==(const ResponseCodeType type) const
		{
			return GetType() == type;
		}

		[[nodiscard]] ResponseCodeType GetType() const
		{
			return static_cast<ResponseCodeType>(m_code);
		}

		long m_code{0};
	};
}
