#pragma once

#include "AudioSystem.h"

#if PLATFORM_WEB
typedef struct ALCcontext_struct ALCcontext;
typedef struct ALCdevice_struct ALCdevice;
#else
struct ALCdevice;
struct ALCcontext;
#endif

namespace ngine::Audio
{
	struct OpenALSystem final : public SystemInterface
	{
	public:
		OpenALSystem() = default;
		OpenALSystem(const OpenALSystem&) = delete;
		OpenALSystem(const OpenALSystem&&) = delete;

		[[nodiscard]] virtual InitializationResult Initialize() override;
		virtual void Shutdown() override;

		[[nodiscard]] virtual ConstStringView GetDeviceName() const override;
	private:
		[[nodiscard]] ALCdevice* GetDevice() const;
	private:
		ALCdevice* m_pDevice{nullptr};
		ALCcontext* m_pContext{nullptr};
	};
}
