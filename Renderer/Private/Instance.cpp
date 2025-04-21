#include "Instance.h"

#include <Renderer/Constants.h>
#include <Renderer/Vulkan/Includes.h>
#include <Renderer/WebGPU/Includes.h>

#include <Common/Platform/CompilerWarnings.h>

#if RENDERER_VULKAN
#include <3rdparty/vulkan/vk_layer_settings_ext.h>

#if PLATFORM_WINDOWS
#include <Common/Platform/Windows.h>
#include <3rdparty/vulkan/vulkan_win32.h>
#elif PLATFORM_ANDROID
#include <3rdparty/vulkan/vulkan_android.h>
#elif PLATFORM_LINUX
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_syswm.h>

#undef DestroyAll
#undef None
#undef Status
#undef False
#undef ControlMask
#undef Bool
#undef Success

#if defined(SDL_VIDEO_DRIVER_X11)
#include <3rdparty/vulkan/vulkan_xlib.h>
#endif

#if defined(SDL_VIDEO_DRIVER_WAYLAND)
#include <3rdparty/vulkan/vulkan_wayland.h>
#endif

#endif

#endif

#if RENDERER_WEBGPU_DAWN

PUSH_CLANG_WARNINGS
DISABLE_CLANG_WARNING("-Wshadow-field-in-constructor")

#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>

POP_CLANG_WARNINGS

#endif

#if RENDERER_WEBGPU
#include <Renderer/Window/Window.h>
#endif

#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/ZeroTerminatedStringView.h>
#include <Common/Version.h>
#include <Common/EnumFlags.h>
#include <Common/IO/Log.h>

namespace ngine::Rendering
{
#if RENDERER_VULKAN
	inline static constexpr Array IgnoredErrors{// Ignore new error saying to use VK_EXT_graphics_pipeline_library (for now)
	                                            (int32)0xc5b8a05c
	};

	VkBool32 VKAPI_CALL ValidationLayerCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		[[maybe_unused]] void* pUserData
	)
	{
		if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			if (!IgnoredErrors.GetView().Contains(pCallbackData->messageIdNumber))
			{
				LogError("(Vulkan validation layer): {}", pCallbackData->pMessage);
				BreakIfDebuggerIsAttached();
			}
		}
		else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			LogWarning("(Vulkan validation layer): {}", pCallbackData->pMessage);
		}
		else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		{
			LogMessage("(Vulkan validation layer): {}", pCallbackData->pMessage);
		}

		return VK_FALSE;
	}
#endif

#if RENDERER_WEBGPU
	[[nodiscard]] InstanceView CreateInstance()
	{
		InstanceView instanceView;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[&instanceView]()
			{
				instanceView = wgpuCreateInstance(nullptr);
#if RENDERER_WEBGPU_DAWN
				wgpuInstanceAddRef(instanceView);
#else
				wgpuInstanceReference(instanceView);
#endif
			}
		);
		return instanceView;
	}
#endif

	Instance::Instance(
		[[maybe_unused]] const ConstZeroTerminatedStringView applicationName,
		[[maybe_unused]] const Version applicationVersion,
		[[maybe_unused]] const ConstZeroTerminatedStringView engineName,
		[[maybe_unused]] const Version engineVersion,
		[[maybe_unused]] const EnumFlags<CreationFlags> creationFlags
	)
#if RENDERER_WEBGPU
		: InstanceView(CreateInstance())
#endif
	{
#if RENDERER_VULKAN
		{
			uint32 supportedVulkanVersion{VK_API_VERSION_1_1};

			PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersionFunction =
				reinterpret_cast<PFN_vkEnumerateInstanceVersion>((void*)vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
			if (vkEnumerateInstanceVersionFunction != nullptr)
			{
				vkEnumerateInstanceVersionFunction(&supportedVulkanVersion);
			}
			supportedVulkanVersion = Math::Min(supportedVulkanVersion, VK_API_VERSION_1_2);

			const VkApplicationInfo appInfo = {
				VK_STRUCTURE_TYPE_APPLICATION_INFO,
				nullptr,
				applicationName,
				static_cast<uint32>(VK_MAKE_VERSION(applicationVersion.GetMajor(), applicationVersion.GetMinor(), applicationVersion.GetPatch())),
				engineName,
				static_cast<uint32>(VK_MAKE_VERSION(engineVersion.GetMajor(), engineVersion.GetMinor(), engineVersion.GetPatch())),
				supportedVulkanVersion
			};

			uint32 supportedExtensionCount;
			vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, nullptr);
			FixedSizeVector<VkExtensionProperties, uint16>
				supportedExtensions(Memory::ConstructWithSize, Memory::Uninitialized, (uint16)supportedExtensionCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, supportedExtensions.GetData());

			constexpr uint8 MaximumVulkanInstanceExtensionCount = 16;
			FlatVector<String, MaximumVulkanInstanceExtensionCount> enabledExtensions;
			enabledExtensions.EmplaceBack(VK_KHR_SURFACE_EXTENSION_NAME);

#if PLATFORM_WINDOWS
			enabledExtensions.EmplaceBack(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif PLATFORM_ANDROID
			enabledExtensions.EmplaceBack(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif PLATFORM_LINUX
			if (supportedExtensions.ContainsIf(
						[](const VkExtensionProperties& extension) -> bool
						{
							return ConstStringView(extension.extensionName, (uint32)strlen(extension.extensionName)) ==
				             VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
						}
					))
			{
				enabledExtensions.EmplaceBack(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
			}
			if (supportedExtensions.ContainsIf(
						[](const VkExtensionProperties& extension) -> bool
						{
							return ConstStringView(extension.extensionName, (uint32)strlen(extension.extensionName)) ==
				             VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
						}
					))
			{
				enabledExtensions.EmplaceBack(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
			}
#endif

			constexpr Array<ConstStringView, 1> desiredValidationLayers = {"VK_LAYER_KHRONOS_validation"};
			FlatVector<const char*, desiredValidationLayers.GetSize()> validationLayers;

			if (creationFlags.IsSet(CreationFlags::Validation))
			{
				if (supportedExtensions.ContainsIf(
							[](const VkExtensionProperties& extension) -> bool
							{
								return ConstStringView(extension.extensionName, (uint32)strlen(extension.extensionName)) ==
					             VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
							}
						))
				{
					enabledExtensions.EmplaceBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

					uint32 layerCount;
					vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

					static constexpr uint8 MaximumAvailableLayerCount = 255;
					Assert(layerCount <= MaximumAvailableLayerCount);
					layerCount = Math::Min(layerCount, MaximumAvailableLayerCount);

					FixedSizeFlatVector<VkLayerProperties, MaximumAvailableLayerCount>
						availableLayers(Memory::ConstructWithSize, Memory::Uninitialized, (uint8)layerCount);
					Assert(layerCount <= availableLayers.GetTheoreticalCapacity());

					vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.GetData());

					for (const ConstStringView& desiredValidationLayer : desiredValidationLayers)
					{
						const OptionalIterator<VkLayerProperties> availableLayer = availableLayers.FindIf(
							[desiredValidationLayer](const VkLayerProperties& layerProperties)
							{
								return desiredValidationLayer ==
							         ConstStringView(layerProperties.layerName, static_cast<StringView::SizeType>(strlen(layerProperties.layerName)));
							}
						);

						if (availableLayer.IsValid())
						{
							validationLayers.EmplaceBack(desiredValidationLayer.GetData());
						}
					}
				}
			}

			if constexpr (ENABLE_NVIDIA_GPU_CHECKPOINTS)
			{
				if (supportedExtensions.ContainsIf(
							[](const VkExtensionProperties& extension) -> bool
							{
								return ConstStringView(extension.extensionName, (uint32)strlen(extension.extensionName)) ==
					             VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
							}
						))
				{
					enabledExtensions.EmplaceBack(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
				}
			}

			FixedSizeFlatVector<const char*, MaximumVulkanInstanceExtensionCount>
				enabledExtensionsMemory(Memory::ConstructWithSize, Memory::Uninitialized, enabledExtensions.GetSize());
			for (decltype(enabledExtensions)::ConstPointerType it = enabledExtensions.begin(), end = enabledExtensions.end(); it != end; ++it)
			{
				const uint8 index = enabledExtensions.GetIteratorIndex(it);
				enabledExtensionsMemory[index] = it->GetData();
			}

			void* pNext = nullptr;

			constexpr ConstZeroTerminatedStringView enableQueueSubmitSyncValidation{
				"VALIDATION_CHECK_ENABLE_SYNCHRONIZATION_VALIDATION_QUEUE_SUBMIT"
			};
			VkLayerSettingValueDataEXT enableQueueSubmitSyncValidationValue;
			enableQueueSubmitSyncValidationValue.arrayString =
				array_char{enableQueueSubmitSyncValidation.GetData(), enableQueueSubmitSyncValidation.GetSize()};

			Array layerSettingValues{
				VkLayerSettingValueEXT{"enables", VK_LAYER_SETTING_VALUE_TYPE_STRING_ARRAY_EXT, enableQueueSubmitSyncValidationValue}
			};
			VkLayerSettingsEXT
				layerSettings{VK_STRUCTURE_TYPE_INSTANCE_LAYER_SETTINGS_EXT, nullptr, layerSettingValues.GetSize(), layerSettingValues.GetData()};

			Array<VkValidationFeatureEnableEXT, 3> validationFeatureEnables{
				VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
				VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
				VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT
			};
			VkValidationFeaturesEXT validationFeatures{
				VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
				&layerSettings,
				validationFeatureEnables.GetSize(),
				validationFeatureEnables.GetData(),
				0,
				nullptr
			};
			if (creationFlags.IsSet(CreationFlags::ExtendedValidation))
			{
				pNext = &validationFeatures;
			}

			const VkInstanceCreateInfo createInfo = {
				VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				pNext,
				0,
				&appInfo,
				validationLayers.GetSize(),
				validationLayers.GetData(),
				enabledExtensionsMemory.GetSize(),
				enabledExtensionsMemory.GetData()
			};

			[[maybe_unused]] const bool success = vkCreateInstance(&createInfo, nullptr, &m_pInstance) == VK_SUCCESS;
			Assert(success, "Failed to create Vulkan instance");
		}

		if (creationFlags.IsSet(CreationFlags::Validation))
		{
			const VkDebugUtilsMessengerCreateInfoEXT createInfo = {
				VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				nullptr,
				0,
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
				&ValidationLayerCallback,
				this
			};

			PFN_vkCreateDebugUtilsMessengerEXT func =
				reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>((void*)vkGetInstanceProcAddr(m_pInstance, "vkCreateDebugUtilsMessengerEXT"));
			if (func != nullptr)
			{
				func(m_pInstance, &createInfo, nullptr, &m_pDebugMessenger);
			}
		}
#endif
	}

	Instance::~Instance()
	{
#if RENDERER_VULKAN
		if (LIKELY(m_pInstance != nullptr))
		{
			if (m_pDebugMessenger != nullptr)
			{
				PFN_vkDestroyDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
					(void*)vkGetInstanceProcAddr(m_pInstance, "vkDestroyDebugUtilsMessengerEXT")
				);
				func(m_pInstance, m_pDebugMessenger, nullptr);
			}

			vkDestroyInstance(m_pInstance, nullptr);
		}
#elif RENDERER_WEBGPU
		Rendering::Window::QueueOnWindowThread(
			[pInstance = m_pInstance]()
			{
				wgpuInstanceRelease(pInstance);
			}
		);
#endif
	}
}
