#include "AssetCompilerCore/AssetCompilers/SceneObjectCompiler.h"
#include "AssetCompilerCore/AssetCompilers/Textures/GenericTextureCompiler.h"

#include <Common/Memory/New.h>
#include <Common/Assert/Assert.h>
#include <Common/Platform/CompilerWarnings.h>

#include <Common/Asset/Asset.h>
#include <Common/Asset/AssetOwners.h>
#include <Common/Asset/AssetDatabase.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Move.h>
#include <Common/Memory/AddressOf.h>
#include <Common/IO/Path.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/Serialization/Serialize.h>
#include <Common/Memory/Containers/Serialization/Vector.h>
#include <Common/Memory/Containers/Serialization/FixedArrayView.h>
#include <Common/Serialization/Guid.h>
#include <Common/Math/IsNegative.h>
#include <Common/Math/Hash.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/SharedPtr.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Memory/Containers/Serialization/UnorderedMap.h>
#include <Common/Memory/CountBits.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Reflection/Serialization/Type.h>

#include <AssetCompilerCore/Plugin.h>
#include <PhysicsCore/Plugin.h>
#include <PhysicsCore/MaterialCache.h>

#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/StaticMeshComponent.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Scene/Scene3DAssetType.h>
#include <Engine/Entity/Lights/PointLightComponent.h>
#include <Engine/Entity/Lights/DirectionalLightComponent.h>
#include <Engine/Entity/Lights/SpotLightComponent.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/CameraProperties.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Threading/Jobs/AsyncJob.h>

#include <Renderer/Assets/Defaults.h>
#include <Renderer/Assets/StaticMesh/StaticObject.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>
#include <Renderer/Assets/StaticMesh/MeshSceneTag.h>
#include <Renderer/Assets/StaticMesh/VertexColors.h>
#include <Renderer/Assets/Material/MaterialInstanceAssetType.h>
#include <Renderer/Assets/Material/MaterialInstanceAsset.h>
#include <Renderer/Assets/Material/MaterialAsset.h>
#include <Renderer/Assets/Texture/TextureAsset.h>
#include <Renderer/Assets/Texture/TextureAssetType.h>
#include <Renderer/Assets/StaticMesh/VertexTangents.h>

#include <Common/Math/Tangents.h>
#include <Common/System/Query.h>

#include <PhysicsCore/Components/Data/BodyComponent.h>
#include <PhysicsCore/Components/BoxColliderComponent.h>
#include <PhysicsCore/Components/CapsuleColliderComponent.h>
#include <PhysicsCore/Components/SphereColliderComponent.h>
#include <PhysicsCore/Components/PlaneColliderComponent.h>
#include <PhysicsCore/Components/MeshColliderComponent.h>
#include <PhysicsCore/DefaultMaterials.h>

#include <Animation/Skeleton.h>
#include <Animation/SkeletonAssetType.h>
#include <Animation/MeshSkin.h>
#include <Animation/MeshSkinAssetType.h>
#include <Animation/Components/SkinnedMeshComponent.h>
#include <Animation/Components/SkeletonComponent.h>
#include <Animation/Animation.h>
#include <Animation/AnimationAssetType.h>
#include <Animation/Components/Controllers/LoopingAnimationController.h>
#include <Animation/3rdparty/ozz/base/maths/simd_math.h>
#include <Animation/3rdparty/ozz/base/maths/soa_transform.h>

#if HAS_FBX_SDK
#include <Animation/3rdparty/ozz/animation/offline/fbx/fbx.h>
#include <Animation/3rdparty/ozz/animation/offline/fbx/fbx_animation.h>
#include <Animation/3rdparty/ozz/animation/offline/animation_builder.h>
#endif

#define OZZ_INCLUDE_PRIVATE_HEADER 1
#include <Animation/3rdparty/ozz/animation/runtime/animation_keyframe.h>

#if SUPPORT_ASSIMP
PUSH_CLANG_WARNINGS
DISABLE_CLANG_WARNING("-Wdeprecated-dynamic-exception-spec");
DISABLE_CLANG_WARNING("-Wimplicit-float-conversion");

PUSH_MSVC_WARNINGS
DISABLE_MSVC_WARNINGS(5267 4296)

#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

POP_MSVC_WARNINGS
#include <assimp/IOSystem.hpp>
POP_CLANG_WARNINGS
#endif

PUSH_MSVC_WARNINGS
DISABLE_MSVC_WARNINGS(5219 4619 5266)

PUSH_CLANG_WARNINGS
DISABLE_CLANG_WARNING("-Wnan-infinity-disabled")
#include <EditorCommon/3rdparty/gli/gli.h>

POP_CLANG_WARNINGS
POP_MSVC_WARNINGS

PUSH_CLANG_WARNINGS
DISABLE_CLANG_WARNING("-Wreserved-id-macro");
DISABLE_CLANG_WARNING("-Wzero-as-null-pointer-constant");
DISABLE_CLANG_WARNING("-Wold-style-cast");
DISABLE_CLANG_WARNING("-Wcast-qual");
DISABLE_CLANG_WARNING("-Wsign-conversion");
DISABLE_CLANG_WARNING("-Wdisabled-macro-expansion");
DISABLE_CLANG_WARNING("-Wcast-align");
DISABLE_CLANG_WARNING("-Wdouble-promotion");
DISABLE_CLANG_WARNING("-Wconversion");
DISABLE_CLANG_WARNING("-Wimplicit-fallthrough");
DISABLE_CLANG_WARNING("-Wconditional-uninitialized");

PUSH_MSVC_WARNINGS
DISABLE_MSVC_WARNINGS(4244 4701 4703 4296 5219 4242)
#include "3rdparty/stb/stb_image.h"
#include "3rdparty/stb/stb_image_write.h"
#include "3rdparty/stb/stb_dxt.h"
POP_MSVC_WARNINGS
POP_CLANG_WARNINGS

namespace ngine::AssetCompiler::Compilers
{
#if SUPPORT_ASSIMP
	[[nodiscard]] Math::Vector3f ConvertVector(const aiVector3D vector)
	{
		return Math::Vector3f{vector.x, vector.y, vector.z};
	}
	[[nodiscard]] aiVector3D ConvertVector(const Math::Vector3f vector)
	{
		return aiVector3D{vector.x, vector.y, vector.z};
	}

	static const Math::Matrix3x3f correctionMatrix = {Math::Right, -Math::Vector3f(Math::Up), Math::Forward};

	[[nodiscard]] Math::LocalTransform ConvertTransform(const aiMatrix4x4 matrix)
	{
		aiVector3D scaling;
		aiQuaternion rotation;
		aiVector3D translation;
		matrix.Decompose(scaling, rotation, translation);

		return Math::LocalTransform{
			Math::ScaledQuaternionf{Math::Quaternionf{rotation.x, rotation.y, rotation.z, rotation.w}, ConvertVector(scaling)},
			ConvertVector(translation)
		};
	}

	[[nodiscard]] aiMatrix4x4 ConvertTransform(const Math::LocalTransform transform)
	{
		aiVector3D scaling{ConvertVector(transform.GetScale())};
		const Math::Quaternionf quaternion = transform.GetRotationQuaternion();
		aiQuaternion rotation{quaternion.w, quaternion.x, quaternion.y, quaternion.z};
		aiVector3D translation{ConvertVector(transform.GetLocation())};

		return aiMatrix4x4{scaling, rotation, translation};
	}

	[[nodiscard]] Math::LocalTransform ConvertAndAdjustTransform(const Math::LocalTransform transform)
	{
		const Math::Quaternionf quaternion = transform.GetRotationQuaternion();
		const Math::Quaternionf newQuaternion{quaternion.x, -quaternion.z, quaternion.y, quaternion.w};

		Math::LocalTransform quatTransform = {
			Math::ScaledQuaternionf{newQuaternion, Math::Vector3f{transform.GetScale().x, transform.GetScale().z, transform.GetScale().y}},
			correctionMatrix.InverseTransformDirection(transform.GetLocation())
		};

		return quatTransform;
	}

	[[nodiscard]] Math::LocalTransform ConvertAndAdjustTransformInverse(const Math::LocalTransform transform)
	{
		const Math::Quaternionf quaternion = transform.GetRotationQuaternion();
		const Math::Quaternionf newQuaternion{quaternion.x, quaternion.y, quaternion.z, quaternion.w};

		Math::LocalTransform quatTransform = {
			Math::ScaledQuaternionf{newQuaternion, Math::Vector3f{transform.GetScale().x, transform.GetScale().y, transform.GetScale().z}},
			transform.GetLocation() // correctionMatrix.TransformDirection(transform.GetLocation())
		};

		return quatTransform;
	}

	[[nodiscard]] float ConvertBlenderLightIntensityToEngineInfluenceRadius(float blenderIntensity)
	{
		// See https://www.desmos.com/calculator/vv80qytwyx
		// l : blender light intensity
		// i : blender irradiance
		// R : sceneri light radius (not influence radius but actual light size)
		// L : light intensity converted to sceneri
		// A : sceneri attenuation function (inverse square law)
		// C : light intensity cutoff (influence radius is defined as the distance to the light where irradiance equals this value)
		// M : influence radius (beyond which light gets cutoff to zero with a window function)
		// D : scenery light intensity derived from influence radius. See PointLightComponent::SetInfluenceRadius)
		// W : Windowing function. Non physically correct function to force irradiance to zero beyond influenceRadius
		// I : Sceneri irradiance without window function
		// J : Sceneri irradiance with window function

		return Math::Sqrt(blenderIntensity / Entity::LightSourceComponent::m_intensityCutoff);
	}

	[[nodiscard]] float ConvertEngineLightRadiusToBlenderInfluenceIntensity(const float influenceRadius)
	{
		return (influenceRadius * influenceRadius) * Entity::LightSourceComponent::m_intensityCutoff;
	}

	inline static constexpr ConstStringView DefaultSkeletonName = "Skeleton";

	namespace ComponentTypes
	{
		struct PointLight : public Entity::PointLightInstanceProperties
		{
			Vector<Asset::Guid> m_stageGuids;
		};

		struct DirectionalLight : public Entity::DirectionalLightInstanceProperties
		{
			Vector<Asset::Guid> m_stageGuids;
		};

		struct SpotLight : public Entity::SpotLightInstanceProperties
		{
			Vector<Asset::Guid> m_stageGuids;
		};

		struct Scene
		{
			Asset::Guid m_sceneAssetGuid;
		};

		struct SimpleComponent
		{
		};

		struct StaticMesh
		{
			Asset::Guid m_meshAssetGuid;
			Asset::Guid m_materialInstanceAssetGuid;
			Vector<Asset::Guid> m_stageGuids;
		};

		struct SkinnedMesh : public StaticMesh
		{
			Asset::Guid m_meshSkinAssetGuid;
			Asset::Guid m_skeletonAssetGuid;
		};

		struct SkeletonMesh
		{
			struct DefaultAnimationController
			{
				bool Serialize(const Serialization::Reader serializer)
				{
					if (*serializer.Read<Guid>("typeGuid") == Reflection::GetTypeGuid<Animation::LoopingAnimationController>())
					{
						serializer.Serialize("animation", m_animationAssetGuid);
						return true;
					}
					return false;
				}

				bool Serialize(Serialization::Writer serializer) const
				{
					serializer.Serialize("typeGuid", Reflection::GetTypeGuid<Animation::LoopingAnimationController>());
					serializer.Serialize("animation", m_animationAssetGuid);
					return m_animationAssetGuid.IsValid();
				}

				Asset::Guid m_animationAssetGuid;
			};

			Asset::Guid m_skeletonAssetGuid;
			Optional<DefaultAnimationController> m_defaultAnimationController{DefaultAnimationController{}};
		};

		namespace Physics
		{
			struct Collider
			{
				Asset::Guid m_physicalMaterialAssetGuid = ngine::Physics::Materials::DefaultAssetGuid;
			};

			struct BoxCollider : public Collider
			{
				Math::Vector3f m_halfSize;
			};

			struct CapsuleCollider : public Collider
			{
				Math::Radiusf m_radius = 0.5_meters;
				Math::Lengthf m_halfHeight = 0.5_meters;
			};

			struct SphereCollider : public Collider
			{
				Math::Radiusf m_radius = 0.5_meters;
			};

			struct InfinitePlaneCollider : public Collider
			{
			};

			struct MeshCollider : public Collider
			{
				Asset::Guid m_meshAssetGuid;
			};
		}
	}

	inline static constexpr Asset::Guid ShadowsStageGuid = "DFCA0EF8-EDED-4660-8ADB-43C0AA8F4E60"_asset;
	inline static constexpr Asset::Guid PBRLightingStageGuid = "F141B823-5844-4FBC-B106-0635FF52199C"_asset;
	inline static constexpr Asset::Guid MaterialsStageGuid = "BBA6AD40-E1EE-4F1E-BAF8-608FF9E7D77F"_asset;

	struct HierarchyEntry
	{
		Guid m_guid;
		String m_name;
		String m_sourceName;
		Vector<HierarchyEntry> m_children;
		Math::LocalTransform m_transform = Math::Identity;
		Guid m_instanceGuid = Guid::Generate();
		Variant<
			ComponentTypes::PointLight,
			ComponentTypes::DirectionalLight,
			ComponentTypes::SpotLight,
			Entity::CameraProperties,
			ComponentTypes::Scene,
			ComponentTypes::SimpleComponent,
			ComponentTypes::StaticMesh,
			ComponentTypes::SkinnedMesh,
			ComponentTypes::SkeletonMesh,
			ComponentTypes::Physics::BoxCollider,
			ComponentTypes::Physics::CapsuleCollider,
			ComponentTypes::Physics::SphereCollider,
			ComponentTypes::Physics::InfinitePlaneCollider,
			ComponentTypes::Physics::MeshCollider>
			m_componentType;
		Optional<Physics::Data::Body::Type> m_physicsType;

		[[nodiscard]] bool IsValid() const
		{
			return m_children.HasElements() || m_componentType.HasValue();
		}

		[[nodiscard]] HierarchyEntry& GetOrCreateChild(const ConstStringView name)
		{
			const OptionalIterator<HierarchyEntry> existingChildEntry = m_children.FindIf(
				[name](const HierarchyEntry& entry)
				{
					return entry.m_name == name;
				}
			);
			HierarchyEntry* pChildEntry;
			if (existingChildEntry.IsValid())
			{
				pChildEntry = existingChildEntry;
			}
			else
			{
				pChildEntry = &m_children.EmplaceBack();
			}

			pChildEntry->m_name = name;
			return *pChildEntry;
		}

		template<typename ComponentType>
		[[nodiscard]] Optional<ComponentType> FindFirstComponentOfTypeRecursive()
		{
			if (const Optional<ComponentType*> pComponentType = m_componentType.Get<ComponentType>())
			{
				return *pComponentType;
			}

			for (HierarchyEntry& child : m_children)
			{
				if (const Optional<ComponentType> componentType = child.FindFirstComponentOfTypeRecursive<ComponentType>())
				{
					return componentType;
				}
			}

			return Invalid;
		}

		bool Serialize(const Serialization::Reader serializer)
		{
			bool serializedAny = serializer.Serialize("guid", m_guid);
			serializedAny |= serializer.Serialize("children", m_children);
			if (Optional<Serialization::Reader> dataComponentsReader = serializer.FindSerializer("data_components"))
			{
				for (const Serialization::Reader dataComponentReader : dataComponentsReader->GetArrayView())
				{
					static constexpr ngine::Guid editorInfoTypeGuid = "fefa51b8-945d-4f88-8859-356c7546efa7"_guid;
					const Guid typeGuid = *dataComponentReader.Read<Guid>("typeGuid");
					if (typeGuid == editorInfoTypeGuid)
					{
						dataComponentReader.Serialize("name", m_name);
					}

					static constexpr ngine::Guid physicsBodyTypeGuid = Reflection::GetTypeGuid<Physics::Data::Body>();
					if (typeGuid == physicsBodyTypeGuid)
					{
						m_physicsType = dataComponentReader.Read<Physics::Data::Body::Type>("type");
					}
					serializedAny = true;
				}
			}

			if(const Optional<Serialization::Reader> transformSerializer =
			       serializer.FindSerializer(Reflection::GetTypeGuid<Entity::Component3D>().ToString().GetView()))
			{
				transformSerializer.Get().SerializeInPlace(m_transform);
			}

			static constexpr auto readStages = [](const Serialization::Reader serializer, Vector<Asset::Guid>& stages)
			{
				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Entity::RenderItemComponent>().ToString().GetView()))
				{
					typeSerializer->Serialize("stages", stages);
				}
			};

			static constexpr auto readStaticMesh = [](const Serialization::Reader serializer, ComponentTypes::StaticMesh& meshOut)
			{
				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Entity::StaticMeshComponent>().ToString().GetView()))
				{
					typeSerializer->Serialize("mesh", meshOut.m_meshAssetGuid);
					typeSerializer->Serialize("material_instance", meshOut.m_materialInstanceAssetGuid);
				}
				readStages(serializer, meshOut.m_stageGuids);
			};

			static constexpr auto readPhysicsCollider = [](const Serialization::Reader serializer, ComponentTypes::Physics::Collider& colliderOut)
			{
				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Physics::ColliderComponent>().ToString().GetView()))
				{
					typeSerializer->Serialize("physical_material", colliderOut.m_physicalMaterialAssetGuid);
				}
			};

			m_instanceGuid = serializer.ReadWithDefaultValue<Guid>("instanceGuid", Guid::Generate());

			const Guid typeGuid = serializer.ReadWithDefaultValue<Guid>("typeGuid", Guid{});
			if (typeGuid == Reflection::GetTypeGuid<Entity::PointLightComponent>())
			{
				ComponentTypes::PointLight properties;

				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Entity::PointLightComponent>().ToString().GetView()))
				{
					Threading::JobBatch jobBatch;
					Reflection::GetType<Entity::PointLightInstanceProperties>()
						.SerializeTypePropertiesInline(serializer, *typeSerializer, properties, Invalid, jobBatch);
				}
				readStages(serializer, properties.m_stageGuids);

				m_componentType = properties;
			}
			else if (typeGuid == Reflection::GetTypeGuid<Entity::DirectionalLightComponent>())
			{
				ComponentTypes::DirectionalLight properties;

				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Entity::DirectionalLightComponent>().ToString().GetView()))
				{
					Threading::JobBatch jobBatch;
					Reflection::GetType<Entity::DirectionalLightInstanceProperties>()
						.SerializeTypePropertiesInline(serializer, *typeSerializer, properties, Invalid, jobBatch);
				}
				readStages(serializer, properties.m_stageGuids);

				m_componentType = properties;
			}
			else if (typeGuid == Reflection::GetTypeGuid<Entity::SpotLightComponent>())
			{
				ComponentTypes::SpotLight properties;

				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Entity::SpotLightComponent>().ToString().GetView()))
				{
					Threading::JobBatch jobBatch;
					Reflection::GetType<Entity::SpotLightInstanceProperties>()
						.SerializeTypePropertiesInline(serializer, *typeSerializer, properties, Invalid, jobBatch);
				}
				readStages(serializer, properties.m_stageGuids);

				m_componentType = properties;
			}
			else if (typeGuid == Reflection::GetTypeGuid<Entity::SceneComponent>())
			{
				ComponentTypes::Scene properties;

				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Entity::SceneComponent>().ToString().GetView()))
				{
					typeSerializer->Serialize("scene", properties.m_sceneAssetGuid);
				}

				Assert(properties.m_sceneAssetGuid.IsValid());
				m_componentType = properties;
			}
			else if (typeGuid == Reflection::GetTypeGuid<Entity::Component3D>())
			{
				ComponentTypes::SimpleComponent properties;
				m_componentType = properties;
			}
			else if (typeGuid == Reflection::GetTypeGuid<Entity::StaticMeshComponent>())
			{
				ComponentTypes::StaticMesh properties;
				readStaticMesh(serializer, properties);
				m_componentType = properties;
			}
			else if (typeGuid == Reflection::GetTypeGuid<Animation::SkinnedMeshComponent>())
			{
				ComponentTypes::SkinnedMesh properties;
				readStaticMesh(serializer, properties);
				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Animation::SkinnedMeshComponent>().ToString().GetView()))
				{
					typeSerializer->Serialize("skin", properties.m_meshSkinAssetGuid);
				}
				m_componentType = properties;
			}
			else if (typeGuid == Reflection::GetTypeGuid<Animation::SkeletonComponent>())
			{
				ComponentTypes::SkeletonMesh properties;
				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Animation::SkeletonComponent>().ToString().GetView()))
				{
					typeSerializer->Serialize("skeleton", properties.m_skeletonAssetGuid);
				}

				if (Optional<Serialization::Reader> dataComponentsReader = serializer.FindSerializer("data_components"))
				{
					for (const Serialization::Reader dataComponentReader : dataComponentsReader->GetArrayView())
					{
						static constexpr ngine::Guid loopingAnimationControllerTypeGuid =
							Reflection::GetTypeGuid<Animation::LoopingAnimationController>();
						if (*dataComponentReader.Read<Guid>("typeGuid") == loopingAnimationControllerTypeGuid)
						{
							if (!properties.m_defaultAnimationController.IsValid())
							{
								properties.m_defaultAnimationController.CreateInPlace();
							}
							dataComponentReader.SerializeInPlace(properties.m_defaultAnimationController);
						}
						else
						{
							properties.m_defaultAnimationController = ComponentTypes::SkeletonMesh::DefaultAnimationController{};
						}

						serializedAny = true;
					}
				}

				m_componentType = properties;
			}
			else if (typeGuid == Reflection::GetTypeGuid<Physics::BoxColliderComponent>())
			{
				ComponentTypes::Physics::BoxCollider properties;
				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Physics::BoxColliderComponent>().ToString().GetView()))
				{
					typeSerializer->Serialize("half_size", properties.m_halfSize);
				}
				readPhysicsCollider(serializer, properties);
				m_componentType = properties;
			}
			else if (typeGuid == Reflection::GetTypeGuid<Physics::CapsuleColliderComponent>())
			{
				ComponentTypes::Physics::CapsuleCollider properties;
				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Physics::CapsuleColliderComponent>().ToString().GetView()))
				{
					typeSerializer->Serialize("radius", properties.m_radius);
					typeSerializer->Serialize("half_height", properties.m_halfHeight);
				}
				readPhysicsCollider(serializer, properties);
				m_componentType = properties;
			}
			else if (typeGuid == Reflection::GetTypeGuid<Physics::SphereColliderComponent>())
			{
				ComponentTypes::Physics::SphereCollider properties;
				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Physics::SphereColliderComponent>().ToString().GetView()))
				{
					typeSerializer->Serialize("radius", properties.m_radius);
				}
				readPhysicsCollider(serializer, properties);
				m_componentType = properties;
			}
			else if (typeGuid == Reflection::GetTypeGuid<Physics::PlaneColliderComponent>())
			{
				ComponentTypes::Physics::InfinitePlaneCollider properties;
				readPhysicsCollider(serializer, properties);
				m_componentType = properties;
			}
			else if (typeGuid == Reflection::GetTypeGuid<Physics::MeshColliderComponent>())
			{
				ComponentTypes::Physics::MeshCollider properties;
				if(const Optional<Serialization::Reader> typeSerializer =
				       serializer.FindSerializer(Reflection::GetTypeGuid<Physics::MeshColliderComponent>().ToString().GetView()))
				{
					typeSerializer->Serialize("mesh", properties.m_meshAssetGuid);
				}
				readPhysicsCollider(serializer, properties);
				m_componentType = properties;
			}

			return serializedAny;
		}

		bool Serialize(Serialization::Writer serializer) const
		{
			bool serializedAny = false;

			Guid componentTypeGuid;
			if (m_componentType.Is<ComponentTypes::PointLight>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Entity::PointLightComponent>();
			}
			else if (m_componentType.Is<ComponentTypes::DirectionalLight>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Entity::DirectionalLightComponent>();
			}
			else if (m_componentType.Is<ComponentTypes::SpotLight>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Entity::SpotLightComponent>();
			}
			else if (m_componentType.Is<Entity::CameraProperties>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Entity::CameraComponent>();
			}
			else if (m_componentType.Is<ComponentTypes::Scene>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Entity::SceneComponent>();
			}
			else if (m_componentType.Is<ComponentTypes::StaticMesh>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Entity::StaticMeshComponent>();
			}
			else if (m_componentType.Is<ComponentTypes::SkinnedMesh>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Animation::SkinnedMeshComponent>();
			}
			else if (m_componentType.Is<ComponentTypes::SkeletonMesh>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Animation::SkeletonComponent>();
			}
			else if (m_componentType.Is<ComponentTypes::Physics::BoxCollider>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Physics::BoxColliderComponent>();
			}
			else if (m_componentType.Is<ComponentTypes::Physics::CapsuleCollider>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Physics::CapsuleColliderComponent>();
			}
			else if (m_componentType.Is<ComponentTypes::Physics::SphereCollider>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Physics::SphereColliderComponent>();
			}
			else if (m_componentType.Is<ComponentTypes::Physics::InfinitePlaneCollider>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Physics::PlaneColliderComponent>();
			}
			else if (m_componentType.Is<ComponentTypes::Physics::MeshCollider>())
			{
				componentTypeGuid = Reflection::GetTypeGuid<Physics::MeshColliderComponent>();
			}
			else
			{
				componentTypeGuid = Reflection::GetTypeGuid<Entity::Component3D>();
			}

			serializedAny |= serializer.Serialize("typeGuid", componentTypeGuid);
			serializedAny |= serializer.Serialize("instanceGuid", m_instanceGuid);

			{
				Serialization::TValue& dataComponentsObject =
					serializer.GetAsObject().FindOrCreateMember("data_components", Serialization::Array(), serializer.GetDocument());
				if (!dataComponentsObject.IsArray())
				{
					dataComponentsObject.GetValue().SetArray();
				}

				Serialization::Writer dataComponentsWriter(dataComponentsObject, serializer.GetData());

				Serialization::Reader dataComponentsReader(dataComponentsWriter.GetValue(), serializer.GetData());

				enum class DefaultComponents : uint8
				{
					Name = 1 << 0,
					PhysicsBody = 1 << 1
				};
				EnumFlags<DefaultComponents> handledDefaultComponents;

				static constexpr ngine::Guid editorInfoTypeGuid = "fefa51b8-945d-4f88-8859-356c7546efa7"_guid;

				for (const Serialization::Reader dataComponentReader : dataComponentsReader.GetArrayView())
				{
					Serialization::Writer dataComponentWriter(
						const_cast<Serialization::Value&>(dataComponentReader.GetValue().GetValue()),
						const_cast<Serialization::Data&>(dataComponentReader.GetData())
					);

					const Guid typeGuid = *dataComponentReader.Read<Guid>("typeGuid");
					if (typeGuid == editorInfoTypeGuid)
					{
						if (m_name.HasElements())
						{
							dataComponentWriter.Serialize("name", m_name);
							handledDefaultComponents |= DefaultComponents::Name;
						}
					}
					else if (typeGuid == Reflection::GetTypeGuid<Physics::Data::Body>())
					{
						if (m_physicsType.IsValid())
						{
							// Don't override the prior settings as they might've been changed by the user
							handledDefaultComponents |= DefaultComponents::PhysicsBody;
						}
					}
				}

				if (!handledDefaultComponents.IsSet(DefaultComponents::Name) && m_name.HasElements())
				{
					Serialization::Value nameObjectValue(rapidjson::Type::kObjectType);
					Serialization::Writer nameWriter(nameObjectValue, dataComponentsWriter.GetData());
					nameWriter.Serialize("typeGuid", editorInfoTypeGuid);
					nameWriter.Serialize("name", m_name);
					dataComponentsWriter.GetAsArray().PushBack(Move(nameObjectValue), dataComponentsWriter.GetDocument());
				}
				if (!handledDefaultComponents.IsSet(DefaultComponents::PhysicsBody) && m_physicsType.IsValid())
				{
					Serialization::Value nameObjectValue(rapidjson::Type::kObjectType);
					Serialization::Writer nameWriter(nameObjectValue, dataComponentsWriter.GetData());
					nameWriter.Serialize("typeGuid", Reflection::GetTypeGuid<Physics::Data::Body>());
					nameWriter.Serialize("type", *m_physicsType);
					dataComponentsWriter.GetAsArray().PushBack(Move(nameObjectValue), dataComponentsWriter.GetDocument());
				}
			}

			Serialization::Reader reader(serializer.GetValue(), serializer.GetData());
			if (const Optional<Serialization::Reader> childArrayReader = reader.FindSerializer("children");
			    childArrayReader.IsValid() && m_children.HasElements())
			{
				ArrayView<const HierarchyEntry> childView = m_children.GetView();

				for (const Serialization::Reader childReader : childArrayReader->GetArrayView())
				{
					// TODO: unify writer and reader
					Serialization::Writer childWriter(
						const_cast<Serialization::TValue&>(childReader.GetValue()),
						const_cast<Serialization::Data&>(childReader.GetData())
					);
					childWriter.SerializeInPlace(childView[0]);

					childView++;
					if (!childView.HasElements())
					{
						break;
					}
				}

				if (childView.HasElements())
				{
					Serialization::Writer childArrayWriter(
						const_cast<Serialization::TValue&>(childArrayReader->GetValue()),
						const_cast<Serialization::Data&>(childArrayReader->GetData())
					);

					for (const HierarchyEntry& child : childView)
					{
						serializedAny |= childArrayWriter.SerializeArrayElementToBack(child);
					}
				}
			}
			else
			{
				serializedAny |= serializer.Serialize("children", m_children);
			}

			serializedAny |= serializer.SerializeObjectWithCallback(
				Reflection::GetTypeGuid<Entity::Component3D>().ToString().GetView(),
				[this](Serialization::Writer serializer) -> bool
				{
					return serializer.SerializeInPlace(m_transform);
				}
			);

			static constexpr auto serializeLightRenderStages = [](Serialization::Writer serializer, const ArrayView<const Asset::Guid> stageGuids)
			{
				return serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Entity::RenderItemComponent>().ToString().GetView(),
					[stageGuids](Serialization::Writer serializer) -> bool
					{
						static constexpr Array<Asset::Guid, 2> defaultLightStageGuids{PBRLightingStageGuid, ShadowsStageGuid};

						return serializer.Serialize("stages", stageGuids.HasElements() ? stageGuids : defaultLightStageGuids.GetView());
					}
				);
			};

			static constexpr auto serializeStaticMeshProperties =
				[](Serialization::Writer serializer, const ComponentTypes::StaticMesh componentTypeInfo)
			{
				bool serializedAny = serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Entity::RenderItemComponent>().ToString().GetView(),
					[componentTypeInfo](Serialization::Writer serializer) -> bool
					{
						static constexpr Array<Asset::Guid, 2> defaultStageGuids{MaterialsStageGuid, ShadowsStageGuid};

						serializer.Serialize(
							"stages",
							componentTypeInfo.m_stageGuids.HasElements() ? componentTypeInfo.m_stageGuids.GetView() : defaultStageGuids.GetView()
						);
						return true;
					}
				);

				Assert(componentTypeInfo.m_materialInstanceAssetGuid.IsValid());
				Assert(componentTypeInfo.m_meshAssetGuid.IsValid());
				serializedAny |= serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Entity::StaticMeshComponent>().ToString().GetView(),
					[componentTypeInfo](Serialization::Writer serializer) -> bool
					{
						serializer.Serialize("mesh", componentTypeInfo.m_meshAssetGuid);
						serializer.Serialize("material_instance", componentTypeInfo.m_materialInstanceAssetGuid);
						return true;
					}
				);
				return serializedAny;
			};

			static constexpr auto serializePhysicsColliderProperties =
				[](Serialization::Writer serializer, const ComponentTypes::Physics::Collider componentTypeInfo)
			{
				return serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Physics::ColliderComponent>().ToString().GetView(),
					[componentTypeInfo](Serialization::Writer serializer) -> bool
					{
						serializer.SerializeWithDefaultValue(
							"physical_material",
							componentTypeInfo.m_physicalMaterialAssetGuid,
							Physics::Materials::DefaultAssetGuid
						);
						return true;
					}
				);
			};

			if (m_componentType.Is<ComponentTypes::PointLight>())
			{
				const ComponentTypes::PointLight& lightInfo = m_componentType.GetExpected<ComponentTypes::PointLight>();
				serializedAny |= serializeLightRenderStages(serializer, lightInfo.m_stageGuids);
				serializedAny |= serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Entity::PointLightComponent>().ToString().GetView(),
					[lightInfo](Serialization::Writer serializer) -> bool
					{
						return Reflection::GetType<Entity::PointLightInstanceProperties>()
					    .SerializeTypePropertiesInline(serializer, lightInfo, Invalid);
					}
				);
			}
			else if (m_componentType.Is<ComponentTypes::DirectionalLight>())
			{
				const ComponentTypes::DirectionalLight& lightInfo = m_componentType.GetExpected<ComponentTypes::DirectionalLight>();
				serializedAny |= serializeLightRenderStages(serializer, lightInfo.m_stageGuids);
				serializedAny |= serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Entity::DirectionalLightComponent>().ToString().GetView(),
					[lightInfo](Serialization::Writer serializer) -> bool
					{
						return Reflection::GetType<Entity::DirectionalLightInstanceProperties>()
					    .SerializeTypePropertiesInline(serializer, lightInfo, Invalid);
					}
				);
			}
			else if (m_componentType.Is<ComponentTypes::SpotLight>())
			{
				const ComponentTypes::SpotLight& lightInfo = m_componentType.GetExpected<ComponentTypes::SpotLight>();
				serializedAny |= serializeLightRenderStages(serializer, lightInfo.m_stageGuids);
				serializedAny |= serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Entity::SpotLightComponent>().ToString().GetView(),
					[lightInfo](Serialization::Writer serializer) -> bool
					{
						return Reflection::GetType<Entity::SpotLightInstanceProperties>().SerializeTypePropertiesInline(serializer, lightInfo, Invalid);
					}
				);
			}
			else if (m_componentType.Is<Entity::CameraProperties>())
			{
				const Entity::CameraProperties& cameraProperties = m_componentType.GetExpected<Entity::CameraProperties>();
				serializedAny |= serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Entity::CameraComponent>().ToString().GetView(),
					[cameraProperties](Serialization::Writer serializer) -> bool
					{
						return Reflection::GetType<Entity::CameraProperties>().SerializeTypePropertiesInline(serializer, cameraProperties, Invalid);
					}
				);
			}
			else if (m_componentType.Is<ComponentTypes::Scene>())
			{
				const ComponentTypes::Scene& componentTypeInfo = m_componentType.GetExpected<ComponentTypes::Scene>();
				Assert(componentTypeInfo.m_sceneAssetGuid.IsValid());
				serializedAny |= serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Entity::SceneComponent>().ToString().GetView(),
					[componentTypeInfo](Serialization::Writer serializer) -> bool
					{
						return serializer.Serialize("scene", componentTypeInfo.m_sceneAssetGuid);
					}
				);
			}
			else if (m_componentType.Is<ComponentTypes::StaticMesh>())
			{
				const ComponentTypes::StaticMesh& componentTypeInfo = m_componentType.GetExpected<ComponentTypes::StaticMesh>();
				serializedAny |= serializeStaticMeshProperties(serializer, componentTypeInfo);
			}
			else if (m_componentType.Is<ComponentTypes::SkinnedMesh>())
			{
				const ComponentTypes::SkinnedMesh& componentTypeInfo = m_componentType.GetExpected<ComponentTypes::SkinnedMesh>();
				serializedAny |= serializeStaticMeshProperties(serializer, componentTypeInfo);
				serializedAny |= serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Animation::SkinnedMeshComponent>().ToString().GetView(),
					[componentTypeInfo](Serialization::Writer serializer) -> bool
					{
						return serializer.Serialize("skin", componentTypeInfo.m_meshSkinAssetGuid);
					}
				);
			}
			else if (m_componentType.Is<ComponentTypes::SkeletonMesh>())
			{
				const ComponentTypes::SkeletonMesh& componentTypeInfo = m_componentType.GetExpected<ComponentTypes::SkeletonMesh>();
				serializedAny |= serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Animation::SkeletonComponent>().ToString().GetView(),
					[componentTypeInfo](Serialization::Writer serializer) -> bool
					{
						return serializer.Serialize("skeleton", componentTypeInfo.m_skeletonAssetGuid);
					}
				);

				Serialization::TValue& dataComponentsObject =
					serializer.GetAsObject().FindOrCreateMember("data_components", Serialization::Array(), serializer.GetDocument());
				if (!dataComponentsObject.IsArray())
				{
					dataComponentsObject.GetValue().SetArray();
				}

				if (componentTypeInfo.m_defaultAnimationController.IsValid())
				{
					Serialization::Value animationControllerValue(rapidjson::Type::kObjectType);
					Serialization::Writer elementWriter(animationControllerValue, serializer.GetData());
					if (elementWriter.SerializeInPlace(componentTypeInfo.m_defaultAnimationController))
					{
						bool wasFound = false;
						if (dataComponentsObject.GetValue().Size() > 0)
						{
							Serialization::Reader dataComponentsReader(dataComponentsObject.GetValue(), serializer.GetData());

							for (Serialization::Reader dataComponentReader : dataComponentsReader.GetArrayView())
							{
								if (*dataComponentReader.Read<Guid>("typeGuid") == Reflection::GetTypeGuid<Animation::LoopingAnimationController>())
								{
									const_cast<Serialization::TValue&>(dataComponentReader.GetValue()) = Move(animationControllerValue);
									wasFound = true;
									break;
								}
							}
						}

						if (!wasFound)
						{
							dataComponentsObject.GetValue().PushBack(Move(animationControllerValue), serializer.GetDocument().GetAllocator());
						}
					}
				}
			}
			else if (m_componentType.Is<ComponentTypes::Physics::BoxCollider>())
			{
				const ComponentTypes::Physics::BoxCollider& componentTypeInfo = m_componentType.GetExpected<ComponentTypes::Physics::BoxCollider>();
				serializedAny |= serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Physics::BoxColliderComponent>().ToString().GetView(),
					[componentTypeInfo](Serialization::Writer serializer) -> bool
					{
						bool serializedAny = serializer.Serialize("half_size", componentTypeInfo.m_halfSize);
						return serializedAny;
					}
				);
				serializedAny |= serializePhysicsColliderProperties(serializer, componentTypeInfo);
			}
			else if (m_componentType.Is<ComponentTypes::Physics::CapsuleCollider>())
			{
				const ComponentTypes::Physics::CapsuleCollider& componentTypeInfo =
					m_componentType.GetExpected<ComponentTypes::Physics::CapsuleCollider>();
				serializedAny |= serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Physics::CapsuleColliderComponent>().ToString().GetView(),
					[componentTypeInfo](Serialization::Writer serializer) -> bool
					{
						bool serializedAny = serializer.Serialize("radius", componentTypeInfo.m_radius);
						serializedAny |= serializer.Serialize("half_height", componentTypeInfo.m_halfHeight);
						return serializedAny;
					}
				);
				serializedAny |= serializePhysicsColliderProperties(serializer, componentTypeInfo);
			}
			else if (m_componentType.Is<ComponentTypes::Physics::SphereCollider>())
			{
				const ComponentTypes::Physics::SphereCollider& componentTypeInfo =
					m_componentType.GetExpected<ComponentTypes::Physics::SphereCollider>();
				serializedAny |= serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Physics::SphereColliderComponent>().ToString().GetView(),
					[componentTypeInfo](Serialization::Writer serializer) -> bool
					{
						bool serializedAny = serializer.Serialize("radius", componentTypeInfo.m_radius);
						return serializedAny;
					}
				);
				serializedAny |= serializePhysicsColliderProperties(serializer, componentTypeInfo);
			}
			else if (m_componentType.Is<ComponentTypes::Physics::InfinitePlaneCollider>())
			{
				const ComponentTypes::Physics::InfinitePlaneCollider& componentTypeInfo =
					m_componentType.GetExpected<ComponentTypes::Physics::InfinitePlaneCollider>();
				serializedAny |= serializePhysicsColliderProperties(serializer, componentTypeInfo);
			}
			else if (m_componentType.Is<ComponentTypes::Physics::MeshCollider>())
			{
				const ComponentTypes::Physics::MeshCollider& componentTypeInfo = m_componentType.GetExpected<ComponentTypes::Physics::MeshCollider>(
				);
				Assert(componentTypeInfo.m_meshAssetGuid.IsValid());
				serializedAny |= serializer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Physics::MeshColliderComponent>().ToString().GetView(),
					[componentTypeInfo](Serialization::Writer serializer) -> bool
					{
						bool serializedAny = serializer.Serialize("mesh", componentTypeInfo.m_meshAssetGuid);
						return serializedAny;
					}
				);
				serializedAny |= serializePhysicsColliderProperties(serializer, componentTypeInfo);
			}

			return serializedAny;
		}

		[[nodiscard]] void PopulateDependencies(const HierarchyEntry& rootEntry, InlineVector<Asset::Guid, 4>& dependencies) const
		{
			for (const HierarchyEntry& child : m_children)
			{
				child.PopulateDependencies(rootEntry, dependencies);
			}

			m_componentType.Visit(
				[](const ComponentTypes::PointLight&)
				{
				},
				[](const ComponentTypes::DirectionalLight&)
				{
				},
				[](const ComponentTypes::SpotLight&)
				{
				},
				[](const Entity::CameraProperties&)
				{
				},
				[&dependencies, guid = rootEntry.m_guid](const ComponentTypes::Scene& scene)
				{
					if (scene.m_sceneAssetGuid != guid)
					{
						dependencies.EmplaceBackUnique(Guid(scene.m_sceneAssetGuid));
					}
				},
				[](const ComponentTypes::SimpleComponent&)
				{
				},
				[&dependencies, guid = rootEntry.m_guid](const ComponentTypes::StaticMesh& staticMesh)
				{
					dependencies.EmplaceBackUnique(Guid(staticMesh.m_materialInstanceAssetGuid));

					if (staticMesh.m_meshAssetGuid != guid)
					{
						dependencies.EmplaceBackUnique(Guid(staticMesh.m_meshAssetGuid));
					}
				},
				[&dependencies, guid = rootEntry.m_guid](const ComponentTypes::SkinnedMesh& skinnedMesh)
				{
					dependencies.EmplaceBackUnique(Guid(skinnedMesh.m_materialInstanceAssetGuid));
					dependencies.EmplaceBackUnique(Guid(skinnedMesh.m_skeletonAssetGuid));

					if (skinnedMesh.m_meshAssetGuid != guid)
					{
						dependencies.EmplaceBackUnique(Guid(skinnedMesh.m_meshAssetGuid));
					}
					if (skinnedMesh.m_meshSkinAssetGuid != guid)
					{
						dependencies.EmplaceBackUnique(Guid(skinnedMesh.m_meshSkinAssetGuid));
					}
				},
				[&dependencies, guid = rootEntry.m_guid](const ComponentTypes::SkeletonMesh& skeletonMesh)
				{
					if (skeletonMesh.m_defaultAnimationController.IsValid())
					{
						dependencies.EmplaceBackUnique(Guid(skeletonMesh.m_defaultAnimationController->m_animationAssetGuid));
					}

					if (skeletonMesh.m_skeletonAssetGuid != guid)
					{
						dependencies.EmplaceBackUnique(Guid(skeletonMesh.m_skeletonAssetGuid));
					}
				},
				[&dependencies](const ComponentTypes::Physics::BoxCollider& collider)
				{
					dependencies.EmplaceBackUnique(Guid(collider.m_physicalMaterialAssetGuid));
				},
				[&dependencies](const ComponentTypes::Physics::CapsuleCollider& collider)
				{
					dependencies.EmplaceBackUnique(Guid(collider.m_physicalMaterialAssetGuid));
				},
				[&dependencies](const ComponentTypes::Physics::SphereCollider& collider)
				{
					dependencies.EmplaceBackUnique(Guid(collider.m_physicalMaterialAssetGuid));
				},
				[&dependencies](const ComponentTypes::Physics::InfinitePlaneCollider& collider)
				{
					dependencies.EmplaceBackUnique(Guid(collider.m_physicalMaterialAssetGuid));
				},
				[&dependencies, guid = rootEntry.m_guid](const ComponentTypes::Physics::MeshCollider& collider)
				{
					dependencies.EmplaceBackUnique(Guid(collider.m_physicalMaterialAssetGuid));

					if (collider.m_meshAssetGuid != guid)
					{
						dependencies.EmplaceBackUnique(Guid(collider.m_meshAssetGuid));
					}
				},
				[]()
				{
				}
			);
		}
	};

	struct RootHierarchyEntry : public HierarchyEntry
	{
		using HierarchyEntry::HierarchyEntry;
		using HierarchyEntry::operator=;

		using HierarchyEntry::Serialize;
		bool Serialize(Serialization::Writer serializer) const
		{
			bool success = HierarchyEntry::Serialize(serializer);

			InlineVector<Asset::Guid, 4> dependencies;
			PopulateDependencies(*this, dependencies);
			success |= serializer.Serialize("dependencies", dependencies);

			return success;
		}
	};

	int __cdecl SortTriangles(const void* _left, const void* _right)
	{
		const Rendering::Index* left = static_cast<const Rendering::Index*>(_left);
		const Rendering::Index* right = static_cast<const Rendering::Index*>(_right);
		return (left[0] + left[1] + left[2]) - (right[0] + right[1] + right[2]);
	}

	using JobsContainer = FixedCapacityVector<ReferenceWrapper<Threading::Job>>;

	struct QueuedMaterialInstanceCompilationInfo
	{
		Asset::Guid m_materialInstanceAssetGuid;
		Threading::Job* pJob = nullptr;
	};
	using QueuedMaterialInstanceCompilationsMap = UnorderedMap<const aiMaterial*, QueuedMaterialInstanceCompilationInfo>;

	struct QueuedTextureCompilationInfo
	{
		IO::Path textureAssetMetadataFilePath;
		Threading::Job* pJob = nullptr;
	};
	using QueuedTextureCompilationsMap = UnorderedMap<IO::Path, QueuedTextureCompilationInfo, IO::Path::Hash>;

	struct QueuedMeshSceneCompilationInfo
	{
		Asset::Guid sceneAssetGuid;
	};
	using QueuedMeshSceneCompilationsMap = UnorderedMap<ConstStringView, QueuedMeshSceneCompilationInfo, ConstStringView::Hash>;

	struct QueuedMeshCompilationInfo
	{
		String meshName;
		IO::Path meshMetadataPath;
		Asset::Guid meshAssetGuid;
		Asset::Guid materialInstanceAssetGuid;
		Asset::Guid meshSkinAssetGuid;
		Asset::Guid skeletonAssetGuid;
		Asset::Guid defaultSkeletonAnimationAssetGuid;
		Threading::Job* pJob = nullptr;
	};
	using QueuedMeshCompilationsMap = UnorderedMap<const aiMesh*, QueuedMeshCompilationInfo>;

	struct QueuedSkeletonCompilationInfo
	{
		Asset::Guid skeletonAssetGuid;
		Asset::Guid defaultAnimationAssetGuid;
		Animation::Skeleton m_skeleton;
		UnorderedMap<const aiNode*, uint16> m_jointIndexMap;
		size m_characterCount = 0;
		uint16 m_jointCount = 0;
		Threading::Job* pJob = nullptr;
	};
	using QueuedSkeletonCompilationsMap = UnorderedMap<const aiNode*, QueuedSkeletonCompilationInfo>;

	struct QueuedMeshSkinCompilationInfo
	{
		Asset::Guid meshSkinAssetGuid;
		Threading::Job* pJob = nullptr;
	};
	using QueuedMeshSkinCompilationsMap = UnorderedMap<const aiMesh*, QueuedMeshSkinCompilationInfo>;

	struct HierarchyProcessInfo
	{
		const aiScene& __restrict m_scene;
		const Plugin& m_assetCompiler;
		Threading::JobRunnerThread& m_currentThread;
		const EnumFlags<Platform::Type> m_platforms;
		const EnumFlags<AssetCompiler::CompileFlags> m_compileFlags;
		const Asset::Context& m_assetContext;
		const Asset::Context& m_sourceAssetContext;
		const Asset::Database m_fullOwnersAssetDatabase;
		const IO::PathView m_sourceFilePath;
		const IO::PathView m_sourceDirectory;
		const IO::PathView m_rootDirectory;
		FixedCapacityVector<const aiCamera*> m_remainingCameras;
		FixedCapacityVector<const aiLight*> m_remainingLights;
		FixedCapacityVector<const aiAnimation*> m_remainingAnimations;
		JobsContainer m_jobsToQueue;
		JobsContainer m_jobDependencies;
		const CompileCallback& m_callback;
		QueuedMeshSceneCompilationsMap m_queuedMeshSceneCompilationsMap;
		QueuedMeshCompilationsMap m_queuedMeshCompilationsMap;
		QueuedMeshSkinCompilationsMap m_queuedMeshSkinCompilationsMap;
		QueuedMaterialInstanceCompilationsMap m_queuedMaterialInstanceCompilationsMap;
		QueuedTextureCompilationsMap m_queuedTextureCompilationsMap;
		QueuedSkeletonCompilationsMap m_queuedSkeletonCompilationsMap;
	};

	[[nodiscard]] String FilterSourceName(ConstStringView name)
	{
		constexpr Array<const ConstStringView, 5> prefixes{"SC_", "M_", "SM_", "MI_", "SK_"};

		for (const ConstStringView prefix : prefixes)
		{
			name += prefix.GetSize() * name.StartsWith(prefix);
		}

		String result(name);
		result.ReplaceCharacterOccurrences('_', ' ');
		result.ReplaceCharacterOccurrences('-', ' ');
		return result;
	}

	[[nodiscard]] QueuedMaterialInstanceCompilationInfo GetOrCompileMaterialInstance(
		HierarchyProcessInfo& __restrict info, const aiMaterial& __restrict material, const ConstStringView fallbackName
	)
	{
		const QueuedMaterialInstanceCompilationInfo* pQueuedMaterialInstanceInfo;

		const QueuedMaterialInstanceCompilationsMap::const_iterator it = info.m_queuedMaterialInstanceCompilationsMap.Find(&material);
		if (it != info.m_queuedMaterialInstanceCompilationsMap.end())
		{
			pQueuedMaterialInstanceInfo = &it->second;
		}
		else
		{
			QueuedMaterialInstanceCompilationInfo& queuedMaterialInstanceInfo =
				info.m_queuedMaterialInstanceCompilationsMap.Emplace(&material, {})->second;

			const aiString aiMaterialName = material.GetName();
			ConstStringView materialInstanceName(aiMaterialName.C_Str(), aiMaterialName.length);
			if (materialInstanceName.IsEmpty())
			{
				materialInstanceName = fallbackName;
			}

			if ((materialInstanceName == AI_DEFAULT_MATERIAL_NAME) | (materialInstanceName == "None"))
			{
				queuedMaterialInstanceInfo.m_materialInstanceAssetGuid = Rendering::Constants::DefaultMaterialInstanceAssetGuid;
			}
			else
			{
				IO::Path materialsPath = IO::Path::Combine(info.m_rootDirectory, MAKE_PATH_LITERAL("Material Instances"));
				if (!materialsPath.Exists())
				{
					materialsPath.CreateDirectories();
				}

				IO::Path materialInstancePath = IO::Path::Combine(
					materialsPath,
					IO::Path::Merge(FilterSourceName(materialInstanceName).GetView(), MaterialInstanceAssetType::AssetFormat.metadataFileExtension)
				);
				Serialization::Data materialInstanceAssetData(materialInstancePath);
				UniquePtr<Rendering::MaterialInstanceAsset>
					pMaterialInstanceAsset(Memory::ConstructInPlace, materialInstanceAssetData, Move(materialInstancePath));
				Rendering::MaterialInstanceAsset& materialInstanceAsset = *pMaterialInstanceAsset;

				if (!materialInstanceAsset.GetGuid().IsValid())
				{
					materialInstanceAsset.RegenerateGuid();
				}

				queuedMaterialInstanceInfo.m_materialInstanceAssetGuid = materialInstanceAsset.GetGuid();

				struct Texture
				{
					IO::Path path;
				};
				struct ConstantColor
				{
					Math::Color value;
				};
				struct ConstantScalar
				{
					float value;
				};
				using MaterialSlotContents = Variant<Texture, ConstantColor, ConstantScalar>;
				Array<MaterialSlotContents, (uint8)Rendering::TexturePreset::Count> detectedMaterialContents;

				{
					static constexpr Array aiDiffuseTextureTypes = {aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE};
					static constexpr Array<ConstZeroTerminatedStringView, 2> aiDiffusePropertyNames{"$clr.base", "$clr.diffuse"};

					static constexpr Array aiNormalTextureTypes = {aiTextureType_NORMAL_CAMERA, aiTextureType_NORMALS, aiTextureType_HEIGHT};

					static constexpr Array aiMetalnessTextureTypes = {aiTextureType_METALNESS};
					static constexpr Array<ConstZeroTerminatedStringView, 2> aiMetalnessPropertyNames{"$clr.metallicFactor", "$mat.reflectivity"};

					static constexpr Array aiRoughnessTextureTypes = {aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_SHININESS};
					static constexpr Array<ConstZeroTerminatedStringView, 1> aiRoughnessPropertyNames{"$mat.roughnessFactor"};

					static constexpr Array aiEmissionColorTextureTypes = {aiTextureType_EMISSION_COLOR};
					static constexpr Array<ConstZeroTerminatedStringView, 1> aiEmissionColorPropertyNames{"$clr.emissive"};
					static constexpr Array<ConstZeroTerminatedStringView, 1> aiEmissionFactorPropertyNames{"$mat.emissiveIntensity"};

					static constexpr Array aiAmbientOcclusionTextureTypes = {aiTextureType_AMBIENT_OCCLUSION, aiTextureType_AMBIENT};
					static constexpr Array<ConstZeroTerminatedStringView, 1> aiAmbientPropertyNames{"$clr.ambient"};

					[[maybe_unused]] static constexpr Array aiCubemapTextureTypes = {aiTextureType_REFLECTION};
					static constexpr Array<ConstZeroTerminatedStringView, 1> aiCubemapPropertyNames{"$clr.reflective"};

					static constexpr Array aiAlphaTextureTypes = {aiTextureType_OPACITY};
					static constexpr Array<ConstZeroTerminatedStringView, 1> aiAlphaPropertyNames{"$clr.transparent"};

					auto detectMaterialSlot = [&material, &detectedMaterialContents, sourceDirectory = info.m_sourceDirectory](
																			const Rendering::TexturePreset slotType,
																			const ArrayView<const aiTextureType> textureTypes,
																			const ArrayView<const ConstZeroTerminatedStringView> propertyNames
																		)
					{
						const auto findTexture = [&material, sourceDirectory](const aiTextureType textureType) -> Optional<IO::Path>
						{
							if (material.GetTextureCount(textureType) > 0)
							{
								aiString texturePath;
								if (material.GetTexture((aiTextureType)textureType, 0, &texturePath) == aiReturn_SUCCESS)
								{
									const ConstStringView view(texturePath.C_Str(), texturePath.length);
									IO::Path result = IO::Path(IO::Path::StringType(view));

									if (view.HasElements() && view[0] == '*')
									{
										// Embedded texture, will be resolved later
										return result;
									}

									result.MakeNativeSlashes();
									if (result.IsRelative())
									{
										result = IO::Path::Combine(sourceDirectory, result);
									}

									if (result.Exists())
									{
										return result;
									}
									else
									{
										// Bad path, check if the texture is available in the current directory (common case)
										result = IO::Path::Combine(sourceDirectory, result.GetFileName());
										if (result.Exists())
										{
											return result;
										}
									}
								}
							}
							return Invalid;
						};

						for (const aiTextureType textureType : textureTypes)
						{
							if (Optional<IO::Path> texturePath = findTexture(textureType))
							{
								detectedMaterialContents[(uint8)slotType] = Texture{Move(*texturePath)};
								return;
							}
						}

						for (const ConstZeroTerminatedStringView propertyName : propertyNames)
						{
							const uint32 propertyType = 0;
							const uint32 propertyIndex = 0;

							const aiMaterialProperty* pAiProperty = nullptr;
							if (aiGetMaterialProperty(&material, propertyName, propertyType, propertyIndex, &pAiProperty) == aiReturn_SUCCESS)
							{
								switch (pAiProperty->mType)
								{
									case aiPTI_Float:
									{
										if (pAiProperty->mDataLength == sizeof(float) * 3 || pAiProperty->mDataLength == sizeof(float) * 4)
										{
											auto findMaterialConstantColor =
												[&material](ConstZeroTerminatedStringView propertyName, const uint32 type, const uint32 index)
												-> Optional<Math::Color>
											{
												aiColor4D aiColor4;
												if (aiGetMaterialColor(&material, propertyName, type, index, &aiColor4) == aiReturn_SUCCESS)
												{
													return Math::Color{aiColor4.r, aiColor4.g, aiColor4.b, aiColor4.a};
												}
												return Invalid;
											};
											if (const Optional<Math::Color> constant = findMaterialConstantColor(propertyName, propertyType, propertyIndex))
											{
												detectedMaterialContents[(uint8)slotType] = ConstantColor{*constant};
											}
										}
										else if (pAiProperty->mDataLength == sizeof(float))
										{
											auto findMaterialConstantScalar =
												[&material](ConstZeroTerminatedStringView propertyName, const uint32 type, const uint32 index) -> Optional<float>
											{
												float value;
												if (aiGetMaterialFloat(&material, propertyName, type, index, &value) == aiReturn_SUCCESS)
												{
													return value;
												}
												return Invalid;
											};

											if (const Optional<float> constant = findMaterialConstantScalar(propertyName, propertyType, propertyIndex))
											{
												detectedMaterialContents[(uint8)slotType] = ConstantScalar{*constant};
											}
										}
									}
									break;
									default:
										Assert(false);
										break;
								}
							}
						}
					};

					detectMaterialSlot(
						Rendering::TexturePreset::Diffuse,
						aiDiffuseTextureTypes.GetDynamicView(),
						aiDiffusePropertyNames.GetDynamicView()
					);
					detectMaterialSlot(Rendering::TexturePreset::Normals, aiNormalTextureTypes.GetDynamicView(), {});
					detectMaterialSlot(
						Rendering::TexturePreset::Metalness,
						aiMetalnessTextureTypes.GetDynamicView(),
						aiMetalnessPropertyNames.GetDynamicView()
					);
					detectMaterialSlot(
						Rendering::TexturePreset::Roughness,
						aiRoughnessTextureTypes.GetDynamicView(),
						aiRoughnessPropertyNames.GetDynamicView()
					);
					detectMaterialSlot(
						Rendering::TexturePreset::EmissionColor,
						aiEmissionColorTextureTypes.GetDynamicView(),
						aiEmissionColorPropertyNames.GetDynamicView()
					);
					detectMaterialSlot(Rendering::TexturePreset::EmissionFactor, {}, aiEmissionFactorPropertyNames.GetDynamicView());
					detectMaterialSlot(
						Rendering::TexturePreset::AmbientOcclusion,
						aiAmbientOcclusionTextureTypes.GetDynamicView(),
						aiAmbientPropertyNames.GetDynamicView()
					);
					detectMaterialSlot(
						Rendering::TexturePreset::EnvironmentCubemapDiffuseHDR,
						aiCubemapTextureTypes.GetDynamicView(),
						aiCubemapPropertyNames.GetDynamicView()
					);
					detectedMaterialContents[(uint8)Rendering::TexturePreset::EnvironmentCubemapSpecular] =
						detectedMaterialContents[(uint8)Rendering::TexturePreset::EnvironmentCubemapDiffuseHDR];

					detectMaterialSlot(Rendering::TexturePreset::Alpha, aiAlphaTextureTypes.GetDynamicView(), aiAlphaPropertyNames.GetDynamicView());

					// Detect whether the diffuse texture uses an alpha channel
					if (const Optional<Texture*> pDiffuseTexture = detectedMaterialContents[(uint8)Rendering::TexturePreset::Diffuse].Get<Texture>())
					{
						switch (GenericTextureCompiler::GetAlphaChannelUsageType(pDiffuseTexture->path))
						{
							// TODO: Introduce when we have transparent materials listed here
							/*case GenericTextureCompiler::AlphaChannelUsageType::Transparency:
								detectedMaterialContents[(uint8)Rendering::TexturePreset::DiffuseWithAlphaTransparency] =
								  detectedMaterialContents[(uint8)Rendering::TexturePreset::Diffuse];
								break;*/
							case GenericTextureCompiler::AlphaChannelUsageType::Transparency:
							case GenericTextureCompiler::AlphaChannelUsageType::Mask:
								detectedMaterialContents[(uint8)Rendering::TexturePreset::DiffuseWithAlphaMask] =
									detectedMaterialContents[(uint8)Rendering::TexturePreset::Diffuse];
								break;
							case GenericTextureCompiler::AlphaChannelUsageType::None:
								break;
						}
					}
				}

				if (materialInstanceAsset.m_materialAssetGuid.IsInvalid())
				{
					enum class SlotType : uint8
					{
						Empty,
						Texture,
						ConstantVector,
						ConstantScalar
					};

					using Rendering::TexturePreset;

					struct SupportedMaterial
					{
						Asset::Guid materialAssetGuid;
						static_assert((uint8)Rendering::TexturePreset::Count < 32);
						uint32 requiredSlotTextureMask = 0;
						uint32 requiredSlotConstantColorMask = 0;
						uint32 requiredSlotConstantScalarMask = 0;
					};

					auto presetToFlag = [](const Rendering::TexturePreset preset) -> uint32
					{
						return 1 << (uint8)preset;
					};

					// TODO: Detect this from the asset context and give the user an option if multiple matches are found
					constexpr Array supportedMaterials{
						// Deferred PBR masked material (all textures)
						SupportedMaterial{
							"7942EB3F-D202-4030-A7BE-03F4D886F43D"_asset,
							presetToFlag(TexturePreset::DiffuseWithAlphaMask) | presetToFlag(TexturePreset::Normals) |
								presetToFlag(TexturePreset::Metalness) | presetToFlag(TexturePreset::Roughness)
						},

						// Deferred PBR masked material (diffuse tex w. alpha, metallic + roughness, constants, no normals)
						SupportedMaterial{
							"ac0befd3-fdfd-491c-a266-f5a5a1ebd6b5"_asset,
							presetToFlag(TexturePreset::DiffuseWithAlphaMask),
							0,
							presetToFlag(TexturePreset::Metalness) | presetToFlag(TexturePreset::Roughness)
						},

						// Deferred PBR material (all textures)
						SupportedMaterial{
							"ADE8B81E-1EDF-443A-8AF7-37473EDF1390"_asset,
							presetToFlag(TexturePreset::Diffuse) | presetToFlag(TexturePreset::Normals) | presetToFlag(TexturePreset::Metalness) |
								presetToFlag(TexturePreset::Roughness)
						},

						// Deferred PBR w. bloom material (all textures)
						SupportedMaterial{
							"a342b17f-be9f-4610-b973-0bc1f4157b5b"_asset,
							presetToFlag(TexturePreset::Diffuse) | presetToFlag(TexturePreset::Normals) | presetToFlag(TexturePreset::Metalness) |
								presetToFlag(TexturePreset::Roughness),
							0,
							presetToFlag(TexturePreset::EmissionFactor)
						},

						// Deferred PBR material (diffuse tex, metallic + roughness constants, no normals)
						SupportedMaterial{
							"846a87e3-b738-4471-a336-bcac78e126b1"_asset,
							presetToFlag(TexturePreset::Diffuse),
							0,
							presetToFlag(TexturePreset::Metalness) | presetToFlag(TexturePreset::Roughness)
						},

						// Deferred PBR material (all constants, no normals)
						SupportedMaterial{
							"a897cdfa-a65f-4222-a5ab-d64df68c17b6"_asset,
							0,
							presetToFlag(TexturePreset::Diffuse),
							presetToFlag(TexturePreset::Metalness) | presetToFlag(TexturePreset::Roughness)
						},

						// Deferred PBR w. emissive material (all constants, no normals, emissive)
						SupportedMaterial{
							"40bcfd38-00d8-4d1c-b231-1f8f77b23c9a"_asset,
							0,
							presetToFlag(TexturePreset::Diffuse),
							presetToFlag(TexturePreset::Metalness) | presetToFlag(TexturePreset::Roughness) | presetToFlag(TexturePreset::EmissionFactor)
						}
					};

					const SupportedMaterial* pBestMatch = nullptr;
					uint32 bestMatchNumMaterialRequirements = 0;
					for (const SupportedMaterial& __restrict supportedMaterial : supportedMaterials)
					{
						[[maybe_unused]] const uint32 numMaterialRequirements =
							Memory::GetNumberOfSetBits(supportedMaterial.requiredSlotTextureMask) +
							Memory::GetNumberOfSetBits(supportedMaterial.requiredSlotConstantColorMask) +
							Memory::GetNumberOfSetBits(supportedMaterial.requiredSlotConstantScalarMask);

						uint32 numFullfilledRequirements = 0;
						// Start by checking how many slots we have values for
						for (const uint8 textureSlotIndex : Memory::GetSetBitsIterator(supportedMaterial.requiredSlotTextureMask))
						{
							numFullfilledRequirements += (uint32)detectedMaterialContents[textureSlotIndex].Is<Texture>();
						}
						for (const uint8 constantColorSlotIndex : Memory::GetSetBitsIterator(supportedMaterial.requiredSlotConstantColorMask))
						{
							numFullfilledRequirements += (uint32)detectedMaterialContents[constantColorSlotIndex].Is<ConstantColor>();
						}
						for (const uint8 constantScalarSlotIndex : Memory::GetSetBitsIterator(supportedMaterial.requiredSlotConstantScalarMask))
						{
							numFullfilledRequirements += (uint32)detectedMaterialContents[constantScalarSlotIndex].Is<ConstantScalar>();
						}

						if (numFullfilledRequirements > bestMatchNumMaterialRequirements)
						{
							pBestMatch = &supportedMaterial;
							bestMatchNumMaterialRequirements = numFullfilledRequirements;
						}
					}

					materialInstanceAsset.m_materialAssetGuid = pBestMatch->materialAssetGuid;
				}

				const Optional<const Asset::DatabaseEntry*> pMaterialAssetEntry =
					info.m_fullOwnersAssetDatabase.GetAssetEntry(materialInstanceAsset.m_materialAssetGuid);
				Assert(pMaterialAssetEntry.IsValid());
				if (UNLIKELY(pMaterialAssetEntry.IsInvalid()))
				{
					info.m_callback(
						CompileFlags{},
						ArrayView<Asset::Asset>{*pMaterialInstanceAsset},
						ArrayView<const Serialization::Data>{materialInstanceAssetData}
					);
					return QueuedMaterialInstanceCompilationInfo{};
				}

				IO::Path materialAssetPath(pMaterialAssetEntry->m_path);
				const Serialization::Data materialAssetData(materialAssetPath);
				Rendering::MaterialAsset materialAsset(materialAssetData, Move(materialAssetPath));
				const ArrayView<const Rendering::MaterialAsset::DescriptorBinding, uint8> materialAssetDescriptorBindings =
					materialAsset.GetDescriptorBindings();

				if (materialInstanceAsset.m_descriptorContents.GetSize() < materialAssetDescriptorBindings.GetSize())
				{
					materialInstanceAsset.m_descriptorContents.Resize(materialAssetDescriptorBindings.GetSize());
				}

				// Set default push constant settings
				{
					const ArrayView<const Rendering::PushConstantDefinition, uint8> pushConstantDefinitions = materialAsset.GetPushConstants();
					if (materialInstanceAsset.m_pushConstants.GetSize() < pushConstantDefinitions.GetSize())
					{
						materialInstanceAsset.m_pushConstants.Resize(pushConstantDefinitions.GetSize());
					}

					const ArrayView<Rendering::PushConstant, uint8> pushConstants = materialInstanceAsset.m_pushConstants;
					for (uint8 pushConstantIndex = 0, pushConstantCount = pushConstantDefinitions.GetSize(); pushConstantIndex < pushConstantCount;
					     ++pushConstantIndex)
					{
						Rendering::PushConstant& targetPushConstant = pushConstants[pushConstantIndex];
						const Rendering::PushConstantDefinition& sourcePushConstantDefinition = pushConstantDefinitions[pushConstantIndex];

						if (!targetPushConstant.m_value.HasValue())
						{
							auto applyConstantColor =
								[&targetPushConstant, &detectedMaterialContents, &sourcePushConstantDefinition](const Rendering::TexturePreset presetType)
							{
								MaterialSlotContents& slotContents = detectedMaterialContents[(uint8)presetType];
								if (const Optional<ConstantColor*> pConstantColor = slotContents.Get<ConstantColor>())
								{
									targetPushConstant.m_value = pConstantColor->value;
								}
								else
								{
									targetPushConstant = sourcePushConstantDefinition.m_defaultValue;
								}
							};

							auto applyConstantScalar =
								[&targetPushConstant, &detectedMaterialContents, &sourcePushConstantDefinition](const Rendering::TexturePreset presetType)
							{
								MaterialSlotContents& slotContents = detectedMaterialContents[(uint8)presetType];
								if (const Optional<ConstantScalar*> pConstantScalar = slotContents.Get<ConstantScalar>())
								{
									targetPushConstant.m_value = pConstantScalar->value;
								}
								else
								{
									targetPushConstant = sourcePushConstantDefinition.m_defaultValue;
								}
							};

							switch (sourcePushConstantDefinition.m_preset)
							{
								case Rendering::PushConstantPreset::Unknown:
								case Rendering::PushConstantPreset::Speed:
									targetPushConstant = sourcePushConstantDefinition.m_defaultValue;
									break;
								case Rendering::PushConstantPreset::DiffuseColor:
									applyConstantColor(Rendering::TexturePreset::Diffuse);
									break;
								case Rendering::PushConstantPreset::EmissiveColor:
									applyConstantColor(Rendering::TexturePreset::EmissionColor);
									break;
								case Rendering::PushConstantPreset::AmbientColor:
									applyConstantColor(Rendering::TexturePreset::AmbientOcclusion);
									break;
								case Rendering::PushConstantPreset::ReflectiveColor:
									applyConstantColor(Rendering::TexturePreset::EnvironmentCubemapDiffuseHDR);
									break;
								case Rendering::PushConstantPreset::Metalness:
									applyConstantScalar(Rendering::TexturePreset::Metalness);
									break;
								case Rendering::PushConstantPreset::Roughness:
									applyConstantScalar(Rendering::TexturePreset::Roughness);
									break;
								case Rendering::PushConstantPreset::Emissive:
									applyConstantScalar(Rendering::TexturePreset::EmissionFactor);
									break;
							}
						}
					}
				}

				Threading::Job& saveMaterialInstanceJob = Threading::CreateCallback(
					[materialInstanceAssetData = Move(materialInstanceAssetData),
				   pMaterialInstanceAsset = Move(pMaterialInstanceAsset),
				   callback = CompileCallback(info.m_callback),
				   compileFlags = info.m_compileFlags & ~CompileFlags::WasDirectlyRequested](Threading::JobRunnerThread&) mutable
					{
						pMaterialInstanceAsset->SetTypeGuid(MaterialInstanceAssetType::AssetFormat.assetTypeGuid);
						pMaterialInstanceAsset->UpdateDependencies();
						Serialization::Serialize(materialInstanceAssetData, *pMaterialInstanceAsset);
						callback(
							compileFlags | CompileFlags::Compiled,
							ArrayView<Asset::Asset>{*pMaterialInstanceAsset},
							ArrayView<const Serialization::Data>{materialInstanceAssetData}
						);
					},
					Threading::JobPriority::AssetCompilation
				);

				IO::Path textureTargetDirectory = IO::Path::Combine(info.m_rootDirectory, MAKE_PATH("Textures"));

				for (const Rendering::MaterialAsset::DescriptorBinding& materialAssetDescriptorBinding : materialAssetDescriptorBindings)
				{
					if (materialAssetDescriptorBinding.m_type != Rendering::DescriptorContentType::Texture)
					{
						continue;
					}

					const uint8 descriptorIndex = materialAssetDescriptorBindings.GetIteratorIndex(Memory::GetAddressOf(materialAssetDescriptorBinding
					));
					Rendering::MaterialInstanceAsset::DescriptorContent& targetBinding = materialInstanceAsset.m_descriptorContents[descriptorIndex];

					if (targetBinding.GetTextureAssetGuid().IsValid())
					{
						// Don't overwrite the custom texture asset guid
						continue;
					}

					MaterialSlotContents& slotContents =
						detectedMaterialContents[(uint8)materialAssetDescriptorBinding.m_samplerInfo.m_texturePreset];
					if (!slotContents.Is<Texture>())
					{
						Threading::Job& applyDefaultTextureJob = Threading::CreateCallback(
							[&materialInstanceAsset,
						   descriptorIndex,
						   defaultTextureGuid = materialAssetDescriptorBinding.m_samplerInfo.m_defaultTextureGuid,
						   defaultAddressMode = materialAssetDescriptorBinding.m_samplerInfo.m_defaultAddressMode](Threading::JobRunnerThread&)
							{
								materialInstanceAsset.m_descriptorContents[descriptorIndex] = Rendering::MaterialInstanceAsset::DescriptorContent{
									Rendering::DescriptorContentType::Texture,
									defaultTextureGuid,
									defaultAddressMode
								};
							},
							Threading::JobPriority::AssetCompilation
						);

						applyDefaultTextureJob.AddSubsequentStage(saveMaterialInstanceJob);
						info.m_jobsToQueue.EmplaceBack(applyDefaultTextureJob);
						continue;
					}
					Texture& texture = slotContents.GetExpected<Texture>();
					if (!textureTargetDirectory.Exists())
					{
						textureTargetDirectory.CreateDirectories();
					}

					QueuedTextureCompilationsMap::const_iterator textureIt = info.m_queuedTextureCompilationsMap.Find(texture.path);
					if (textureIt == info.m_queuedTextureCompilationsMap.end())
					{
						auto textureAssetCompilationCallback = [pCallback = SharedPtr<CompileCallback>::Make(info.m_callback),
						                                        &materialInstanceAsset,
						                                        descriptorIndex,
						                                        defaultTextureGuid = materialAssetDescriptorBinding.m_samplerInfo.m_defaultTextureGuid,
						                                        defaultAddressMode = materialAssetDescriptorBinding.m_samplerInfo.m_defaultAddressMode](
																										 const EnumFlags<CompileFlags> compileFlags,
																										 ArrayView<Asset::Asset> assets,
																										 ArrayView<const Serialization::Data> assetsData
																									 )
						{
							(*pCallback)(compileFlags, assets, assetsData);

							Asset::Guid textureAssetGuid;

							if (compileFlags.AreAnySet(CompileFlags::Compiled | CompileFlags::UpToDate))
							{
								Assert(assets.GetSize() == 1);
								textureAssetGuid = assets[0].GetGuid();
							}
							else
							{
								textureAssetGuid = defaultTextureGuid;
							}

							materialInstanceAsset.m_descriptorContents[descriptorIndex] = Rendering::MaterialInstanceAsset::DescriptorContent{
								Rendering::DescriptorContentType::Texture,
								textureAssetGuid,
								defaultAddressMode
							};
						};

						IO::Path textureMetadataPath;
						if (texture.path.GetView()[0] == MAKE_PATH_LITERAL('*'))
						{
							// Embedded texture
							String textureFilename{texture.path.GetView().GetStringView()};
							const aiTexture* pEmbeddedTexture = info.m_scene.GetEmbeddedTexture(textureFilename.GetZeroTerminated());

							const ConstStringView fileNameView(pEmbeddedTexture->mFilename.C_Str(), pEmbeddedTexture->mFilename.length);
							IO::Path fileName = IO::Path(IO::Path::StringType(fileNameView));
							if (fileName.IsEmpty())
							{
								fileName = IO::Path(IO::Path::StringType(fallbackName));
							}

							const ConstStringView format{
								pEmbeddedTexture->achFormatHint,
								(ConstStringView::SizeType)strlen(pEmbeddedTexture->achFormatHint)
							};
							fileName = IO::Path::Merge(fileName.GetWithoutExtensions(), MAKE_PATH_LITERAL("."), IO::Path(IO::Path::StringType(format)));

							IO::Path tempPath = IO::Path::Combine(IO::Path::GetTemporaryDirectory(), MAKE_PATH("EmbeddedTextures"), fileName);
							IO::Path{tempPath.GetParentPath()}.CreateDirectories();
							{
								IO::File tempFile{tempPath, IO::AccessModeFlags::WriteBinary};
								Assert(tempFile.IsValid());
								tempFile.Write(ArrayView<const ByteType, uint32>{(const ByteType*)pEmbeddedTexture->pcData, pEmbeddedTexture->mWidth});
							}
							texture.path = Move(tempPath);

							textureMetadataPath = IO::Path::Combine(
								textureTargetDirectory,
								IO::Path::Merge(fileName.GetFileNameWithoutExtensions(), TextureAssetType::AssetFormat.metadataFileExtension)
							);
						}
						else
						{
							textureMetadataPath = IO::Path::Combine(
								textureTargetDirectory,
								IO::Path::Merge(texture.path.GetFileNameWithoutExtensions(), TextureAssetType::AssetFormat.metadataFileExtension)
							);
						}
						QueuedTextureCompilationInfo& queuedTexture = info.m_queuedTextureCompilationsMap.Emplace(IO::Path(texture.path), {})->second;
						queuedTexture.textureAssetMetadataFilePath = textureMetadataPath;
						if(Threading::Job* pTextureAssetCompilationJob = info.m_assetCompiler.CompileAssetSourceFile(
						       {},
						       textureAssetCompilationCallback,
						       info.m_currentThread,
						       info.m_platforms,
						       IO::Path(texture.path),
						       textureTargetDirectory,
						       info.m_assetContext,
						       info.m_sourceAssetContext
						   ))
						{
							queuedTexture.pJob = pTextureAssetCompilationJob;
							pTextureAssetCompilationJob->AddSubsequentStage(saveMaterialInstanceJob);
							info.m_jobsToQueue.EmplaceBack(*pTextureAssetCompilationJob);
						}
					}
					else
					{
						const QueuedTextureCompilationInfo& queuedTexture = textureIt->second;

						Threading::Job& applyTextureJob = Threading::CreateCallback(
							[&materialInstanceAsset,
						   descriptorIndex,
						   texturePath = queuedTexture.textureAssetMetadataFilePath,
						   defaultTextureGuid = materialAssetDescriptorBinding.m_samplerInfo.m_defaultTextureGuid,
						   defaultAddressMode = materialAssetDescriptorBinding.m_samplerInfo.m_defaultAddressMode](Threading::JobRunnerThread&)
							{
								Asset::Guid textureAssetGuid;

								const Serialization::Data textureAssetData(texturePath);
								const Asset::Asset textureAsset(textureAssetData, IO::Path(texturePath));
								if (textureAsset.GetMetaDataFilePath().Exists())
								{
									textureAssetGuid = textureAsset.GetGuid();
								}
								else
								{
									textureAssetGuid = defaultTextureGuid;
								}

								materialInstanceAsset.m_descriptorContents[descriptorIndex] = Rendering::MaterialInstanceAsset::DescriptorContent{
									Rendering::DescriptorContentType::Texture,
									textureAssetGuid,
									defaultAddressMode
								};
							},
							Threading::JobPriority::AssetCompilation
						);

						applyTextureJob.AddSubsequentStage(saveMaterialInstanceJob);

						if (queuedTexture.pJob != nullptr)
						{
							info.m_jobDependencies.EmplaceBack(applyTextureJob);
							queuedTexture.pJob->AddSubsequentStage(applyTextureJob);
						}
						else
						{
							info.m_jobsToQueue.EmplaceBack(applyTextureJob);
						}
					}
				}

				if (!saveMaterialInstanceJob.HasDependencies())
				{
					info.m_jobsToQueue.EmplaceBack(saveMaterialInstanceJob);
				}
				else
				{
					info.m_jobDependencies.EmplaceBack(saveMaterialInstanceJob);
				}
			}

			pQueuedMaterialInstanceInfo = &queuedMaterialInstanceInfo;
		}

		return *pQueuedMaterialInstanceInfo;
	}

	void CompileMesh(
		const aiMesh& __restrict mesh,
		const Asset::Guid meshAssetGuid,
		const Asset::Guid materialInstanceAssetGuid,
		Serialization::Data& meshMetaData,
		const IO::Path& meshSharedPath,
		const IO::Path& meshMetadataPath,
		const CompileCallback& callback,
		const EnumFlags<CompileFlags> compileFlags,
		const Optional<Tag::Guid> tag
	)
	{
		const Rendering::Index triangleIndexCount = ArrayView<const aiFace>{mesh.mFaces, mesh.mNumFaces}.Count(
			[](const aiFace& __restrict face)
			{
				return (face.mNumIndices == 3) * 3;
			}
		);
		const Rendering::Index vertexCount = mesh.mNumVertices;

		const EnumFlags<Rendering::StaticObject::Flags> staticObjectFlags = [&mesh]()
		{
			EnumFlags<Rendering::StaticObject::Flags> flags;
			for (uint8 vertexColorIndex = 0; vertexColorIndex < Rendering::VertexColors::Size; ++vertexColorIndex)
			{
				if (mesh.HasVertexColors(vertexColorIndex))
				{
					flags |= Rendering::StaticObject::Flags::IsVertexColorSlotUsedFirst << vertexColorIndex;

					const ArrayView<aiColor4D> aiVertexColors{mesh.mColors[vertexColorIndex], mesh.mNumVertices};
					flags |= (Rendering::StaticObject::Flags::HasVertexColorSlotAlphaFirst << vertexColorIndex) *
					         aiVertexColors.Any(
										 [](const aiColor4D& __restrict color)
										 {
											 return color.a < 1.f;
										 }
									 );
				}
			}
			return flags;
		}();

		Rendering::StaticObject
			staticObject(Memory::ConstructWithSize, Memory::Uninitialized, vertexCount, triangleIndexCount, staticObjectFlags);

		const aiVector3D zeroVector(0.0f, 0.0f, 0.0f);
		const aiVector3D rightVector(1.0f, 0.0f, 0.0f);
		const aiVector3D forwardVector(0.0f, 0.0f, 1.0f);

		const bool hasTextureCoordinates = mesh.HasTextureCoords(0);
		const bool hasTangentsAndBitangents = mesh.HasTangentsAndBitangents();
		const ArrayView<aiVector3D> aiVertices{mesh.mVertices, mesh.mNumVertices};

		ArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositions = staticObject.GetVertexElementView<Rendering::VertexPosition>();
		for (Rendering::Index vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++)
		{
			const Math::Vector3f position = ConvertVector(aiVertices[vertexIndex]);
			vertexPositions[vertexIndex] = correctionMatrix.InverseTransformDirection(position);
		}

		ArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormals = staticObject.GetVertexElementView<Rendering::VertexNormals>();
		for (Rendering::Index vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++)
		{
			const Math::UncompressedTangents uncompressedTangents = {
				correctionMatrix.InverseTransformDirection(ConvertVector(mesh.mNormals[vertexIndex])),
				correctionMatrix.InverseTransformDirection(ConvertVector(hasTangentsAndBitangents ? mesh.mTangents[vertexIndex] : rightVector)),
				correctionMatrix.InverseTransformDirection(ConvertVector(hasTangentsAndBitangents ? mesh.mBitangents[vertexIndex] : forwardVector))
			};
			vertexNormals[vertexIndex] = {uncompressedTangents.m_normal, uncompressedTangents.GetCompressed().m_tangent};
		}

		ArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinates =
			staticObject.GetVertexElementView<Rendering::VertexTextureCoordinate>();
		for (Rendering::Index vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++)
		{
			const Math::Vector3f texCoord = ConvertVector(hasTextureCoordinates ? mesh.mTextureCoords[0][vertexIndex] : zeroVector);
			vertexTextureCoordinates[vertexIndex] = Math::Vector2f{texCoord.x, 1.f - texCoord.y};
		}

		uint8 usedVertexColorIndex = 0;
		for (uint8 vertexColorIndex = 0; vertexColorIndex < Rendering::VertexColors::Size; ++vertexColorIndex)
		{
			if (!mesh.HasVertexColors(vertexColorIndex))
			{
				continue;
			}

			const ArrayView<aiColor4D> aiVertexColors{mesh.mColors[vertexColorIndex], mesh.mNumVertices};

			ArrayView<Rendering::VertexColors, Rendering::Index> vertexColors = staticObject.GetVertexElementView<Rendering::VertexColors>();
			for (Rendering::Index vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++)
			{
				const aiColor4D& __restrict aiColor = aiVertexColors[vertexIndex];
				vertexColors[vertexIndex][usedVertexColorIndex] = Math::Color{aiColor.r, aiColor.g, aiColor.b, aiColor.a};
			}

			usedVertexColorIndex++;
		}

		ArrayView<Rendering::Index, Rendering::Index> indices = staticObject.GetIndices();
		Rendering::Index index = 0;
		for (const aiFace& __restrict face : ArrayView<const aiFace>{mesh.mFaces, mesh.mNumFaces})
		{
			if (face.mNumIndices != 3)
				continue;

			const unsigned int* __restrict pIndices = face.mIndices;
			indices[index] = pIndices[0];
			++index;
			indices[index] = pIndices[1];
			++index;
			indices[index] = pIndices[2];
			++index;
		}

		std::qsort(indices.GetData(), indices.GetSize() / 3, sizeof(Rendering::Index) * 3, &SortTriangles);

		[[maybe_unused]] const bool hasTangents =
			Rendering::VertexTangents::Generate(indices, vertexPositions, vertexNormals, vertexTextureCoordinates, 180.0);

		staticObject.CalculateAndSetBoundingBox();

		bool success = true;
		{
			const IO::Path binaryFilePath = IO::Path::Merge(meshSharedPath, MeshPartAssetType::AssetFormat.binaryFileExtension);
			const IO::File binaryFile(binaryFilePath, IO::AccessModeFlags::WriteBinary);
			if (binaryFile.IsValid())
			{
				staticObject.WriteToFile(binaryFile);
			}
			else
			{
				success = false;
			}
		}

		RootHierarchyEntry rootEntry;
		Serialization::Deserialize(meshMetaData, rootEntry);

		if (rootEntry.m_physicsType.IsInvalid())
		{
			rootEntry.m_physicsType = Physics::Data::Body::Type::Static;
		}

		HierarchyEntry& meshColliderEntry = rootEntry.GetOrCreateChild("Mesh Collider");

		{
			ComponentTypes::Physics::MeshCollider& meshCollider =
				meshColliderEntry.m_componentType.GetOrEmplace<ComponentTypes::Physics::MeshCollider>();
			meshCollider.m_meshAssetGuid = meshAssetGuid;
		}

		HierarchyEntry& meshEntry = meshColliderEntry.GetOrCreateChild("Mesh");

		ComponentTypes::StaticMesh& staticMesh = meshEntry.m_componentType.GetOrEmplace<ComponentTypes::StaticMesh>();
		staticMesh.m_meshAssetGuid = meshAssetGuid;
		staticMesh.m_materialInstanceAssetGuid = materialInstanceAssetGuid;

		Serialization::Serialize(meshMetaData, rootEntry);

		Asset::Asset meshAsset(meshMetaData, IO::Path(meshMetadataPath));
		meshAsset.SetTypeGuid(MeshPartAssetType::AssetFormat.assetTypeGuid);
		if (tag.IsValid())
		{
			meshAsset.SetTag(*tag);
		}
		Serialization::Serialize(meshMetaData, meshAsset);
		// Ensure changes from prior serialization match up with the asset file
		Serialization::Deserialize(meshMetaData, meshAsset);

		callback(
			compileFlags | CompileFlags::Compiled * success,
			ArrayView<Asset::Asset>{meshAsset},
			ArrayView<const Serialization::Data>{meshMetaData}
		);
	}

	template<typename Callback>
	static void IterateNodeHierarchy(Callback&& callback, const aiNode& node)
	{
		callback(node);

		const ArrayView<aiNode*> children = {node.mChildren, node.mNumChildren};
		for (aiNode* childNode : children)
		{
			IterateNodeHierarchy(callback, *childNode);
		}
	}

	void CompileSkeleton(
		const aiNode& __restrict skeletonRootNode,
		const Asset::Guid skeletonAssetGuid,
		Serialization::Data& skeletonAssetData,
		const IO::Path& skeletonFilePath,
		QueuedSkeletonCompilationInfo& __restrict skeletonInfo,
		const CompileCallback& callback
	)
	{
		{
			skeletonInfo.m_jointIndexMap.Reserve(skeletonInfo.m_jointCount);
			IterateNodeHierarchy(
				[&jointIndexMap = skeletonInfo.m_jointIndexMap, index = (uint16)0](const aiNode& node) mutable
				{
					jointIndexMap.Emplace(&node, uint16(index));
					index++;
				},
				skeletonRootNode
			);
		}

		{
			using SetJointParents =
				void (*)(const aiNode& node, const int16 jointParentIndex, int16& nextJointIndex, ArrayView<int16_t, uint16>& jointParents);
			static SetJointParents setJointParents =
				[](const aiNode& node, const int16 jointParentIndex, int16& nextJointIndex, ArrayView<int16, uint16>& jointParents)
			{
				const uint16 jointIndex = nextJointIndex++;
				jointParents[0] = jointParentIndex;
				jointParents++;

				const ArrayView<aiNode*> children = {node.mChildren, node.mNumChildren};
				for (aiNode* childNode : children)
				{
					setJointParents(*childNode, jointIndex, nextJointIndex, jointParents);
				}
			};
			int16 nextJointIndex = 0;
			ArrayView<int16_t, uint16> jointParents = skeletonInfo.m_skeleton.GetJointParents();
			setJointParents(skeletonRootNode, ozz::animation::Skeleton::kNoParent, nextJointIndex, jointParents);
		}

		{
			FixedSizeVector<Math::LocalTransform, uint16>
				jointTransformsTemp(Memory::ConstructWithSize, Memory::Uninitialized, skeletonInfo.m_skeleton.GetJointCount());
			using JointTransformsView = ArrayView<Math::LocalTransform, uint16>;
			using SetJointTransforms = void (*)(const aiNode& node, JointTransformsView& jointTransforms);
			static SetJointTransforms setJointTransforms = [](const aiNode& node, JointTransformsView& jointTransforms)
			{
				Math::LocalTransform rawNodeTransform = ConvertTransform(node.mTransformation);
				jointTransforms[0] = rawNodeTransform;
				jointTransforms++;

				const ArrayView<aiNode*> children = {node.mChildren, node.mNumChildren};
				for (aiNode* childNode : children)
				{
					setJointTransforms(*childNode, jointTransforms);
				}
			};

			JointTransformsView jointTransformsView = jointTransformsTemp.GetView();
			setJointTransforms(skeletonRootNode, jointTransformsView);
			jointTransformsTemp[0].SetRotation(Math::Quaternionf(Math::EulerAnglesf{90_degrees, 0_degrees, 0_degrees}));

			// Transfer bind poses.
			ArrayView<ozz::math::SoaTransform, uint16> jointSoATransforms = skeletonInfo.m_skeleton.GetJointBindPoses();

			const ozz::math::SimdFloat4 w_axis = ozz::math::simd_float4::w_axis();
			const ozz::math::SimdFloat4 zero = ozz::math::simd_float4::zero();
			const ozz::math::SimdFloat4 one = ozz::math::simd_float4::one();

			const uint16 jointCount = skeletonInfo.m_skeleton.GetJointCount();
			for (uint16 i = 0, n = skeletonInfo.m_skeleton.GetStructureOfArraysJointCount(); i < n; ++i)
			{
				ozz::math::SimdFloat4 translations[4];
				ozz::math::SimdFloat4 scales[4];
				ozz::math::SimdFloat4 rotations[4];
				for (uint16 j = 0; j < 4; ++j)
				{
					if (LIKELY(i * 4 + j < jointCount))
					{
						const Math::LocalTransform sourceJointTransform = jointTransformsTemp[i * 4 + j];
						const Math::Vector3f location = sourceJointTransform.GetLocation();
						translations[j] = ozz::math::simd_float4::LoadPtr(&location.x);
						const Math::Quaternionf rotation = sourceJointTransform.GetRotationQuaternion();
						rotations[j] = ozz::math::NormalizeSafe4(ozz::math::simd_float4::LoadPtr(&rotation.x), w_axis);
						const Math::Vector3f scale = sourceJointTransform.GetScale();
						scales[j] = ozz::math::simd_float4::LoadPtr(&scale.x);
					}
					else
					{
						translations[j] = zero;
						rotations[j] = w_axis;
						scales[j] = one;
					}
				}

				ozz::math::Transpose4x3(translations, &jointSoATransforms[i].translation.x);
				ozz::math::Transpose4x4(rotations, &jointSoATransforms[i].rotation.x);
				ozz::math::Transpose4x3(scales, &jointSoATransforms[i].scale.x);
			}
		}

		RootHierarchyEntry objectEntry;
		Serialization::Deserialize(skeletonAssetData, objectEntry);

		ComponentTypes::SkeletonMesh& skeletonMesh = objectEntry.m_componentType.GetOrEmplace<ComponentTypes::SkeletonMesh>();
		skeletonMesh.m_skeletonAssetGuid = skeletonAssetGuid;

		Serialization::Serialize(skeletonAssetData, objectEntry);

		{
			struct SkeletonJoints
			{
				struct JointInfo
				{
					bool Serialize(const Serialization::Reader serializer)
					{
						serializer.Serialize("name", m_name);
						serializer.Serialize("index", m_index);
						return true;
					}

					bool Serialize(Serialization::Writer serializer) const
					{
						serializer.Serialize("name", m_name);
						serializer.Serialize("index", m_index);
						return true;
					}

					String m_name;
					Animation::JointIndex m_index;
				};

				bool Serialize(const Serialization::Reader serializer)
				{
					const uint32 jointCount = serializer.GetValue().GetValue().MemberCount();
					m_joints.Reserve(jointCount);
					m_jointNameLookup.Reserve(jointCount);
					for (Serialization::Member<Optional<JointInfo>> jointMember : serializer.GetMemberView<JointInfo>())
					{
						const Guid jointGuid = Guid::TryParse(jointMember.key);
						const JointInfo& joint = m_joints.Emplace(Guid(jointGuid), Move(*jointMember.value))->second;
						m_jointNameLookup.Emplace(joint.m_name, Guid(jointGuid));
					}
					return true;
				}

				bool Serialize(Serialization::Writer serializer) const
				{
					return serializer.SerializeInPlace(m_joints);
				}

				UnorderedMap<Guid, JointInfo, Guid::Hash> m_joints;
				UnorderedMap<ConstStringView, Guid, ConstStringView::Hash> m_jointNameLookup;
			};

			SkeletonJoints joints = Move(Serialization::Reader(skeletonAssetData).ReadWithDefaultValue<SkeletonJoints>("joints", SkeletonJoints{})
			);
			joints.m_joints.Reserve(skeletonInfo.m_skeleton.GetJointNames().GetSize());

			// Generate new instance guids for non-existent joints
			for (char* const & jointNameData : skeletonInfo.m_skeleton.GetJointNames())
			{
				const Animation::JointIndex jointIndex = skeletonInfo.m_skeleton.GetJointNames().GetIteratorIndex(Memory::GetAddressOf(jointNameData
				));
				const ConstZeroTerminatedStringView jointName(jointNameData, (uint32)strlen(jointNameData) + 1);

				const auto nameIt = joints.m_jointNameLookup.Find(jointName);
				if (nameIt != joints.m_jointNameLookup.end())
				{
					const auto jointIt = joints.m_joints.Find(nameIt->second);
					Assert(jointIt != joints.m_joints.end());
					jointIt->second.m_index = jointIndex;
				}
				else
				{
					const Guid jointGuid = Guid::Generate();
					joints.m_joints.Emplace(Guid(jointGuid), SkeletonJoints::JointInfo{String(jointName), jointIndex});
				}
			}

			Serialization::Writer(skeletonAssetData).Serialize("joints", joints);
		}

		Asset::Asset skeletonAsset(skeletonAssetData, IO::Path(skeletonFilePath));
		skeletonAsset.SetTypeGuid(Animation::SkeletonAssetType::AssetFormat.assetTypeGuid);
		Serialization::Serialize(skeletonAssetData, skeletonAsset);
		// Ensure changes from prior serialization match up with the asset file
		Serialization::Deserialize(skeletonAssetData, skeletonAsset);

		const IO::Path targetSkeletonBinaryFilePath =
			skeletonAsset.GetBinaryFilePath(Animation::SkeletonAssetType::AssetFormat.binaryFileExtension);
		IO::File targetSkeletonBinaryFile(targetSkeletonBinaryFilePath, IO::AccessModeFlags::WriteBinary);
		skeletonInfo.m_skeleton.Save(targetSkeletonBinaryFile);

		callback(CompileFlags::Compiled, ArrayView<Asset::Asset>{skeletonAsset}, ArrayView<const Serialization::Data>{skeletonAssetData});
	}

	void CompileMeshSkin(
		const aiMesh& __restrict mesh,
		const Asset::Guid meshAssetGuid,
		const Asset::Guid materialInstanceAssetGuid,
		const Asset::Guid meshSkinAssetGuid,
		Serialization::Data& meshSkinMetaData,
		const ConstStringView meshName,
		const QueuedSkeletonCompilationInfo& __restrict skeletonInfo,
		const IO::Path& targetDirectory,
		const CompileCallback& callback
	)
	{
		Vector<Math::Matrix4x4f, uint16>
			inverseBindPoses(Memory::ConstructWithSize, Memory::InitializeAll, skeletonInfo.m_skeleton.GetJointCount(), Math::Identity);
		const ArrayView<aiBone*, uint16> bones = {mesh.mBones, (uint16)mesh.mNumBones};
		for (uint16 i = 0, n = bones.GetSize(); i < n; ++i)
		{
			const aiBone* pBone = bones[i];
			Math::LocalTransform inverseBindPoseTransform = ConvertTransform(pBone->mOffsetMatrix);

			const Math::Matrix3x3f rotation = inverseBindPoseTransform.GetRotationMatrix();
			const Math::Vector3f location = inverseBindPoseTransform.GetLocation();

			const uint16 jointIndex = skeletonInfo.m_jointIndexMap.Find(pBone->mNode)->second;
			inverseBindPoses[jointIndex] = {
				{rotation.m_right.x, rotation.m_right.y, rotation.m_right.z, 0.f},
				{rotation.m_forward.x, rotation.m_forward.y, rotation.m_forward.z, 0.f},
				{rotation.m_up.x, rotation.m_up.y, rotation.m_up.z, 0.f},
				{location.x, location.y, location.z, 1.f}
			};
		}

		for (uint16 i = 0, n = inverseBindPoses.GetSize(); i < n; ++i)
		{
			Math::Matrix4x4f& m = inverseBindPoses[i];
			Math::Matrix3x3f correction = {
				{m.m_rows[0][0], m.m_rows[0][1], m.m_rows[0][2]},
				{m.m_rows[1][0], m.m_rows[1][1], m.m_rows[1][2]},
				{m.m_rows[2][0], m.m_rows[2][1], m.m_rows[2][2]},
			};

			correction = {correction.m_right, -correction.m_up, correction.m_forward};

			inverseBindPoses[i] = {
				{correction.m_right.x, correction.m_right.y, correction.m_right.z, 0.f},
				{correction.m_forward.x, correction.m_forward.y, correction.m_forward.z, 0.f},
				{correction.m_up.x, correction.m_up.y, correction.m_up.z, 0.f},
				{m.m_rows[3][0], m.m_rows[3][1], m.m_rows[3][2], 1.f}
			};
		}

		// Sort joint indexes according to weights.
		// Also deduce max number of indices per vertex.
		struct VertexWeight
		{
			uint16 m_jointIndex;
			float m_weight;
		};

		using VertexWeights = Vector<VertexWeight, uint16>;
		using BoneVerticesWeights = FixedCapacityVector<VertexWeights, Rendering::Index>;
		BoneVerticesWeights boneVerticesWeights(Memory::ConstructWithSize, Memory::Zeroed, mesh.mNumVertices);

		for (uint16 boneIndex = 0, boneCount = bones.GetSize(); boneIndex < boneCount; ++boneIndex)
		{
			const aiBone* pBone = bones[boneIndex];
			const uint16 jointIndex = skeletonInfo.m_jointIndexMap.Find(pBone->mNode)->second;

			const ArrayView<const aiVertexWeight, Rendering::Index> vertexWeights{pBone->mWeights, pBone->mNumWeights};
			for (const aiVertexWeight& vertexWeight : vertexWeights)
			{
				boneVerticesWeights[vertexWeight.mVertexId].EmplaceBack(VertexWeight{jointIndex, vertexWeight.mWeight});
			}
		}

		uint16 maxInfluenceCount = 0;
		for (const VertexWeights& vertexWeights : boneVerticesWeights)
		{
			maxInfluenceCount = Math::Max(maxInfluenceCount, vertexWeights.GetSize());
		}

		Vector<uint16, Rendering::Index> jointIndices(Memory::ConstructWithSize, Memory::Uninitialized, mesh.mNumVertices * maxInfluenceCount);
		Vector<float, Rendering::Index> jointWeights(Memory::ConstructWithSize, Memory::Uninitialized, mesh.mNumVertices * maxInfluenceCount);

		[[maybe_unused]] bool hasAnyUninfluencedVertices = false;
		for (Rendering::Index vertexIndex = 0, vertexCount = mesh.mNumVertices; vertexIndex < vertexCount; ++vertexIndex)
		{
			ArrayView<uint16, uint16> indices = {&jointIndices[vertexIndex * maxInfluenceCount], maxInfluenceCount};
			ArrayView<float, uint16> weights = {&jointWeights[vertexIndex * maxInfluenceCount], maxInfluenceCount};

			VertexWeights& vertexWeights = boneVerticesWeights[vertexIndex];
			const uint16 vertexInfluenceCount = vertexWeights.GetSize();
			hasAnyUninfluencedVertices |= vertexInfluenceCount == 0;

			// Sort weights, bigger ones first, so that lowest one can be filtered out.
			auto sortInfluenceWeights = [](const VertexWeight& left, const VertexWeight& right)
			{
				return left.m_weight > right.m_weight;
			};
			std::sort(vertexWeights.begin().Get(), vertexWeights.end().Get(), sortInfluenceWeights);

			for (uint16 j = 0; j < vertexInfluenceCount; ++j)
			{
				const VertexWeight& vertexWeight = vertexWeights[j];

				indices[j] = vertexWeight.m_jointIndex;
				weights[j] = vertexWeight.m_weight;
			}

			// Set unused indices and weights.
			for (uint16 j = vertexInfluenceCount; j < maxInfluenceCount; ++j)
			{
				indices[j] = 0;
				weights[j] = 0.f;
			}
		}
		Assert(!hasAnyUninfluencedVertices);

		// Collects all unique indices.
		Vector<uint16, Rendering::Index> jointRemappingIndices(jointIndices.GetView());
		std::sort(jointRemappingIndices.begin().Get(), jointRemappingIndices.end().Get());
		jointRemappingIndices.Remove(ArrayView<uint16, Rendering::Index>{
			std::unique(jointRemappingIndices.begin().Get(), jointRemappingIndices.end().Get()),
			jointRemappingIndices.end().Get()
		});

		// Build mapping table of mesh original joints to the new ones. Unused joints
		// are set to 0.
		Vector<uint16, uint16> originalJointRemap(Memory::ConstructWithSize, Memory::Zeroed, skeletonInfo.m_skeleton.GetJointCount());
		for (uint16 i = 0, n = (uint16)jointRemappingIndices.GetSize(); i < n; ++i)
		{
			originalJointRemap[jointRemappingIndices[i]] = i;
		}

		// Reset all joints in the mesh.
		for (Rendering::Index i = 0, n = jointIndices.GetSize(); i < n; ++i)
		{
			jointIndices[i] = originalJointRemap[jointIndices[i]];
		}

		// Remaps bind poses and removes unused joints.
		for (uint16 i = 0, n = (uint16)jointRemappingIndices.GetSize(); i < n; ++i)
		{
			inverseBindPoses[i] = inverseBindPoses[jointRemappingIndices[i]];
		}
		inverseBindPoses.Resize((uint16)jointRemappingIndices.GetSize());

		Vector<uint16, uint16> jointRemappingIndicesFinal(jointRemappingIndices.GetView());
		Animation::MeshSkin meshSkin(Move(jointRemappingIndicesFinal), Move(inverseBindPoses));

		// TODO: Skin weight part splitting

		// Removes the less significant weight, which is recomputed at runtime (sum of
		// weights equals 1).
		// for (Animation::MeshSkin::Part& part : meshSkin.GetParts())
		{
			const uint16 influence_count = maxInfluenceCount;        // part.GetMaximumJointsPerVertex();
			const Rendering::Index vertex_count = mesh.mNumVertices; //	 part.GetVertexCount();
			if (influence_count <= 1)
			{
				jointWeights.Clear();
			}
			else
			{
				const Vector<float, Rendering::Index> copy{jointWeights};
				jointWeights.Clear();
				jointWeights.Reserve(vertex_count * (influence_count - 1));

				for (Rendering::Index j = 0; j < vertex_count; ++j)
				{
					for (int k = 0; k < influence_count - 1; ++k)
					{
						jointWeights.EmplaceBack(copy[j * influence_count + k]);
					}
				}
			}
			assert(jointWeights.GetSize() == vertex_count * (influence_count - 1));
		}

		meshSkin.EmplacePart(mesh.mNumVertices, Move(jointIndices), Move(jointWeights));

		if (!targetDirectory.Exists())
		{
			targetDirectory.CreateDirectories();
		}

		IO::Path meshSkinMetadataFilePath =
			IO::Path::Combine(targetDirectory, IO::Path::Merge(meshName, Animation::MeshSkinAssetType::AssetFormat.metadataFileExtension));

		RootHierarchyEntry objectEntry;
		Serialization::Deserialize(meshSkinMetaData, objectEntry);

		ComponentTypes::SkinnedMesh& skinnedMesh = objectEntry.m_componentType.GetOrEmplace<ComponentTypes::SkinnedMesh>();
		skinnedMesh.m_meshAssetGuid = meshAssetGuid;
		skinnedMesh.m_materialInstanceAssetGuid = materialInstanceAssetGuid;
		skinnedMesh.m_meshSkinAssetGuid = meshSkinAssetGuid;
		skinnedMesh.m_skeletonAssetGuid = skeletonInfo.skeletonAssetGuid;

		Serialization::Serialize(meshSkinMetaData, objectEntry);

		Asset::Asset meshSkinAsset(meshSkinMetaData, Move(meshSkinMetadataFilePath));
		meshSkinAsset.SetTypeGuid(Animation::MeshSkinAssetType::AssetFormat.assetTypeGuid);

		const IO::Path targetMeshSkinBinaryFilePath =
			meshSkinAsset.GetBinaryFilePath(Animation::MeshSkinAssetType::AssetFormat.binaryFileExtension);
		IO::File targetMeshSkinBinaryFile(targetMeshSkinBinaryFilePath, IO::AccessModeFlags::WriteBinary);
		meshSkin.WriteToFile(targetMeshSkinBinaryFile);

		Serialization::Serialize(meshSkinMetaData, meshSkinAsset);
		// Ensure changes from prior serialization match up with the asset file
		Serialization::Deserialize(meshSkinMetaData, meshSkinAsset);
		callback(CompileFlags::Compiled, ArrayView<Asset::Asset>{meshSkinAsset}, ArrayView<const Serialization::Data>{meshSkinMetaData});
	}

	namespace AnimationBuilder
	{
		template<typename KeyType>
		struct KeyWrapper : public KeyType
		{
			using KeyType::KeyType;
			float m_previousKeyTime;
		};

		template<typename TargetElementType>
		uint32 PushBackIdentityKey(uint16_t _track, float _time, ArrayView<TargetElementType, uint32> target, const uint32 nextTargetIndex)
		{
			float prev_time = -1.f;
			if (nextTargetIndex > 0 && target[nextTargetIndex - 1].track == _track)
			{
				prev_time = target[nextTargetIndex - 1].ratio;
			}
			target[nextTargetIndex] = TargetElementType{_time, _track, Math::Identity};
			target[nextTargetIndex].m_previousKeyTime = prev_time;
			return nextTargetIndex + 1;
		}

		// Copies a track from a RawAnimation to an Animation.
		// Also fixes up the front (t = 0) and back keys (t = 1).
		template<typename SourceElementType, typename TargetElementType>
		[[nodiscard]] uint32 CopyRaw(
			const ArrayView<SourceElementType, uint32> source,
			const uint16_t _track,
			const double inverseDuration,
			ArrayView<TargetElementType, uint32> target,
			uint32 nextTargetIndex
		)
		{
			if (source.IsEmpty())
			{
				// Adds 2 new keys.
				nextTargetIndex = PushBackIdentityKey(_track, 0.f, target, nextTargetIndex);
				nextTargetIndex = PushBackIdentityKey(_track, 1.f, target, nextTargetIndex);
			}
			else if (source.GetSize() == 1)
			{
				// Adds 1 new key.
				const SourceElementType& raw_key = source[0];
				Assert(raw_key.mTime >= 0 && raw_key.mTime * inverseDuration <= 1.0);

				target[nextTargetIndex] = TargetElementType{0.f, _track, Convert(raw_key.mValue)};
				target[nextTargetIndex].m_previousKeyTime = -1.f;
				nextTargetIndex++;

				target[nextTargetIndex] = TargetElementType{1.f, _track, Convert(raw_key.mValue)};
				target[nextTargetIndex].m_previousKeyTime = 0.f;
				nextTargetIndex++;
			}
			else
			{
				// Copies all keys, and fixes up first and last keys.
				float prev_time = -1.f;
				if (source[0].mTime != 0.f)
				{
					// Needs a key at t = 0.f.
					const SourceElementType& raw_key = source[0];
					target[nextTargetIndex] = TargetElementType{0.f, _track, Convert(raw_key.mValue)};
					target[nextTargetIndex].m_previousKeyTime = prev_time;
					nextTargetIndex++;

					prev_time = 0.f;
				}
				for (const SourceElementType& raw_key : source)
				{
					// Copies all keys.
					Assert(raw_key.mTime >= 0 && (raw_key.mTime * inverseDuration) <= 1.f);
					const float keyTime = float(raw_key.mTime * inverseDuration);
					target[nextTargetIndex] = TargetElementType{keyTime, _track, Convert(raw_key.mValue)};
					target[nextTargetIndex].m_previousKeyTime = prev_time;
					nextTargetIndex++;

					prev_time = keyTime;
				}
				if (source.GetLastElement().mTime * inverseDuration - 1.0 != 0)
				{
					// Needs a key at t = _duration.
					const SourceElementType& raw_key = source.GetLastElement();
					target[nextTargetIndex] = TargetElementType{1.f, _track, Convert(raw_key.mValue)};
					target[nextTargetIndex].m_previousKeyTime = prev_time;
					nextTargetIndex++;
				}
			}
			Assert(target[0].ratio == 0.f && target[nextTargetIndex - 1].ratio == 1.f);
			return nextTargetIndex;
		}
	}

	void CompileAnimation(
		[[maybe_unused]] HierarchyProcessInfo& __restrict info,
		const aiAnimation& __restrict aiAnimation,
		const ConstZeroTerminatedStringView sourceAnimationName,
		const IO::Path& animationFilePath,
		Serialization::Data& animationSerializedData,
		[[maybe_unused]] const Asset::Guid skeletonAssetGuid,
		const Animation::Skeleton& skeleton,
		const CompileCallback& callback
	)
	{
#if HAS_FBX_SDK
		{
			ozz::animation::offline::RawAnimation rawAnimation;

			{
				static Threading::Mutex mutex;
				// Intentional leak, workaround for annoying FBX shutdown crashes.
				static ozz::animation::offline::fbx::FbxManagerInstance* pFbxManager = new ozz::animation::offline::fbx::FbxManagerInstance();
				static ozz::animation::offline::fbx::FbxDefaultIOSettings ioSettings =
					ozz::animation::offline::fbx::FbxDefaultIOSettings(*pFbxManager);
				Threading::UniqueLock lock(mutex);
				ozz::animation::offline::fbx::FbxSceneLoader sceneLoader;

				if (info.m_sourceFilePath.GetRightMostExtension() == MAKE_PATH(".fbx"))
				{
					String sourceFilePathString(info.m_sourceFilePath.GetStringView());

					sceneLoader =
						ozz::animation::offline::fbx::FbxSceneLoader(sourceFilePathString.GetZeroTerminated(), "", *pFbxManager, ioSettings);
				}
				else
				{
					Assert(false);
				}

				const bool extractedAnimation = ozz::animation::offline::fbx::ExtractAnimation(
					sourceAnimationName,
					sceneLoader,
					skeleton.GetOzzType(),
					(float)aiAnimation.mTicksPerSecond,
					&rawAnimation
				);
				if (!extractedAnimation || !rawAnimation.Validate())
				{
					Asset::Asset animationAsset(animationSerializedData, IO::Path(animationFilePath));
					callback({}, ArrayView<Asset::Asset>{animationAsset}, ArrayView<const Serialization::Data>{animationSerializedData});
					return;
				}
			}

			ozz::animation::offline::AnimationBuilder builder;
			ozz::unique_ptr<ozz::animation::Animation> pOzzAnimation = builder(rawAnimation);
			if (pOzzAnimation == nullptr)
			{
				Asset::Asset animationAsset(animationSerializedData, IO::Path(animationFilePath));
				callback({}, ArrayView<Asset::Asset>{animationAsset}, ArrayView<const Serialization::Data>{animationSerializedData});
				return;
			}

			Asset::Asset animationAsset(animationSerializedData, IO::Path(animationFilePath));
			animationAsset.SetTypeGuid(Animation::AnimationAssetType::AssetFormat.assetTypeGuid);
			animationAsset.SetDependencies(Array{skeletonAssetGuid});
			animationAsset.SetTag(skeletonAssetGuid);
			Serialization::Serialize(animationSerializedData, animationAsset);
			// Ensure changes from prior serialization match up with the asset file
			Serialization::Deserialize(animationSerializedData, animationAsset);

			const IO::Path targetAnimationBinaryFilePath =
				animationAsset.GetBinaryFilePath(Animation::AnimationAssetType::AssetFormat.binaryFileExtension);
			IO::File targetAnimationBinaryFile(targetAnimationBinaryFilePath, IO::AccessModeFlags::WriteBinary);

			Animation::Animation animation(Move(*pOzzAnimation));

			const ArrayView<ozz::animation::QuaternionKey, typename Animation::Animation::KeyIndexType> rotationKeys = animation.GetRotationKeys(
			);
			for (typename Animation::Animation::KeyIndexType keyIndex = 0; keyIndex < rotationKeys.GetSize(); keyIndex++)
			{
				ozz::animation::QuaternionKey& rotationKey = rotationKeys[keyIndex];
				if (rotationKey.track != 0)
				{
					continue;
				}

				ozz::math::Quaternion ozzQuaternion = rotationKey.Decompress();
				Math::Quaternionf quaternion = {ozzQuaternion.x, ozzQuaternion.y, ozzQuaternion.z, ozzQuaternion.w};

				quaternion = quaternion.TransformRotation(Math::Quaternionf(Math::EulerAnglesf{90_degrees, 0_degrees, 0_degrees}));

				ozzQuaternion = {quaternion.x, quaternion.y, quaternion.z, quaternion.w};
				rotationKey = ozz::animation::QuaternionKey(rotationKey.ratio, rotationKey.track, ozzQuaternion);
			}

			// TODO: Figure out how to make an entry that supports previewing this animation
			// Could add joint components for each bone.

			RootHierarchyEntry objectEntry;
			Serialization::Deserialize(animationSerializedData, objectEntry);

			ComponentTypes::SkeletonMesh& skeletonMesh = objectEntry.m_componentType.GetOrEmplace<ComponentTypes::SkeletonMesh>();
			skeletonMesh.m_skeletonAssetGuid = skeletonAssetGuid;

			if (skeletonMesh.m_defaultAnimationController.IsValid())
			{
				skeletonMesh.m_defaultAnimationController->m_animationAssetGuid = animationAsset.GetGuid();
			}

			Serialization::Serialize(animationSerializedData, objectEntry);

			animation.WriteToFile(targetAnimationBinaryFile);
			Serialization::Serialize(animationSerializedData, animationAsset);
			// Ensure changes from prior serialization match up with the asset file
			Serialization::Deserialize(animationSerializedData, animationAsset);
			callback(
				info.m_compileFlags & ~CompileFlags::WasDirectlyRequested | CompileFlags::Compiled,
				ArrayView<Asset::Asset>{animationAsset},
				ArrayView<const Serialization::Data>{animationSerializedData}
			);
		}
#else
		constexpr bool supportAssImpAnimationImporting = false;
		if constexpr (supportAssImpAnimationImporting)
		{
			using namespace AnimationBuilder;

			const uint16 jointCount = skeleton.GetJointCount();
			const ArrayView<const aiNodeAnim* const, uint16> nodeAnimations = {aiAnimation.mChannels, (uint16)aiAnimation.mNumChannels};
			Assert(nodeAnimations.GetSize() <= jointCount);
			// +2 because worst case we need to add the first and last keys
			uint32 translationCount = jointCount * 2;
			uint32 rotationCount = jointCount * 2;
			uint32 scaleCount = jointCount * 2;

			for (const aiNodeAnim* const pNodeAnim : nodeAnimations)
			{
				translationCount += pNodeAnim->mNumPositionKeys;
				rotationCount += pNodeAnim->mNumRotationKeys;
				scaleCount += pNodeAnim->mNumScalingKeys;
			}

			const uint16 numSoATracks = ozz::Align(jointCount, 4);
			translationCount += numSoATracks;
			rotationCount += numSoATracks;
			scaleCount += numSoATracks;
			Vector<ByteType, size> tempKeyVector(
				Memory::ConstructWithSize,
				Memory::Uninitialized,
				(translationCount + scaleCount) * sizeof(KeyWrapper<ozz::animation::Float3Key>) +
					rotationCount * sizeof(KeyWrapper<ozz::animation::QuaternionKey>)
			);

			const ArrayView<KeyWrapper<ozz::animation::Float3Key>, uint32> tempTargetPositionKeys{
				reinterpret_cast<KeyWrapper<ozz::animation::Float3Key>*>(tempKeyVector.GetData()),
				translationCount
			};
			const ArrayView<KeyWrapper<ozz::animation::QuaternionKey>, uint32> tempTargetRotationKeys{
				reinterpret_cast<KeyWrapper<ozz::animation::QuaternionKey>*>(tempKeyVector.GetData() + tempTargetPositionKeys.GetDataSize()),
				rotationCount
			};
			const ArrayView<KeyWrapper<ozz::animation::Float3Key>, uint32> tempTargetScaleKeys{
				reinterpret_cast<KeyWrapper<ozz::animation::Float3Key>*>(
					tempKeyVector.GetData() + tempTargetPositionKeys.GetDataSize() + tempTargetRotationKeys.GetDataSize()
				),
				scaleCount
			};

			const double duration = Time::Durationd::FromMilliseconds((uint64)aiAnimation.mDuration).GetSeconds();
			const double inverseDuration = Math::MultiplicativeInverse(duration);

			const double inverseDurationToSeconds = inverseDuration * 0.001;

			uint32 positionIndex = 0, rotationIndex = 0, scaleIndex = 0;

			const ArrayView<const char* const, uint16> jointNames = skeleton.GetJointNames();
			for (uint16 jointIndex = 0, n = skeleton.GetJointCount(); jointIndex < n; ++jointIndex)
			{
				const char* zeroTerminatedJointName = jointNames[jointIndex];
				const ConstStringView jointName{zeroTerminatedJointName, (uint32)strlen(zeroTerminatedJointName)};

				const OptionalIterator<const aiNodeAnim* const> pNodeAnim = nodeAnimations.FindIf(
					[jointName](const aiNodeAnim* const pNodeAnim)
					{
						const ConstStringView animNodeName{pNodeAnim->mNodeName.C_Str(), pNodeAnim->mNodeName.length};
						return animNodeName == jointName;
					}
				);

				if (pNodeAnim.IsValid())
				{
					const aiNodeAnim& nodeAnim = **pNodeAnim;

					const ArrayView<const aiVectorKey, uint32> positionKeys = {nodeAnim.mPositionKeys, nodeAnim.mNumPositionKeys};
					positionIndex = CopyRaw(positionKeys, jointIndex, inverseDurationToSeconds, tempTargetPositionKeys, positionIndex);

					const ArrayView<const aiQuatKey, uint32> rotationKeys = {nodeAnim.mRotationKeys, nodeAnim.mNumRotationKeys};
					rotationIndex = CopyRaw(rotationKeys, jointIndex, inverseDurationToSeconds, tempTargetRotationKeys, rotationIndex);

					const ArrayView<const aiVectorKey, uint32> scaleKeys = {nodeAnim.mScalingKeys, nodeAnim.mNumScalingKeys};
					scaleIndex = CopyRaw(scaleKeys, jointIndex, inverseDurationToSeconds, tempTargetScaleKeys, scaleIndex);
				}
				else
				{
					positionIndex = PushBackIdentityKey(jointIndex, 0.f, tempTargetPositionKeys, positionIndex);
					positionIndex = PushBackIdentityKey(jointIndex, 1.f, tempTargetPositionKeys, positionIndex);

					rotationIndex = PushBackIdentityKey(jointIndex, 0.f, tempTargetRotationKeys, rotationIndex);
					rotationIndex = PushBackIdentityKey(jointIndex, 1.f, tempTargetRotationKeys, rotationIndex);

					scaleIndex = PushBackIdentityKey(jointIndex, 0.f, tempTargetScaleKeys, scaleIndex);
					scaleIndex = PushBackIdentityKey(jointIndex, 1.f, tempTargetScaleKeys, scaleIndex);
				}
			}

			// Add enough identity keys to match soa requirements.
			for (uint16 i = skeleton.GetJointCount(); i < numSoATracks; ++i)
			{
				positionIndex = PushBackIdentityKey(i, 0.f, tempTargetPositionKeys, positionIndex);
				positionIndex = PushBackIdentityKey(i, 1.f, tempTargetPositionKeys, positionIndex);

				rotationIndex = PushBackIdentityKey(i, 0.f, tempTargetRotationKeys, rotationIndex);
				rotationIndex = PushBackIdentityKey(i, 1.f, tempTargetRotationKeys, rotationIndex);

				scaleIndex = PushBackIdentityKey(i, 0.f, tempTargetScaleKeys, scaleIndex);
				scaleIndex = PushBackIdentityKey(i, 1.f, tempTargetScaleKeys, scaleIndex);
			}

			auto sortFunc = [](const auto& left, const auto& right)
			{
				const float time_diff = left.m_previousKeyTime - right.m_previousKeyTime;
				return time_diff < 0.f || (time_diff == 0.f && left.track < right.track);
			};
			std::sort(tempTargetPositionKeys.begin().Get(), tempTargetPositionKeys.begin().Get() + positionIndex, sortFunc);
			std::sort(tempTargetRotationKeys.begin().Get(), tempTargetRotationKeys.begin().Get() + rotationIndex, sortFunc);
			std::sort(tempTargetScaleKeys.begin().Get(), tempTargetScaleKeys.begin().Get() + scaleIndex, sortFunc);

			Animation::Animation
				animation((float)duration, skeleton.GetJointCount(), sourceAnimationName, positionIndex, rotationIndex, scaleIndex);

			const ArrayView<ozz::animation::Float3Key, uint32> targetPositionKeys = animation.GetTranslationKeys();
			for (uint32 i = 0, n = positionIndex; i < n; ++i)
			{
				targetPositionKeys[i] = tempTargetPositionKeys[i];
			}
			const ArrayView<ozz::animation::QuaternionKey, uint32> targetRotationKeys = animation.GetRotationKeys();
			for (uint32 i = 0, n = rotationIndex; i < n; ++i)
			{
				targetRotationKeys[i] = tempTargetRotationKeys[i];
			}
			const ArrayView<ozz::animation::Float3Key, uint32> targetScaleKeys = animation.GetScaleKeys();
			for (uint32 i = 0, n = scaleIndex; i < n; ++i)
			{
				targetScaleKeys[i] = tempTargetScaleKeys[i];
			}

			Asset::Asset animationAsset(animationSerializedData, IO::Path(animationFilePath));
			animationAsset.SetTypeGuid(Animation::AnimationAssetType::AssetFormat.assetTypeGuid);
			animationAsset.SetDependencies(Array{skeletonAssetGuid});

			const IO::Path targetAnimationBinaryFilePath =
				animationAsset.GetBinaryFilePath(Animation::AnimationAssetType::AssetFormat.binaryFileExtension);
			IO::File targetAnimationBinaryFile(targetAnimationBinaryFilePath, IO::AccessModeFlags::WriteBinary);
			animation.WriteToFile(targetAnimationBinaryFile);

			Serialization::Serialize(animationSerializedData, animationAsset);
			// Ensure changes from prior serialization match up with the asset file
			Serialization::Deserialize(animationSerializedData, animationAsset);
			callback(
				info.m_compileFlags & ~CompileFlags::WasDirectlyRequested | CompileFlags::Compiled,
				ArrayView<Asset::Asset>{animationAsset},
				ArrayView<const Serialization::Data>{animationSerializedData}
			);
		}
		else
		{
			Asset::Asset animationAsset(animationSerializedData, IO::Path(animationFilePath));
			callback({}, ArrayView<Asset::Asset>{animationAsset}, ArrayView<const Serialization::Data>{animationSerializedData});
		}
#endif
	}

	[[nodiscard]] HierarchyEntry& FindOrCreateSkeletonEntry(
		HierarchyEntry& parentEntry, const Asset::Guid skeletonAssetGuid, const Asset::Guid defaultSkeletonAnimationAssetGuid
	)
	{
		if (parentEntry.m_componentType.Is<ComponentTypes::SkeletonMesh>())
		{
			if (parentEntry.m_componentType.GetExpected<ComponentTypes::SkeletonMesh>().m_skeletonAssetGuid == skeletonAssetGuid)
			{
				return parentEntry;
			}
		}

		for (HierarchyEntry& siblingEntry : parentEntry.m_children)
		{
			if (siblingEntry.m_componentType.Is<ComponentTypes::SkeletonMesh>())
			{
				if (siblingEntry.m_componentType.GetExpected<ComponentTypes::SkeletonMesh>().m_skeletonAssetGuid == skeletonAssetGuid)
				{
					return siblingEntry;
				}
			}
		}

		HierarchyEntry& newSkeletonEntry = parentEntry.m_children.EmplaceBack();
		newSkeletonEntry.m_name = DefaultSkeletonName;

		ComponentTypes::SkeletonMesh& skeleton = newSkeletonEntry.m_componentType.GetOrEmplace<ComponentTypes::SkeletonMesh>();
		skeleton.m_skeletonAssetGuid = skeletonAssetGuid;

		if (skeleton.m_defaultAnimationController.IsValid())
		{
			skeleton.m_defaultAnimationController->m_animationAssetGuid = defaultSkeletonAnimationAssetGuid;
		}

		return newSkeletonEntry;
	}

	struct MeshCompilationResult
	{
		Threading::Job* pJob;
	};

	enum class MeshCompilationFlags : uint8
	{
		IsMeshPart = 1 << 0,
		// Creates parent components such as skeleton, physics colliders etc
		CreateParentComponents = 1 << 1
	};

	[[nodiscard]] MeshCompilationResult GetOrCompileMesh(
		HierarchyProcessInfo& __restrict info,
		const aiMesh& __restrict mesh,
		String&& meshName,
		const IO::PathView parentDirectory,
		Optional<const HierarchyEntry*> pTemplateEntry,
		HierarchyEntry& parentEntry,
		const EnumFlags<MeshCompilationFlags> flags
	)
	{
		const QueuedMeshCompilationInfo* pQueuedMeshInfo;
		const QueuedSkeletonCompilationInfo* pQueuedSkeletonInfo = nullptr;

		const QueuedMeshCompilationsMap::const_iterator it = info.m_queuedMeshCompilationsMap.Find(&mesh);
		if (it != info.m_queuedMeshCompilationsMap.end())
		{
			pQueuedMeshInfo = &it->second;
		}
		else
		{
			const Asset::Format& meshAssetFormat = flags.IsSet(MeshCompilationFlags::IsMeshPart) ? MeshPartAssetType::AssetFormat
			                                                                                     : MeshSceneAssetType::AssetFormat;

			IO::Path meshSharedPath = IO::Path::Combine(parentDirectory, meshName.GetView());
			IO::Path meshMetadataPath = IO::Path::Merge(meshSharedPath, meshAssetFormat.metadataFileExtension);

			// Determine the mesh name
			String newMeshName(meshName);

			uint16 counter = 2;
			for (auto mapIt = info.m_queuedMeshCompilationsMap.begin(), end = info.m_queuedMeshCompilationsMap.end(); mapIt != end; ++mapIt)
			{
				if (mapIt->second.meshMetadataPath == meshMetadataPath)
				{
					newMeshName.Format("{}-{}", meshName, counter);
					meshSharedPath = IO::Path::Combine(parentDirectory, newMeshName.GetView());
					meshMetadataPath = IO::Path::Merge(meshSharedPath, meshAssetFormat.metadataFileExtension);

					++counter;
					mapIt = info.m_queuedMeshCompilationsMap.begin();
				}
			}

			QueuedMeshCompilationInfo& queuedMeshInfo = info.m_queuedMeshCompilationsMap.Emplace(&mesh, {})->second;
			queuedMeshInfo.meshName = Move(newMeshName);
			queuedMeshInfo.meshMetadataPath = meshMetadataPath;

			meshSharedPath = IO::Path::Combine(parentDirectory, queuedMeshInfo.meshName.GetView());
			meshMetadataPath = IO::Path::Merge(meshSharedPath, meshAssetFormat.metadataFileExtension);
			Serialization::Data meshMetaData(meshMetadataPath);
			if (meshMetaData.IsValid())
			{
				Serialization::Reader reader(meshMetaData);
				queuedMeshInfo.meshAssetGuid = reader.ReadWithDefaultValue<Guid>("guid", Guid::Generate());
				Serialization::Writer writer(meshMetaData.GetDocument(), meshMetaData);
				writer.Serialize("guid", queuedMeshInfo.meshAssetGuid);
			}
			else
			{
				meshMetaData = Serialization::Data(rapidjson::Type::kObjectType);
				Serialization::Writer writer(meshMetaData.GetDocument(), meshMetaData);
				queuedMeshInfo.meshAssetGuid = Guid::Generate();
				writer.Serialize("guid", queuedMeshInfo.meshAssetGuid);
			}

			const uint32 materialIndex = mesh.mMaterialIndex;
			const ArrayView<aiMaterial* const> aiMaterials{info.m_scene.mMaterials, info.m_scene.mNumMaterials};
			const QueuedMaterialInstanceCompilationInfo materialInfo =
				GetOrCompileMaterialInstance(info, *aiMaterials[materialIndex], queuedMeshInfo.meshName);

			Assert(materialInfo.m_materialInstanceAssetGuid.IsValid());
			queuedMeshInfo.materialInstanceAssetGuid = materialInfo.m_materialInstanceAssetGuid;

			queuedMeshInfo.pJob = &Threading::CreateCallback(
				[&mesh,
			   meshAssetGuid = queuedMeshInfo.meshAssetGuid,
			   materialInstanceAssetGuid = queuedMeshInfo.materialInstanceAssetGuid,
			   meshMetaData = Move(meshMetaData),
			   meshSharedPath,
			   meshMetadataPath = Move(meshMetadataPath),
			   callback = CompileCallback(info.m_callback),
			   flags,
			   compileFlags = info.m_compileFlags & ~CompileFlags::WasDirectlyRequested](Threading::JobRunnerThread&) mutable
				{
					CompileMesh(
						mesh,
						meshAssetGuid,
						materialInstanceAssetGuid,
						meshMetaData,
						meshSharedPath,
						meshMetadataPath,
						callback,
						compileFlags,
						flags.IsSet(MeshCompilationFlags::IsMeshPart) ? Tags::MeshPart : Optional<Tag::Guid>()
					);
				},
				Threading::JobPriority::AssetCompilation
			);

			if (mesh.HasBones())
			{
				const aiNode& __restrict skeletonRootNode = *mesh.mBones[0]->mArmature;

				{
					QueuedSkeletonCompilationsMap::const_iterator skeletonIt = info.m_queuedSkeletonCompilationsMap.Find(&skeletonRootNode);
					if (skeletonIt != info.m_queuedSkeletonCompilationsMap.end())
					{
						pQueuedSkeletonInfo = &skeletonIt->second;
						queuedMeshInfo.skeletonAssetGuid = pQueuedSkeletonInfo->skeletonAssetGuid;
						queuedMeshInfo.defaultSkeletonAnimationAssetGuid = pQueuedSkeletonInfo->defaultAnimationAssetGuid;
					}
					else
					{
						QueuedSkeletonCompilationInfo& queuedSkeletonInfo = info.m_queuedSkeletonCompilationsMap.Emplace(&skeletonRootNode, {})->second;

						IO::Path skeletonTargetDirectory = IO::Path::Combine(parentDirectory, MAKE_PATH("Skeletons"));
						if (!skeletonTargetDirectory.Exists())
						{
							skeletonTargetDirectory.CreateDirectories();
						}

						const ConstStringView skeletonName{skeletonRootNode.mName.C_Str(), skeletonRootNode.mName.length};
						IO::Path skeletonFilePath = IO::Path::Combine(
							skeletonTargetDirectory,
							IO::Path::Merge(skeletonName, Animation::SkeletonAssetType::AssetFormat.metadataFileExtension)
						);

						Serialization::Data skeletonMetaData(skeletonFilePath);
						if (!skeletonMetaData.GetDocument().IsObject())
						{
							skeletonMetaData.GetDocument().SetObject();
						}

						{
							Serialization::Reader reader(skeletonMetaData);
							queuedSkeletonInfo.skeletonAssetGuid = reader.ReadWithDefaultValue<Guid>("guid", Guid::Generate());
							queuedMeshInfo.skeletonAssetGuid = queuedSkeletonInfo.skeletonAssetGuid;
							Serialization::Writer writer(skeletonMetaData.GetDocument(), skeletonMetaData);
							writer.Serialize("guid", queuedSkeletonInfo.skeletonAssetGuid);
							writer.Serialize("assetTypeGuid", Animation::SkeletonAssetType::AssetFormat.assetTypeGuid);
						}

						IterateNodeHierarchy(
							[&skeletonInfo = queuedSkeletonInfo](const aiNode& node)
							{
								skeletonInfo.m_characterCount += node.mName.length + 1;
								skeletonInfo.m_jointCount++;
							},
							skeletonRootNode
						);

						char* pNextName =
							queuedSkeletonInfo.m_skeleton.Reserve(queuedSkeletonInfo.m_characterCount * sizeof(char), queuedSkeletonInfo.m_jointCount);

						{
							// Copy joint names
							ArrayView<char*, uint16> jointNames = queuedSkeletonInfo.m_skeleton.GetJointNames();
							IterateNodeHierarchy(
								[jointNames, pNextName](const aiNode& node) mutable
								{
									jointNames[0] = pNextName;
									ConstStringView nodeName{node.mName.C_Str(), node.mName.length};
									StringView(pNextName, node.mName.length).CopyFrom(nodeName);
									pNextName += nodeName.GetSize();
									pNextName[0] = '\0';
									pNextName++;
									jointNames++;
								},
								skeletonRootNode
							);
						}

						queuedSkeletonInfo.pJob = &Threading::CreateCallback(
							[&skeletonRootNode,
						   skeletonAssetGuid = queuedSkeletonInfo.skeletonAssetGuid,
						   skeletonMetaData = Move(skeletonMetaData),
						   skeletonFilePath = Move(skeletonFilePath),
						   &queuedSkeletonInfo,
						   callback = CompileCallback(info.m_callback)](Threading::JobRunnerThread&) mutable
							{
								CompileSkeleton(skeletonRootNode, skeletonAssetGuid, skeletonMetaData, skeletonFilePath, queuedSkeletonInfo, callback);
							},
							Threading::JobPriority::AssetCompilation
						);
						info.m_jobsToQueue.EmplaceBack(*queuedSkeletonInfo.pJob);

						for (auto animationIt = info.m_remainingAnimations.begin(), animationEnd = info.m_remainingAnimations.end();
						     animationIt != animationEnd;)
						{
							const aiAnimation& __restrict animation = **animationIt;
							const aiString& __restrict aiFirstBoneName = animation.mChannels[0]->mNodeName;
							const ConstStringView firstBoneName{aiFirstBoneName.C_Str(), aiFirstBoneName.length};

							bool isCompatibleWithSkeleton = false;
							for (const char* pJointName : queuedSkeletonInfo.m_skeleton.GetJointNames())
							{
								if (firstBoneName == ConstStringView(pJointName, (uint32)strlen(pJointName)))
								{
									isCompatibleWithSkeleton = true;
									break;
								}
							}

							if (isCompatibleWithSkeleton)
							{
								IO::Path animationTargetDirectory = IO::Path::Combine(parentDirectory, MAKE_PATH("Animations"));
								if (!animationTargetDirectory.Exists())
								{
									animationTargetDirectory.CreateDirectories();
								}

								const ConstStringView animationName{animation.mName.C_Str(), animation.mName.length};
								IO::Path animationFilePath = IO::Path::Combine(
									animationTargetDirectory,
									IO::Path::Merge(animationName, Animation::AnimationAssetType::AssetFormat.metadataFileExtension)
								);
								Serialization::Data animationSerializedData(animationFilePath);

								Guid animationAssetGuid;
								{
									Serialization::Reader reader(animationSerializedData);
									animationAssetGuid = reader.ReadWithDefaultValue<Guid>("guid", Guid::Generate());
									Serialization::Writer writer(animationSerializedData.GetDocument(), animationSerializedData);
									writer.Serialize("guid", animationAssetGuid);
								}

								Threading::Job* pAnimationJob = &Threading::CreateCallback(
									[&info,
								   &animation = animation,
								   animationName = String(animationName),
								   animationFilePath = Move(animationFilePath),
								   animationSerializedData = Move(animationSerializedData),
								   skeletonAssetGuid = queuedSkeletonInfo.skeletonAssetGuid,
								   &skeleton = queuedSkeletonInfo.m_skeleton,
								   callback = CompileCallback(info.m_callback)](Threading::JobRunnerThread&) mutable
									{
										CompileAnimation(
											info,
											animation,
											animationName,
											animationFilePath,
											animationSerializedData,
											skeletonAssetGuid,
											skeleton,
											callback
										);
									},
									Threading::JobPriority::AssetCompilation
								);
								info.m_jobDependencies.EmplaceBack(*pAnimationJob);
								queuedSkeletonInfo.pJob->AddSubsequentStage(*pAnimationJob);

								if (queuedSkeletonInfo.defaultAnimationAssetGuid.IsInvalid())
								{
									queuedSkeletonInfo.defaultAnimationAssetGuid = animationAssetGuid;
									queuedMeshInfo.defaultSkeletonAnimationAssetGuid = queuedSkeletonInfo.defaultAnimationAssetGuid;
								}

								info.m_remainingAnimations.Remove(animationIt);
								--animationEnd;
							}
							else
							{
								++animationIt;
							}
						}

						pQueuedSkeletonInfo = &queuedSkeletonInfo;
					}
				}

				{
					const QueuedMeshSkinCompilationsMap::const_iterator meshSkinIt = info.m_queuedMeshSkinCompilationsMap.Find(&mesh);
					if (meshSkinIt != info.m_queuedMeshSkinCompilationsMap.end())
					{
						// pQueuedMeshSkinInfo = &meshSkinIt->second;
					}
					else
					{
						QueuedMeshSkinCompilationInfo& queuedMeshSkinInfo = info.m_queuedMeshSkinCompilationsMap.Emplace(&mesh, {})->second;

						IO::Path meshSkinMetadataPath =
							IO::Path::Merge(meshSharedPath, Animation::MeshSkinAssetType::AssetFormat.metadataFileExtension);
						Serialization::Data meshSkinMetaData(meshSkinMetadataPath);
						if (!meshSkinMetaData.GetDocument().IsObject())
						{
							meshSkinMetaData.GetDocument().SetObject();
						}

						Serialization::Reader meshSkinReader(meshSkinMetaData);
						queuedMeshSkinInfo.meshSkinAssetGuid = meshSkinReader.ReadWithDefaultValue<Guid>("guid", Guid::Generate());
						queuedMeshInfo.meshSkinAssetGuid = queuedMeshSkinInfo.meshSkinAssetGuid;
						Serialization::Writer meshSkinWriter(meshSkinMetaData.GetDocument(), meshSkinMetaData);
						meshSkinWriter.Serialize("guid", queuedMeshSkinInfo.meshSkinAssetGuid);

						queuedMeshSkinInfo.pJob = &Threading::CreateCallback(
							[&mesh,
						   meshSkinGuid = queuedMeshSkinInfo.meshSkinAssetGuid,
						   meshAssetGuid = queuedMeshInfo.meshAssetGuid,
						   materialInstanceAssetGuid = queuedMeshInfo.materialInstanceAssetGuid,
						   meshSkinMetaData = Move(meshSkinMetaData),
						   meshName = String(queuedMeshInfo.meshName),
						   &queuedSkeletonInfo = *pQueuedSkeletonInfo,
						   parentDirectory = IO::Path(parentDirectory),
						   callback = CompileCallback(info.m_callback)](Threading::JobRunnerThread&) mutable
							{
								CompileMeshSkin(
									mesh,
									meshSkinGuid,
									meshAssetGuid,
									materialInstanceAssetGuid,
									meshSkinMetaData,
									meshName,
									queuedSkeletonInfo,
									parentDirectory,
									callback
								);
							},
							Threading::JobPriority::AssetCompilation
						);
						info.m_jobDependencies.EmplaceBack(*queuedMeshSkinInfo.pJob);
						pQueuedSkeletonInfo->pJob->AddSubsequentStage(*queuedMeshSkinInfo.pJob);
						queuedMeshInfo.pJob->AddSubsequentStage(*queuedMeshSkinInfo.pJob);
					}
				}
			}

			pQueuedMeshInfo = &queuedMeshInfo;
		}

		if (pQueuedMeshInfo->meshSkinAssetGuid.IsValid())
		{
			HierarchyEntry& skeletonEntry =
				FindOrCreateSkeletonEntry(parentEntry, pQueuedMeshInfo->skeletonAssetGuid, pQueuedMeshInfo->defaultSkeletonAnimationAssetGuid);
			HierarchyEntry& skinnedMeshEntry = skeletonEntry.GetOrCreateChild(pQueuedMeshInfo->meshName);

			ComponentTypes::SkinnedMesh& skinnedMesh = skinnedMeshEntry.m_componentType.GetOrEmplace<ComponentTypes::SkinnedMesh>();
			skinnedMesh.m_meshAssetGuid = pQueuedMeshInfo->meshAssetGuid;
			if (skinnedMesh.m_materialInstanceAssetGuid.IsInvalid())
			{
				skinnedMesh.m_materialInstanceAssetGuid = pQueuedMeshInfo->materialInstanceAssetGuid;
			}
			skinnedMesh.m_meshSkinAssetGuid = pQueuedMeshInfo->meshSkinAssetGuid;

			return MeshCompilationResult{pQueuedMeshInfo->pJob};
		}
		else
		{
			if (flags.IsSet(MeshCompilationFlags::CreateParentComponents))
			{
				HierarchyEntry& entry = parentEntry.GetOrCreateChild(pQueuedMeshInfo->meshName);
				if (pTemplateEntry.IsValid())
				{
					entry = *pTemplateEntry;
				}
				entry.m_name = pQueuedMeshInfo->meshName;

				if (entry.m_physicsType.IsInvalid())
				{
					entry.m_physicsType = Physics::Data::Body::Type::Static;
				}

				HierarchyEntry& meshColliderEntry = entry.GetOrCreateChild(pQueuedMeshInfo->meshName);

				{
					ComponentTypes::Physics::MeshCollider& meshCollider =
						meshColliderEntry.m_componentType.GetOrEmplace<ComponentTypes::Physics::MeshCollider>();
					meshCollider.m_meshAssetGuid = pQueuedMeshInfo->meshAssetGuid;
				}

				HierarchyEntry& meshEntry = meshColliderEntry.GetOrCreateChild(pQueuedMeshInfo->meshName);

				ComponentTypes::StaticMesh& staticMesh = meshEntry.m_componentType.GetOrEmplace<ComponentTypes::StaticMesh>();
				staticMesh.m_meshAssetGuid = pQueuedMeshInfo->meshAssetGuid;
				if (staticMesh.m_materialInstanceAssetGuid.IsInvalid())
				{
					staticMesh.m_materialInstanceAssetGuid = pQueuedMeshInfo->materialInstanceAssetGuid;
				}

				return MeshCompilationResult{pQueuedMeshInfo->pJob};
			}
			else
			{
				HierarchyEntry& entry = parentEntry.GetOrCreateChild(pQueuedMeshInfo->meshName + " Collider");
				if (pTemplateEntry.IsValid())
				{
					entry = *pTemplateEntry;
				}
				entry.m_name = pQueuedMeshInfo->meshName + " Collider";

				{
					ComponentTypes::Physics::MeshCollider& meshCollider = entry.m_componentType.GetOrEmplace<ComponentTypes::Physics::MeshCollider>();
					meshCollider.m_meshAssetGuid = pQueuedMeshInfo->meshAssetGuid;
				}

				HierarchyEntry& meshEntry = entry.GetOrCreateChild(pQueuedMeshInfo->meshName);

				ComponentTypes::StaticMesh& staticMesh = meshEntry.m_componentType.GetOrEmplace<ComponentTypes::StaticMesh>();
				staticMesh.m_meshAssetGuid = pQueuedMeshInfo->meshAssetGuid;
				if (staticMesh.m_materialInstanceAssetGuid.IsInvalid())
				{
					staticMesh.m_materialInstanceAssetGuid = pQueuedMeshInfo->materialInstanceAssetGuid;
				}

				return MeshCompilationResult{pQueuedMeshInfo->pJob};
			}
		}
	}

#if COMPILER_MSVC
#pragma optimize("", off)
#endif
	void ProcessNode(
		HierarchyProcessInfo& __restrict info,
		const aiNode& __restrict node,
		HierarchyEntry& __restrict entry,
		const IO::PathView parentDirectory,
		const Guid parentAssetGuid
	)
	{
		UNUSED(parentAssetGuid);
		if (node.mNumMeshes > 0)
		{
			Assert(!info.m_remainingLights
			          .FindIf(
									[sourceEntryName = entry.m_sourceName](const aiLight* __restrict pLight)
									{
										return sourceEntryName == ConstStringView{pLight->mName.C_Str(), pLight->mName.length};
									}
								)
			          .IsValid());
			Assert(!info.m_remainingCameras
			          .FindIf(
									[sourceEntryName = entry.m_sourceName](const aiCamera* __restrict pCamera)
									{
										return sourceEntryName == ConstStringView{pCamera->mName.C_Str(), pCamera->mName.length};
									}
								)
			          .IsValid());

			ArrayView<const uint32> meshes{node.mMeshes, node.mNumMeshes};
			const aiMesh& __restrict firstMesh = *info.m_scene.mMeshes[meshes[0]];
			meshes++;
			const ConstStringView firstMeshName{firstMesh.mName.C_Str(), firstMesh.mName.length};

			const bool isCombinedMesh = meshes.All(
				[firstMeshName, meshes = info.m_scene.mMeshes](const uint32 meshIndex)
				{
					const aiMesh& __restrict mesh = *meshes[meshIndex];
					const ConstStringView meshName{mesh.mName.C_Str(), mesh.mName.length};
					return meshName == firstMeshName;
				}
			);

			if (isCombinedMesh)
			{
				const QueuedMeshSceneCompilationsMap::const_iterator it = info.m_queuedMeshSceneCompilationsMap.Find(firstMeshName);
				if (it != info.m_queuedMeshSceneCompilationsMap.end())
				{
					entry.m_componentType = ComponentTypes::Scene{it->second.sceneAssetGuid};
				}
				else
				{
					QueuedMeshSceneCompilationInfo& queuedMeshSceneInfo =
						info.m_queuedMeshSceneCompilationsMap.Emplace(ConstStringView(firstMeshName), {})->second;

					String meshSceneName = FilterSourceName(firstMeshName);

					IO::Path sceneAssetDirectory = IO::Path::Combine(
						parentDirectory,
						IO::Path::Merge(meshSceneName.GetView(), MeshSceneAssetType::AssetFormat.metadataFileExtension)
					);

					if (!sceneAssetDirectory.Exists())
					{
						sceneAssetDirectory.CreateDirectories();
					}

					IO::Path sceneAssetPath = IO::Path::Combine(
						sceneAssetDirectory,
						IO::Path::Merge(meshSceneName.GetView(), MeshSceneAssetType::AssetFormat.metadataFileExtension)
					);
					if (!sceneAssetPath.Exists())
					{
						sceneAssetPath = IO::Path::Combine(
							sceneAssetDirectory,
							IO::Path::Merge(MAKE_PATH("Main"), MeshSceneAssetType::AssetFormat.metadataFileExtension)
						);
					}

					Serialization::Data sceneAssetData(sceneAssetPath);
					Asset::Asset sceneAsset(sceneAssetData, Move(sceneAssetPath));
					sceneAsset.SetTag(Tags::MeshScene);
					sceneAsset.SetTypeGuid(MeshSceneAssetType::AssetFormat.assetTypeGuid);
					Serialization::Serialize(sceneAssetData, sceneAsset);
					// Ensure changes from prior serialization match up with the asset file
					Serialization::Deserialize(sceneAssetData, sceneAsset);

					RootHierarchyEntry diskEntry;
					Serialization::Deserialize(sceneAssetData, diskEntry);

					queuedMeshSceneInfo.sceneAssetGuid = sceneAsset.GetGuid();
					diskEntry.m_componentType = ComponentTypes::Scene{sceneAsset.GetGuid()};
					entry.m_componentType = ComponentTypes::Scene{sceneAsset.GetGuid()};

					FixedCapacityVector<ReferenceWrapper<Threading::Job>> jobs(Memory::Reserve, node.mNumMeshes);

					if (diskEntry.m_physicsType.IsInvalid())
					{
						diskEntry.m_physicsType = Physics::Data::Body::Type::Static;
					}

					const ArrayView<const uint32> meshIndices{node.mMeshes, node.mNumMeshes};
					for (const uint32 meshIndex : meshIndices)
					{
						const aiMesh& __restrict mesh = *info.m_scene.mMeshes[meshIndex];
						const aiMaterial& __restrict aiMaterial = *info.m_scene.mMaterials[mesh.mMaterialIndex];

						const aiString aiMaterialName = aiMaterial.GetName();
						const ConstStringView materialName{aiMaterialName.C_Str(), aiMaterialName.length};
						String filteredMaterialName = FilterSourceName(materialName);
						filteredMaterialName = meshSceneName + " " + filteredMaterialName;

						// TODO: Use one physics setup for all mesh subparts
						MeshCompilationResult compilationResult = GetOrCompileMesh(
							info,
							mesh,
							Move(filteredMaterialName),
							sceneAssetDirectory,
							Invalid,
							diskEntry,
							MeshCompilationFlags::IsMeshPart
						);
						if (compilationResult.pJob != nullptr)
						{
							jobs.EmplaceBack(*compilationResult.pJob);
						}
					}

					Threading::Job* pFinishedJob = &Threading::CreateCallback(
						[sceneAssetData = Move(sceneAssetData),
					   diskEntry = Move(diskEntry),
					   sceneAsset = Move(sceneAsset),
					   callback = CompileCallback(info.m_callback),
					   compileFlags = info.m_compileFlags & ~CompileFlags::WasDirectlyRequested](Threading::JobRunnerThread&) mutable
						{
							Serialization::Serialize(sceneAssetData, sceneAsset);
							Serialization::Serialize(sceneAssetData, diskEntry);
							// Ensure changes from prior serialization match up with the asset file
							Serialization::Deserialize(sceneAssetData, sceneAsset);
							callback(
								compileFlags | CompileFlags::Compiled,
								ArrayView<Asset::Asset>{sceneAsset},
								ArrayView<const Serialization::Data>{sceneAssetData}
							);
						},
						Threading::JobPriority::AssetCompilation
					);

					for (Threading::Job& job : jobs)
					{
						job.AddSubsequentStage(*pFinishedJob);
						info.m_jobsToQueue.EmplaceBack(job);
					}

					info.m_jobDependencies.EmplaceBack(*pFinishedJob);
				}
			}
			else
			{
				IO::Path sceneAssetDirectory =
					IO::Path::Combine(parentDirectory, IO::Path::Merge(entry.m_name.GetView(), Scene3DAssetType::AssetFormat.metadataFileExtension));
				if (sceneAssetDirectory.Exists())
				{
					IO::Path sceneAssetDirectoryBase = sceneAssetDirectory;
					uint32 counter = 2;
					while (!sceneAssetDirectory.IsDirectory())
					{
						sceneAssetDirectory = IO::Path::Combine(
							parentDirectory,
							IO::Path::Merge(
								entry.m_name.GetView(),
								IO::Path::StringType().Format("-{}", counter).GetView(),
								Scene3DAssetType::AssetFormat.metadataFileExtension
							)
						);
						counter++;
					}

					if (!sceneAssetDirectory.Exists())
					{
						sceneAssetDirectory.CreateDirectories();
					}
				}
				else
				{
					sceneAssetDirectory.CreateDirectories();
				}

				IO::Path sceneAssetPath = IO::Path::Combine(
					sceneAssetDirectory,
					IO::Path::Merge(entry.m_name.GetView(), Scene3DAssetType::AssetFormat.metadataFileExtension)
				);
				if (!sceneAssetPath.Exists())
				{
					sceneAssetPath =
						IO::Path::Combine(sceneAssetDirectory, IO::Path::Merge(MAKE_PATH("Main"), Scene3DAssetType::AssetFormat.metadataFileExtension));
				}

				Serialization::Data sceneAssetData(sceneAssetPath);
				Asset::Asset sceneAsset(sceneAssetData, Move(sceneAssetPath));
				sceneAsset.SetTypeGuid(Scene3DAssetType::AssetFormat.assetTypeGuid);
				Serialization::Serialize(sceneAssetData, sceneAsset);
				// Ensure changes from prior serialization match up with the asset file
				Serialization::Deserialize(sceneAssetData, sceneAsset);

				{
					// Notify that we're compiling a folder asset
					Assert(sceneAsset.GetMetaDataFilePath().GetParentPath().GetRightMostExtension() == Asset::AssetFormat.metadataFileExtension);
					Asset::Asset folderAsset;
					folderAsset.SetMetaDataFilePath(sceneAsset.GetMetaDataFilePath().GetParentPath());
					folderAsset.SetTypeGuid(Scene3DAssetType::AssetFormat.assetTypeGuid);
					info.m_callback(
						CompileFlags::IsCollection,
						Array<Asset::Asset, 1>{Move(folderAsset)}.GetDynamicView(),
						ArrayView<const Serialization::Data>{}
					);
				}

				RootHierarchyEntry diskEntry;
				Serialization::Deserialize(sceneAssetData, diskEntry);

				diskEntry.m_componentType = ComponentTypes::Scene{sceneAsset.GetGuid()};
				entry.m_componentType = ComponentTypes::Scene{sceneAsset.GetGuid()};

				FixedCapacityVector<ReferenceWrapper<Threading::Job>> jobs(Memory::Reserve, node.mNumMeshes);

				for (const uint32 meshIndex : ArrayView<const uint32>{node.mMeshes, node.mNumMeshes})
				{
					const aiMesh& __restrict mesh = *info.m_scene.mMeshes[meshIndex];
					const aiMaterial& __restrict aiMaterial = *info.m_scene.mMaterials[mesh.mMaterialIndex];

					const aiString aiMaterialName = aiMaterial.GetName();
					const ConstStringView meshName{aiMaterialName.C_Str(), aiMaterialName.length};

					MeshCompilationResult compilationResult = GetOrCompileMesh(
						info,
						mesh,
						FilterSourceName(meshName),
						sceneAssetDirectory,
						Invalid,
						diskEntry,
						MeshCompilationFlags::CreateParentComponents
					);
					if (compilationResult.pJob != nullptr)
					{
						jobs.EmplaceBack(*compilationResult.pJob);
					}
				}

				Threading::Job* pFinishedJob = &Threading::CreateCallback(
					[sceneAssetData = Move(sceneAssetData),
				   diskEntry = Move(diskEntry),
				   sceneAsset = Move(sceneAsset),
				   callback = CompileCallback(info.m_callback),
				   compileFlags = info.m_compileFlags & ~CompileFlags::WasDirectlyRequested](Threading::JobRunnerThread&) mutable
					{
						Serialization::Serialize(sceneAssetData, sceneAsset);
						Serialization::Serialize(sceneAssetData, diskEntry);
						// Ensure changes from prior serialization match up with the asset file
						Serialization::Deserialize(sceneAssetData, sceneAsset);
						callback(
							compileFlags | CompileFlags::Compiled,
							ArrayView<Asset::Asset>{sceneAsset},
							ArrayView<const Serialization::Data>{sceneAssetData}
						);
					},
					Threading::JobPriority::AssetCompilation
				);

				for (Threading::Job& job : jobs)
				{
					job.AddSubsequentStage(*pFinishedJob);
					info.m_jobsToQueue.EmplaceBack(job);
				}

				info.m_jobDependencies.EmplaceBack(*pFinishedJob);
			}
		}
		else
		{
			const OptionalIterator<const aiLight*> pLight = info.m_remainingLights.FindIf(
				[sourceEntryName = entry.m_sourceName](const aiLight* __restrict pLight)
				{
					return sourceEntryName == ConstStringView{pLight->mName.C_Str(), pLight->mName.length};
				}
			);
			if (pLight)
			{
				const aiLight& __restrict light = **pLight;

				const Math::Vector3f forwardDirection = ConvertVector(light.mDirection);
				const Math::Vector3f upDirection = ConvertVector(light.mUp);

				const Math::LocalTransform transform = ConvertAndAdjustTransform(Math::LocalTransform{
					Math::Matrix3x3f{upDirection.Cross(forwardDirection), upDirection, forwardDirection},
					ConvertVector(light.mPosition)
				});

				entry.m_transform = entry.m_transform.Transform(transform);

				switch (light.mType)
				{
					case aiLightSourceType::aiLightSource_POINT:
					{
						ComponentTypes::PointLight& lightEntry = entry.m_componentType.GetOrEmplace<ComponentTypes::PointLight>();
						Math::Vector3f color{light.mColorDiffuse.r, light.mColorDiffuse.g, light.mColorDiffuse.b};
						const float intensity = Math::Max(color.x, color.y, color.z);
						color /= intensity;
						lightEntry.m_color = Math::Color{color.x, color.y, color.z};

						// assimp light.mAttenuationQuadratic can't be relied on, instead we just derive radius from power

						lightEntry.m_influenceRadius = Math::Radiusf::FromMeters(ConvertBlenderLightIntensityToEngineInfluenceRadius(intensity));
					}
					break;
					case aiLightSourceType::aiLightSource_DIRECTIONAL:
					{
						ComponentTypes::DirectionalLight& lightEntry = entry.m_componentType.GetOrEmplace<ComponentTypes::DirectionalLight>();
						Math::Vector3f color{light.mColorDiffuse.r, light.mColorDiffuse.g, light.mColorDiffuse.b};
						const float intensity = Math::Max(color.x, color.y, color.z);
						color /= intensity;
						lightEntry.m_color = Math::Color{color.x, color.y, color.z};
						lightEntry.m_intensity = intensity;
					}
					break;
					case aiLightSourceType::aiLightSource_SPOT:
					{
						ComponentTypes::SpotLight& lightEntry = entry.m_componentType.GetOrEmplace<ComponentTypes::SpotLight>();

						lightEntry.m_fieldOfView = Math::Anglef::FromRadians(light.mAngleOuterCone);
						Math::Vector3f color{light.mColorDiffuse.r, light.mColorDiffuse.g, light.mColorDiffuse.b};
						const float intensity = Math::Max(color.x, color.y, color.z);
						color /= intensity;
						lightEntry.m_color = Math::Color{color.x, color.y, color.z};

						// assimp light.mAttenuationQuadratic can't be relied on, instead we just derive radius from power

						lightEntry.m_influenceRadius = Math::Radiusf::FromMeters(ConvertBlenderLightIntensityToEngineInfluenceRadius(intensity));
					}
					break;
					default:
						break;
				}
			}
			else
			{
				const OptionalIterator<const aiCamera*> pCamera = info.m_remainingCameras.FindIf(
					[sourceEntryName = entry.m_sourceName](const aiCamera* __restrict pCamera)
					{
						return sourceEntryName == ConstStringView{pCamera->mName.C_Str(), pCamera->mName.length};
					}
				);
				if (pCamera)
				{
					const aiCamera& __restrict camera = **pCamera;

					const Math::Vector3f forwardDirection = ConvertVector(camera.mLookAt);
					const Math::Vector3f upDirection = ConvertVector(camera.mUp);

					const Math::LocalTransform transform = ConvertAndAdjustTransform(Math::LocalTransform{
						Math::Matrix3x3f{upDirection.Cross(forwardDirection), upDirection, forwardDirection},
						ConvertVector(camera.mPosition)
					});
					entry.m_transform = entry.m_transform.Transform(transform);

					Entity::CameraProperties& cameraEntry = entry.m_componentType.GetOrEmplace<Entity::CameraProperties>();
					cameraEntry.m_fieldOfView = Math::Anglef::FromRadians(camera.mHorizontalFOV);
					cameraEntry.m_nearPlane = camera.mClipPlaneNear; // Math::Radiusf::FromMeters(camera.mClipPlaneNear),
					cameraEntry.m_farPlane = camera.mClipPlaneFar;   // Math::Radiusf::FromMeters(camera.mClipPlaneFar),
				}
			}
		}
	}
#if COMPILER_MSVC
#pragma optimize("", on)
#endif

	[[nodiscard]] static HierarchyEntry ProcessHierarchy(
		HierarchyProcessInfo& __restrict info,
		const aiNode& __restrict node,
		const HierarchyEntry& templateEntry,
		String&& nodeName,
		const ConstStringView nodeSourceName,
		const IO::PathView parentDirectory,
		const Guid parentAssetGuid
	)
	{

		HierarchyEntry entry = templateEntry;

		entry.m_transform = ConvertAndAdjustTransform(ConvertTransform(node.mTransformation));
		entry.m_name = Forward<String>(nodeName);
		entry.m_sourceName = nodeSourceName;

		ProcessNode(info, node, entry, parentDirectory, parentAssetGuid);

		entry.m_children.Reserve(entry.m_children.GetSize() + node.mNumChildren);

		for (const aiNode* __restrict childNode : ArrayView<const aiNode* const>{node.mChildren, node.mNumChildren})
		{
			const ConstStringView childEntryName{childNode->mName.C_Str(), childNode->mName.length};
			String childNodeName = FilterSourceName(childEntryName);

			if(const OptionalIterator<HierarchyEntry> pIterator = entry.m_children.FindIf(
			       [childNodeName = childNodeName.GetView()](const HierarchyEntry& existingEntry)
			       {
				       return childNodeName == existingEntry.m_name;
			       }
			   ))
			{
				*pIterator = ProcessHierarchy(info, *childNode, *pIterator, Move(childNodeName), childEntryName, parentDirectory, parentAssetGuid);
				Assert(pIterator->IsValid());
			}
			else
			{
				HierarchyEntry childEntry =
					ProcessHierarchy(info, *childNode, {}, Move(childNodeName), childEntryName, parentDirectory, parentAssetGuid);
				if (childEntry.IsValid())
				{
					entry.m_children.EmplaceBack(Move(childEntry));
				}
			}
		}

		return entry;
	}
#endif

	bool SceneObjectCompiler::IsUpToDate(
		const Platform::Type,
		const Serialization::Data&,
		const Asset::Asset&,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		[[maybe_unused]] const Asset::Context& context
	)
	{
		return false;
	}

	Threading::Job* SceneObjectCompiler::Compile(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		[[maybe_unused]] const AssetCompiler::Plugin& assetCompiler,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		[[maybe_unused]] const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
#if SUPPORT_ASSIMP
		auto compileJob = [pInfo = UniquePtr<HierarchyProcessInfo>{},
		                   pImporter = UniquePtr<Assimp::Importer>{},
		                   flags,
		                   callback,
		                   sourceFilePath,
		                   assetData = Move(assetData),
		                   asset = Move(asset),
		                   &assetCompiler,
		                   platforms,
		                   context = context,
		                   sourceContext = sourceContext](Threading::JobRunnerThread& currentThread, Threading::Job& rootJob) mutable
		{
			Asset::Owners owners(asset.GetMetaDataFilePath(), Move(context));

			pImporter.CreateInPlace();
			const aiScene* pScene;

			{
				FixedSizeVector<char> sourceFileContents;
				{
					IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary);
					if (UNLIKELY(!sourceFile.IsValid()))
					{
						callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
						return;
					}

					sourceFileContents = FixedSizeVector<char>(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
					if (UNLIKELY(!sourceFile.ReadIntoView(sourceFileContents.GetView())))
					{
						callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
						return;
					}
				}

				const String sourceFilePathString(sourceFilePath.GetView().GetStringView());

				constexpr int defaultFlags = aiProcess_GlobalScale | aiProcess_Triangulate | aiProcess_CalcTangentSpace |
				                             aiProcess_GenSmoothNormals | aiProcess_RemoveRedundantMaterials | aiProcess_PopulateArmatureData |
				                             aiProcess_JoinIdenticalVertices;

				// Load as meters
				pImporter->SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.f);
				pImporter->SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);
				pImporter->SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
				pImporter->SetPropertyBool(AI_CONFIG_IMPORT_FBX_OPTIMIZE_EMPTY_ANIMATION_CURVES, false);

				pScene = pImporter->ReadFileFromMemory(
					sourceFileContents.GetData(),
					sourceFileContents.GetSize(),
					defaultFlags,
					sourceFilePathString.GetZeroTerminated()
				);
			}

			if (pScene == nullptr)
			{
				callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
				return;
			}

			RootHierarchyEntry rootEntry;
			rootEntry.m_name = String(asset.GetName().GetView());

			Serialization::Deserialize(assetData, rootEntry);

			uint32 nodeCount = 0;
			IterateNodeHierarchy(
				[&nodeCount](const aiNode&)
				{
					nodeCount++;
				},
				*pScene->mRootNode
			);

			pInfo.CreateInPlace(HierarchyProcessInfo{
				*pScene,
				assetCompiler,
				currentThread,
				platforms,
				flags,
				owners.m_context,
				sourceContext,
				owners.m_context.ComputeFullDependencyDatabase(),
				sourceFilePath,
				sourceFilePath.GetParentPath(),
				asset.GetDirectory().GetParentPath(),
				FixedCapacityVector<const aiCamera*>(ArrayView<const aiCamera* const>{pScene->mCameras, pScene->mNumCameras}),
				FixedCapacityVector<const aiLight*>(ArrayView<const aiLight* const>{pScene->mLights, pScene->mNumLights}),
				FixedCapacityVector<const aiAnimation*>(ArrayView<const aiAnimation* const>{pScene->mAnimations, pScene->mNumAnimations}),
				JobsContainer(
					Memory::Reserve,
					nodeCount + pScene->mNumMeshes * 2 + pScene->mNumMaterials * 2 + pScene->mNumTextures * 2 + pScene->mNumAnimations
				),
				JobsContainer(
					Memory::Reserve,
					nodeCount + pScene->mNumMeshes * 2 + pScene->mNumMaterials * 2 + pScene->mNumTextures * 2 + pScene->mNumAnimations
				),
				callback
			});

			{
				const IO::Path assetDirectory(asset.GetDirectory());
				if (!assetDirectory.Exists())
				{
					assetDirectory.CreateDirectories();
				}
			}

			auto isSimpleMeshScene = [](const aiScene& __restrict scene)
			{
				if ((scene.mNumMeshes == 1) & (scene.mNumCameras == 0) & (scene.mNumLights == 0) & (scene.mNumAnimations == 0))
				{
					const aiNode& __restrict rootNode = *scene.mRootNode;
					return ((rootNode.mNumChildren == 0) & (scene.mNumMeshes == 1)) ||
					       ((rootNode.mNumChildren == 1) & (rootNode.mChildren[0]->mNumMeshes == 1) & (rootNode.mChildren[0]->mNumChildren == 0));
				}

				return false;
			};

			HierarchyEntry parentEntry;
			if (isSimpleMeshScene(*pScene))
			{
				asset.SetTypeGuid(MeshSceneAssetType::AssetFormat.assetTypeGuid);
				Serialization::Serialize(assetData, asset);

				const aiNode* pMeshNode = pScene->mRootNode;
				if (pMeshNode->mNumChildren > 0)
				{
					pMeshNode = pMeshNode->mChildren[0];
				}

				const aiMesh& __restrict mesh = *pScene->mMeshes[pMeshNode->mMeshes[0]];

				MeshCompilationResult compilationResult = GetOrCompileMesh(
					*pInfo,
					mesh,
					String(rootEntry.m_name),
					asset.GetDirectory(),
					rootEntry,
					parentEntry,
					MeshCompilationFlags::CreateParentComponents
				);
				if (compilationResult.pJob != nullptr)
				{
					pInfo->m_jobsToQueue.EmplaceBack(*compilationResult.pJob);
				}
				rootEntry = parentEntry.m_children[0];
			}
			else
			{
				asset.SetTypeGuid(Scene3DAssetType::AssetFormat.assetTypeGuid);
				Serialization::Serialize(assetData, asset);

				{
					// Notify that we're compiling a folder asset
					Assert(asset.GetMetaDataFilePath().GetParentPath().GetRightMostExtension() == Asset::AssetFormat.metadataFileExtension);
					Asset::Asset folderAsset;
					folderAsset.SetMetaDataFilePath(asset.GetMetaDataFilePath().GetParentPath());
					folderAsset.SetTypeGuid(Scene3DAssetType::AssetFormat.assetTypeGuid);
					pInfo->m_callback(
						CompileFlags::IsCollection,
						Array<Asset::Asset, 1>{Move(folderAsset)}.GetDynamicView(),
						ArrayView<const Serialization::Data>{}
					);
				}

				rootEntry = ProcessHierarchy(
					*pInfo,
					*pScene->mRootNode,
					Move(rootEntry),
					FilterSourceName(rootEntry.m_name),
					rootEntry.m_name,
					asset.GetDirectory(),
					asset.GetGuid()
				);

				if (pInfo->m_remainingAnimations.HasElements())
				{
					// Check if we can compile the remaining animations
					if (const Optional<Guid> skeletonAssetGuid = Serialization::Reader(assetData).Read<Guid>("skeleton"))
					{
						if (const Optional<const Asset::DatabaseEntry*> pSkeletonAssetEntry = pInfo->m_fullOwnersAssetDatabase.GetAssetEntry(*skeletonAssetGuid))
						{
							QueuedSkeletonCompilationInfo& queuedSkeletonInfo =
								pInfo->m_queuedSkeletonCompilationsMap
									.Emplace(reinterpret_cast<const aiNode*>(&skeletonAssetGuid), QueuedSkeletonCompilationInfo())
									->second;

							const IO::File skeletonFile(IO::Path(pSkeletonAssetEntry->GetBinaryFilePath()), IO::AccessModeFlags::ReadBinary);
							if (skeletonFile.IsValid() && queuedSkeletonInfo.m_skeleton.Load(skeletonFile))
							{
								const bool isSingleAnimation = (!rootEntry.IsValid() || rootEntry.m_componentType.Is<ComponentTypes::SkeletonMesh>()) &&
								                               pInfo->m_remainingAnimations.GetSize() == 1;
								if (isSingleAnimation)
								{
									const aiAnimation& __restrict animation = *pInfo->m_remainingAnimations[0];
									const ConstStringView sourceAnimationName{animation.mName.C_Str(), animation.mName.length};

									HierarchyProcessInfo& info = *pInfo;
									Threading::Job* pAnimationJob = &Threading::CreateCallback(
										[pInfo = Move(pInfo),
									   &animation,
									   sourceAnimationName = String(sourceAnimationName),
									   animationFilePath = IO::Path(asset.GetMetaDataFilePath()),
									   animationSerializedData = Serialization::Data(assetData),
									   skeletonAssetGuid = *skeletonAssetGuid,
									   &skeleton = queuedSkeletonInfo.m_skeleton,
									   callback = CompileCallback(info.m_callback)](Threading::JobRunnerThread&) mutable
										{
											CompileAnimation(
												*pInfo,
												animation,
												sourceAnimationName,
												animationFilePath,
												animationSerializedData,
												skeletonAssetGuid,
												skeleton,
												callback
											);
										},
										Threading::JobPriority::AssetCompilation
									);
									info.m_jobsToQueue.EmplaceBack(*pAnimationJob);

									for (Threading::Job& compilationJob : info.m_jobsToQueue)
									{
										compilationJob.Queue(currentThread);
									}

									return;
								}
								else
								{
									for (const aiAnimation* __restrict pAnimation : pInfo->m_remainingAnimations)
									{
										IO::Path animationTargetDirectory = IO::Path::Combine(pInfo->m_rootDirectory, MAKE_PATH("Animations"));
										if (!animationTargetDirectory.Exists())
										{
											animationTargetDirectory.CreateDirectories();
										}

										const ConstStringView sourceAnimationName{pAnimation->mName.C_Str(), pAnimation->mName.length};
										IO::Path animationFilePath = IO::Path::Combine(
											animationTargetDirectory,
											IO::Path::Merge(
												FilterSourceName(sourceAnimationName).GetView(),
												Animation::AnimationAssetType::AssetFormat.metadataFileExtension
											)
										);
										Serialization::Data animationSerializedData(animationFilePath);

										Guid animationAssetGuid;
										{
											Serialization::Reader reader(animationSerializedData);
											animationAssetGuid = reader.ReadWithDefaultValue<Guid>("guid", Guid::Generate());
											Serialization::Writer writer(animationSerializedData.GetDocument(), animationSerializedData);
											writer.Serialize("guid", animationAssetGuid);
										}

										Threading::Job* pAnimationJob = &Threading::CreateCallback(
											[&info = *pInfo,
										   &animation = *pAnimation,
										   sourceAnimationName = String(sourceAnimationName),
										   animationFilePath = Move(animationFilePath),
										   animationSerializedData = Move(animationSerializedData),
										   skeletonAssetGuid = *skeletonAssetGuid,
										   &skeleton = queuedSkeletonInfo.m_skeleton,
										   callback = CompileCallback(pInfo->m_callback)](Threading::JobRunnerThread&) mutable
											{
												CompileAnimation(
													info,
													animation,
													sourceAnimationName,
													animationFilePath,
													animationSerializedData,
													skeletonAssetGuid,
													skeleton,
													callback
												);
											},
											Threading::JobPriority::AssetCompilation
										);
										pInfo->m_jobsToQueue.EmplaceBack(*pAnimationJob);
									}
								}
							}
						}
					}
				}
			}

			if (rootEntry.IsValid())
			{
				HierarchyProcessInfo& info = *pInfo;
				Threading::Job* pFinishCompilationJob = &Threading::CreateCallback(
					[rootEntry = Move(rootEntry),
				   assetData = Move(assetData),
				   asset = Move(asset),
				   callback = Move(callback),
				   flags,
				   pInfo = Move(pInfo)](Threading::JobRunnerThread&) mutable
					{
						rootEntry.m_componentType = ComponentTypes::Scene{asset.GetGuid()};

						Serialization::Serialize(assetData, rootEntry);
						// Ensure changes from prior serialization match up with the asset file
						Serialization::Deserialize(assetData, asset);
						callback(flags | CompileFlags::Compiled, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					},
					Threading::JobPriority::AssetCompilation
				);

				for (Threading::Job& compilationJob : info.m_jobDependencies)
				{
					compilationJob.AddSubsequentStage(*pFinishCompilationJob);
				}

				pFinishCompilationJob->AddSubsequentStage(rootJob);

				if (info.m_jobsToQueue.HasElements())
				{
					for (Threading::Job& compilationJob : info.m_jobsToQueue)
					{
						compilationJob.AddSubsequentStage(*pFinishCompilationJob);
						compilationJob.Queue(currentThread);
					}
				}
				else
				{
					pFinishCompilationJob->Queue(currentThread);
				}
			}
			else
			{
				for (Threading::Job& compilationJob : pInfo->m_jobsToQueue)
				{
					compilationJob.Queue(currentThread);
				}

				callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
			}
		};

		using CompileJob = decltype(compileJob);
		struct CompilationJob final : public Threading::Job
		{
			CompilationJob(CompileJob&& callback)
				: Job(Priority::AssetCompilation)
				, m_callback(Forward<CompileJob>(callback))
			{
			}

			enum class State : uint8
			{
				AwaitingStart,
				AwaitingCompletion
			};

			virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override
			{
				Assert(m_state == State::AwaitingStart);
				m_state = State::AwaitingCompletion;
				m_callback(thread, *this);
			}

			Result OnExecute(Threading::JobRunnerThread&) override
			{
				switch (m_state)
				{
					case State::AwaitingStart:
						return Result::AwaitExternalFinish;
					case State::AwaitingCompletion:
						return Result::FinishedAndDelete;
					default:
						ExpectUnreachable();
				}
			}
		protected:
			State m_state = State::AwaitingStart;
			CompileJob m_callback;
		};

		return new CompilationJob(Move(compileJob));
#else
		callback(flags, {}, {});
		return nullptr;
#endif
	}

#if SUPPORT_ASSIMP
	struct SceneExporterJob final : public Threading::Job
	{
		SceneExporterJob(
			ExportedCallback&& callback,
			Asset::Asset&& asset,
			const IO::PathView targetFormat,
			RootHierarchyEntry&& rootEntry,
			Asset::Database&& assetDatabase
		)
			: Job(Priority::AssetCompilation)
			, m_callback(Forward<ExportedCallback>(callback))
			, m_targetFormat(targetFormat)
			, m_rootEntry(Forward<RootHierarchyEntry>(rootEntry))
			, m_assetDatabase(Forward<Asset::Database>(assetDatabase))
		{
			const String assetName(asset.GetName().GetView());
			m_exportedScene.mName.Set(assetName.GetZeroTerminated());

			const uint32 propertyCount = 3;
			m_exportedScene.mMetaData = aiMetadata::Alloc(propertyCount);

			m_exportedScene.mMetaData->Set(0, "UnitScaleFactor", 100.0); // Convert meters to centimeters
			m_exportedScene.mMetaData->Set(1, "UpAxis", 2);
			m_exportedScene.mMetaData->Set(2, "FrontAxis", 1);

			m_exportedScene.mRootNode = new aiNode();

			m_sceneLookupMap.Emplace(asset.GetGuid(), RootHierarchyEntry{});
		}

		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override
		{
			switch (m_state)
			{
				case State::TraversingAssets:
					ExpectUnreachable();
				case State::AwaitingTraversalFinish:
				{
					if (m_dependencyCount.FetchSubtract(1) == 1)
					{
						Queue(thread);
					}
				}
				break;
				case State::AwaitingScenePopulationFinish:
				{
					if (m_dependencyCount.FetchSubtract(1) == 1)
					{
						Queue(thread);
					}
				}
				break;
			}
		}

		[[nodiscard]] virtual Result OnExecute(Threading::JobRunnerThread&) override
		{
			switch (m_state)
			{
				case State::TraversingAssets:
				{
					m_dependencyCount = 1;
					TraverseAssets(m_rootEntry, Math::Identity);
					m_state = State::AwaitingTraversalFinish;
					return Result::AwaitExternalFinish;
				}
				case State::AwaitingTraversalFinish:
				{
					m_dependencyCount = 1;
					PopulateScene();
					m_state = State::AwaitingScenePopulationFinish;
					return Result::AwaitExternalFinish;
				}
				case State::AwaitingScenePopulationFinish:
				{
					[[maybe_unused]] const bool wasPopulated = PopulateNode(*m_exportedScene.mRootNode, m_rootEntry);
					Assert(wasPopulated);

					UniquePtr<Assimp::Exporter> pExporter = UniquePtr<Assimp::Exporter>::Make();

					// assimp format identifiers are the extension in lowercase without the dot
					String formatID{m_targetFormat.GetView().GetStringView().GetSubstringFrom(1)};
					formatID.MakeLower();

					const unsigned int preprocessingFlags{aiProcess_GlobalScale | aiProcess_EmbedTextures | aiProcess_RemoveRedundantMaterials};
					const aiExportDataBlob* pExportDataBlob =
						pExporter->ExportToBlob(&m_exportedScene, formatID.GetZeroTerminated(), preprocessingFlags);
					if (pExportDataBlob != nullptr)
					{
						Assert(pExportDataBlob->next == nullptr, "TODO: return sub-files");

						m_callback(ConstByteView{reinterpret_cast<const ByteType*>(pExportDataBlob->data), pExportDataBlob->size}, m_targetFormat);
						return Result::FinishedAndDelete;
					}

					m_callback({}, m_targetFormat);
					return Result::FinishedAndDelete;
				}
			}
			ExpectUnreachable();
		}

		struct TextureInfo
		{
			aiTexture* pTexture;
			IO::Path path;
		};
		using TextureMap = UnorderedMap<Guid, TextureInfo, Guid::Hash>;

		[[nodiscard]] static bool AddMaterial(
			aiScene& exportedScene,
			const ConstZeroTerminatedStringView materialName,
			const Rendering::MaterialInstanceAsset& materialInstanceAsset,
			const Rendering::MaterialAsset& materialAsset,
			const TextureMap& textureLookupMap,
			const uint32 materialIndex
		)
		{
			aiMaterial* pMaterial = new aiMaterial();

			aiString name;
			name.Set(materialName);
			pMaterial->AddProperty(&name, AI_MATKEY_NAME);

			const ArrayView<const Rendering::MaterialAsset::DescriptorBinding, uint8> descriptorBindings = materialAsset.GetDescriptorBindings();
			const ArrayView<const Rendering::MaterialInstanceAsset::DescriptorContent, uint8> descriptorContents =
				materialInstanceAsset.m_descriptorContents.GetView();
			for (const Rendering::MaterialAsset::DescriptorBinding& descriptorBinding : descriptorBindings)
			{
				const uint8 descriptorIndex = descriptorBindings.GetIteratorIndex(&descriptorBinding);
				const Rendering::MaterialInstanceAsset::DescriptorContent& descriptorContent = descriptorContents[descriptorIndex];

				switch (descriptorBinding.m_type)
				{
					case Rendering::DescriptorContentType::Texture:
					{
						auto textureIt = textureLookupMap.Find(descriptorContent.GetTextureAssetGuid());
						if (textureIt != textureLookupMap.end())
						{
							String textureValue{textureIt->second.path.GetView().GetStringView()};
							aiString texturePath;
							texturePath.Set(textureValue.GetZeroTerminated());

							switch (descriptorBinding.m_samplerInfo.m_texturePreset)
							{
								case Rendering::TexturePreset::Diffuse:
								case Rendering::TexturePreset::DiffuseWithAlphaMask:
								case Rendering::TexturePreset::DiffuseWithAlphaTransparency:
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0));
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_BASE_COLOR, 0));
									break;
								case Rendering::TexturePreset::Normals:
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_NORMALS, 0));
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_NORMAL_CAMERA, 0));
									break;
								case Rendering::TexturePreset::Metalness:
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_METALNESS, 0));
									break;
								case Rendering::TexturePreset::Roughness:
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE_ROUGHNESS, 0));
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_SHININESS, 0));
									break;
								case Rendering::TexturePreset::AmbientOcclusion:
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_AMBIENT_OCCLUSION, 0));
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_AMBIENT, 0));
									break;
								case Rendering::TexturePreset::EnvironmentCubemapDiffuseHDR:
								case Rendering::TexturePreset::EnvironmentCubemapSpecular:
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_REFLECTION, 0));
									break;
								case Rendering::TexturePreset::EmissionColor:
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_EMISSION_COLOR, 0));
									break;
								case Rendering::TexturePreset::Alpha:
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_OPACITY, 0));
									break;
								case Rendering::TexturePreset::EmissionFactor:
									pMaterial->AddProperty(&texturePath, AI_MATKEY_TEXTURE(aiTextureType_EMISSION_COLOR, 0));
									break;
								case Rendering::TexturePreset::Greyscale8:
								case Rendering::TexturePreset::GreyscaleWithAlpha8:
								case Rendering::TexturePreset::Explicit:
								case Rendering::TexturePreset::Depth:
								case Rendering::TexturePreset::BRDF:
								case Rendering::TexturePreset::Unknown:
								case Rendering::TexturePreset::Count:
									break;
							}
						}
						else
						{
							return false;
						}
					}
					break;
					case Rendering::DescriptorContentType::Invalid:
						break;
				}
			}

			const ArrayView<const Rendering::PushConstantDefinition, uint8> pushConstantDefinitions = materialAsset.GetPushConstants();
			const ArrayView<const Rendering::PushConstant, uint8> pushConstants = materialInstanceAsset.m_pushConstants.GetView();
			for (const Rendering::PushConstantDefinition& pushConstantDefinition : pushConstantDefinitions)
			{
				const uint8 pushConstantIndex = pushConstantDefinitions.GetIteratorIndex(&pushConstantDefinition);
				const Rendering::PushConstant& pushConstant = pushConstants[pushConstantIndex];

				switch (pushConstantDefinition.m_preset)
				{
					case Rendering::PushConstantPreset::DiffuseColor:
					{
						const Math::Color color{pushConstant.m_value.GetExpected<Math::Color>()};
						aiColor4D aiColor = aiColor4D{color.r, color.g, color.b, color.a};
						pMaterial->AddProperty(&aiColor, 1, "$clr.base");
						pMaterial->AddProperty(&aiColor, 1, "$clr.diffuse");
					}
					break;
					case Rendering::PushConstantPreset::EmissiveColor:
					{
						const Math::Color color{pushConstant.m_value.GetExpected<Math::Color>()};
						aiColor4D aiColor = aiColor4D{color.r, color.g, color.b, color.a};
						pMaterial->AddProperty(&aiColor, 1, "$clr.emissive");
					}
					break;
					case Rendering::PushConstantPreset::AmbientColor:
					{
						const Math::Color color{pushConstant.m_value.GetExpected<Math::Color>()};
						aiColor4D aiColor = aiColor4D{color.r, color.g, color.b, color.a};
						pMaterial->AddProperty(&aiColor, 1, "$clr.ambient");
					}
					break;
					case Rendering::PushConstantPreset::ReflectiveColor:
					{
						const Math::Color color{pushConstant.m_value.GetExpected<Math::Color>()};
						aiColor4D aiColor = aiColor4D{color.r, color.g, color.b, color.a};
						pMaterial->AddProperty(&aiColor, 1, "$clr.reflective");
					}
					break;
					case Rendering::PushConstantPreset::Metalness:
					{
						const float value{pushConstant.m_value.GetExpected<float>()};
						pMaterial->AddProperty(&value, 1, "$clr.metallicFactor");
						pMaterial->AddProperty(&value, 1, "$mat.reflectivity");
					}
					break;
					case Rendering::PushConstantPreset::Roughness:
					{
						const float value{pushConstant.m_value.GetExpected<float>()};
						pMaterial->AddProperty(&value, 1, "$clr.roughnessFactor");
					}
					break;
					case Rendering::PushConstantPreset::Emissive:
					{
						const float value{pushConstant.m_value.GetExpected<float>()};
						pMaterial->AddProperty(&value, 1, "$clr.emissiveIntensity");
					}
					break;
					case Rendering::PushConstantPreset::Unknown:
					case Rendering::PushConstantPreset::Speed:
						break;
				}
			}

			exportedScene.mMaterials[materialIndex] = pMaterial;
			return true;
		}

		struct MeshKey
		{
			Guid meshGuid;
			Guid materialInstanceGuid;

			[[nodiscard]] bool operator==(const MeshKey& other) const
			{
				return (meshGuid == other.meshGuid) && (materialInstanceGuid == other.materialInstanceGuid);
			}

			struct Hash
			{
				size operator()(const MeshKey& key) const
				{
					return Math::Hash(Guid::Hash{}(key.meshGuid), Guid::Hash{}(key.materialInstanceGuid));
				}
			};
		};

		[[nodiscard]] static bool AddMesh(
			aiScene& exportedScene,
			const ConstZeroTerminatedStringView meshName,
			const ConstByteView meshData,
			const uint32 meshIndex,
			const uint32 materialIndex
		)
		{
			Rendering::StaticObject staticObject = Rendering::StaticObject(meshData);

			aiMesh* pMesh = new aiMesh();
			pMesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
			const Rendering::Index vertexCount = staticObject.GetVertexCount();
			const Rendering::Index indexCount = staticObject.GetIndexCount();
			const Rendering::Index triangleCount = indexCount / 3;
			pMesh->mNumVertices = vertexCount;

			aiVector3D* const pVertices = new aiVector3D[vertexCount];
			Rendering::Index index = 0;
			for (const Rendering::VertexPosition position : staticObject.GetVertexElementView<Rendering::VertexPosition>())
			{
				pVertices[index] = ConvertVector(position);
				index++;
			}

			pMesh->mVertices = pVertices;

			aiVector3D* const pNormals = new aiVector3D[vertexCount];
			aiVector3D* const pTangents = new aiVector3D[vertexCount];
			aiVector3D* const pBitangents = new aiVector3D[vertexCount];
			index = 0;
			for (const Rendering::VertexNormals normals : staticObject.GetVertexElementView<Rendering::VertexNormals>())
			{
				const Math::Vector3f normal = normals.normal;
				const Math::Vector4f tangentAndSign = normals.tangent;
				const Math::Vector3f tangent{tangentAndSign.x, tangentAndSign.y, tangentAndSign.z};
				const Math::Vector3f bitangent = normal.Cross(tangent) * tangentAndSign.w;

				pNormals[index] = ConvertVector(normal);
				pTangents[index] = ConvertVector(tangent);
				pBitangents[index] = ConvertVector(bitangent);
				index++;
			}
			pMesh->mNormals = pNormals;
			pMesh->mTangents = pTangents;
			pMesh->mBitangents = pBitangents;

			for (uint8 vertexColorSlotIndex = 0, vertexColorSlotCount = staticObject.GetUsedVertexColorSlotCount();
			     vertexColorSlotIndex < vertexColorSlotCount;
			     ++vertexColorSlotCount)
			{
				aiColor4D* const pVertexColors = new aiColor4D[vertexCount];
				index = 0;
				for (const Rendering::VertexColors vertexColors : staticObject.GetVertexElementView<Rendering::VertexColors>())
				{
					const Rendering::VertexColor vertexColor = vertexColors[vertexColorSlotIndex];
					pVertexColors[index] = {vertexColor.r, vertexColor.g, vertexColor.b, vertexColor.a};
					index++;
				}
				pMesh->mColors[vertexColorSlotIndex] = pVertexColors;
			}

			aiVector3D* const pTextureCoords = new aiVector3D[vertexCount];
			index = 0;
			for (const Rendering::VertexTextureCoordinate textureCoordinate :
			     staticObject.GetVertexElementView<Rendering::VertexTextureCoordinate>())
			{
				pTextureCoords[index] = {textureCoordinate.x, 1.f - textureCoordinate.y, 0};
				index++;
			}
			pMesh->mTextureCoords[0] = pTextureCoords;
			pMesh->mNumUVComponents[0] = 2;

			pMesh->mNumFaces = triangleCount;
			aiFace* const pFaces = new aiFace[triangleCount];
			const RestrictedArrayView<Rendering::Index, Rendering::Index> indices = staticObject.GetIndices();
			for (auto remainingIndices = indices; remainingIndices.HasElements(); remainingIndices += 3)
			{
				Assert(remainingIndices.GetSize() >= 3);
				const Rendering::Index triangleIndex = indices.GetIteratorIndex(remainingIndices.begin()) / 3;
				uint32* const pIndices = new uint32[3];
				pIndices[0] = remainingIndices[0];
				pIndices[1] = remainingIndices[1];
				pIndices[2] = remainingIndices[2];

				aiFace& face = pFaces[triangleIndex];
				face.mNumIndices = 3;
				face.mIndices = pIndices;
			}
			pMesh->mFaces = pFaces;

			// TODO: Bones if skinned

			pMesh->mMaterialIndex = materialIndex;

			pMesh->mName.Set(meshName);

			const Math::BoundingBox boundingBox = staticObject.GetBoundingBox();
			pMesh->mAABB = aiAABB{ConvertVector(boundingBox.GetMinimum()), ConvertVector(boundingBox.GetMaximum())};

			Assert(meshIndex < exportedScene.mNumMeshes);
			exportedScene.mMeshes[meshIndex] = pMesh;

			return true;
		}

		struct MaterialInstanceInfo
		{
			uint32 materialIndex;
			Rendering::MaterialInstanceAsset asset;
		};
		using MaterialInstanceMap = UnorderedMap<Guid, MaterialInstanceInfo, Guid::Hash>;

		struct MaterialInfo
		{
			MaterialInfo() = default;
			MaterialInfo(MaterialInfo&& other)
				: asset(Move(other.asset))
				, requestingMaterialInstances(Move(other.requestingMaterialInstances))
			{
			}

			Rendering::MaterialAsset asset;
			Threading::Mutex callbackMutex;
			Vector<Guid> requestingMaterialInstances;
		};
		using MaterialMap = UnorderedMap<Guid, MaterialInfo, Guid::Hash>;

		struct MeshInfo
		{
			uint32 index;
			String name;
		};
		using MeshMap = UnorderedMap<MeshKey, MeshInfo, MeshKey::Hash>;

		using SceneMap = UnorderedMap<Guid, RootHierarchyEntry, Guid::Hash>;

		[[nodiscard]] bool PopulateNode(aiNode& node, const HierarchyEntry& entry)
		{
			if (entry.m_componentType.Is<ComponentTypes::Scene>())
			{
				const ComponentTypes::Scene& scene = entry.m_componentType.GetExpected<ComponentTypes::Scene>();
				auto sceneIt = m_sceneLookupMap.Find(scene.m_sceneAssetGuid);
				if (LIKELY(sceneIt != m_sceneLookupMap.end()))
				{
					const uint32 childCount = entry.m_children.GetSize() + sceneIt->second.m_children.GetSize();
					node.mNumChildren = childCount;
					node.mChildren = new aiNode*[childCount];

					uint32 nextChildIndex = 0;

					for (const HierarchyEntry& childEntry : entry.m_children)
					{
						if (LIKELY(childEntry.m_name.HasElements()))
						{
							aiNode* pChildNode = new aiNode();
							pChildNode->mName.Set(childEntry.m_name.GetZeroTerminated());

							pChildNode->mTransformation = ConvertTransform(ConvertAndAdjustTransformInverse(childEntry.m_transform));

							if (PopulateNode(*pChildNode, childEntry))
							{
								node.mChildren[nextChildIndex] = pChildNode;
								nextChildIndex++;
							}
							else
							{
								delete pChildNode;
							}
						}
					}

					for (const HierarchyEntry& childEntry : sceneIt->second.m_children)
					{
						aiNode* pChildNode = new aiNode();
						pChildNode->mName.Set(childEntry.m_name.GetZeroTerminated());

						pChildNode->mTransformation = ConvertTransform(ConvertAndAdjustTransformInverse(childEntry.m_transform));

						if (PopulateNode(*pChildNode, childEntry))
						{
							node.mChildren[nextChildIndex] = pChildNode;
							nextChildIndex++;
						}
						else
						{
							delete pChildNode;
						}
					}
					node.mNumChildren = nextChildIndex;

					return node.mNumChildren > 0;
				}
			}

			entry.m_componentType.Visit(
				[](const ComponentTypes::PointLight&)
				{
				},
				[](const ComponentTypes::DirectionalLight&)
				{
				},
				[](const ComponentTypes::SpotLight&)
				{
				},
				[](const Entity::CameraProperties&)
				{
				},
				[](const ComponentTypes::Scene&)
				{
				},
				[](const ComponentTypes::SimpleComponent&)
				{
				},
				[&node, &meshLookupMap = m_meshLookupMap](const ComponentTypes::StaticMesh& staticMesh)
				{
					auto meshIt = meshLookupMap.Find(MeshKey{staticMesh.m_meshAssetGuid, staticMesh.m_materialInstanceAssetGuid});
					if (meshIt != meshLookupMap.end())
					{
						node.mNumMeshes = 1;
						node.mMeshes = new unsigned int[1];
						node.mMeshes[0] = meshIt->second.index;
					}
				},
				[&node, &meshLookupMap = m_meshLookupMap](const ComponentTypes::SkinnedMesh& skinnedMesh)
				{
					auto meshIt = meshLookupMap.Find(MeshKey{skinnedMesh.m_meshAssetGuid, skinnedMesh.m_materialInstanceAssetGuid});
					if (meshIt != meshLookupMap.end())
					{
						node.mNumMeshes = 1;
						node.mMeshes = new unsigned int[1];
						node.mMeshes[0] = meshIt->second.index;
					}
				},
				[](const ComponentTypes::SkeletonMesh&)
				{
				},
				[](const ComponentTypes::Physics::BoxCollider&)
				{
				},
				[](const ComponentTypes::Physics::CapsuleCollider&)
				{
				},
				[](const ComponentTypes::Physics::SphereCollider&)
				{
				},
				[](const ComponentTypes::Physics::InfinitePlaneCollider&)
				{
				},
				[](const ComponentTypes::Physics::MeshCollider&)
				{
				},
				[]()
				{
				}
			);

			uint32 childCount = entry.m_children.GetSize();
			uint32 nextChildIndex = 0;
			node.mNumChildren = childCount;
			node.mChildren = new aiNode*[childCount];

			for (const HierarchyEntry& childEntry : entry.m_children)
			{
				if (LIKELY(childEntry.m_name.HasElements()))
				{
					aiNode* pChildNode = new aiNode();
					pChildNode->mName.Set(childEntry.m_name.GetZeroTerminated());

					pChildNode->mTransformation = ConvertTransform(ConvertAndAdjustTransformInverse(childEntry.m_transform));

					if (PopulateNode(*pChildNode, childEntry))
					{
						node.mChildren[nextChildIndex] = pChildNode;
						nextChildIndex++;
					}
					else
					{
						delete pChildNode;
					}
				}
			}
			node.mNumChildren = nextChildIndex;
			return node.mNumChildren > 0 || node.mNumMeshes > 0 || entry.m_componentType.Is<ComponentTypes::PointLight>() ||
			       entry.m_componentType.Is<ComponentTypes::DirectionalLight>() || entry.m_componentType.Is<ComponentTypes::SpotLight>() ||
			       entry.m_componentType.Is<Entity::CameraProperties>();
		}

		void TraverseAssets(const HierarchyEntry& entry, const Math::WorldTransform parentTransform)
		{
			const Math::WorldTransform worldTransform = parentTransform.Transform(entry.m_transform);

			entry.m_componentType.Visit(
				[this, lightName = entry.m_name.GetZeroTerminated(), worldTransform](const ComponentTypes::PointLight& pointLight)
				{
					aiLight* pLight = new aiLight();
					pLight->mName.Set(lightName);
					pLight->mType = aiLightSource_POINT;

					pLight->mPosition = ConvertVector(worldTransform.GetLocation());
					pLight->mDirection = ConvertVector(worldTransform.GetForwardColumn());
					pLight->mUp = ConvertVector(worldTransform.GetUpColumn());

					const float lightIntensity = ConvertEngineLightRadiusToBlenderInfluenceIntensity(pointLight.m_influenceRadius.GetMeters());

					const Math::Vector3f color = Math::Vector3f{pointLight.m_color.r, pointLight.m_color.g, pointLight.m_color.b} * lightIntensity;
					pLight->mColorDiffuse = {color.x, color.y, color.z};

					m_lights.EmplaceBack(pLight);
				},
				[this, lightName = entry.m_name.GetZeroTerminated(), worldTransform](const ComponentTypes::DirectionalLight& directionalLight)
				{
					aiLight* pLight = new aiLight();
					pLight->mName.Set(lightName);
					pLight->mType = aiLightSource_DIRECTIONAL;

					pLight->mPosition = ConvertVector(worldTransform.GetLocation());
					pLight->mDirection = ConvertVector(worldTransform.GetForwardColumn());
					pLight->mUp = ConvertVector(worldTransform.GetUpColumn());

					const float lightIntensity = directionalLight.m_intensity;

					const Math::Vector3f color = Math::Vector3f{directionalLight.m_color.r, directionalLight.m_color.g, directionalLight.m_color.b} *
				                               lightIntensity;
					pLight->mColorDiffuse = {color.x, color.y, color.z};

					m_lights.EmplaceBack(pLight);
				},
				[this, lightName = entry.m_name.GetZeroTerminated(), worldTransform](const ComponentTypes::SpotLight& spotLight)
				{
					aiLight* pLight = new aiLight();
					pLight->mName.Set(lightName);
					pLight->mType = aiLightSource_SPOT;

					pLight->mPosition = ConvertVector(worldTransform.GetLocation());
					pLight->mDirection = ConvertVector(worldTransform.GetForwardColumn());
					pLight->mUp = ConvertVector(worldTransform.GetUpColumn());

					const float lightIntensity = ConvertEngineLightRadiusToBlenderInfluenceIntensity(spotLight.m_influenceRadius.GetMeters());

					const Math::Vector3f color = Math::Vector3f{spotLight.m_color.r, spotLight.m_color.g, spotLight.m_color.b} * lightIntensity;
					pLight->mColorDiffuse = {color.x, color.y, color.z};

					pLight->mAngleOuterCone = spotLight.m_fieldOfView.GetRadians();

					m_lights.EmplaceBack(pLight);
				},
				[this, cameraName = entry.m_name.GetZeroTerminated(), worldTransform](const Entity::CameraProperties& cameraProperties)
				{
					aiCamera* pCamera = new aiCamera();
					pCamera->mName.Set(cameraName);

					pCamera->mPosition = ConvertVector(worldTransform.GetLocation());
					pCamera->mUp = ConvertVector(worldTransform.GetUpColumn());
					pCamera->mLookAt = ConvertVector(worldTransform.GetForwardColumn());

					pCamera->mHorizontalFOV = cameraProperties.m_fieldOfView.GetRadians();
					pCamera->mClipPlaneNear = cameraProperties.m_nearPlane;
					pCamera->mClipPlaneFar = cameraProperties.m_farPlane;

					m_cameras.EmplaceBack(pCamera);
				},
				[this, worldTransform](const ComponentTypes::Scene& scene)
				{
					if (TryEmplaceScene(scene.m_sceneAssetGuid))
					{
						Asset::Manager& assetManager = System::Get<Asset::Manager>();
						if (assetManager.HasAsset(scene.m_sceneAssetGuid))
						{
							m_dependencyCount++;
							Threading::Job* pJob = assetManager.RequestAsyncLoadAssetMetadata(
								scene.m_sceneAssetGuid,
								Threading::JobPriority::AssetCompilation,
								[this, sceneAssetGuid = scene.m_sceneAssetGuid, worldTransform](const ConstByteView data)
								{
									if (LIKELY(data.HasElements()))
									{
										Serialization::Data sceneAssetData(
											ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
										);
										RootHierarchyEntry rootEntry;
										Serialization::Deserialize(sceneAssetData, rootEntry);

										{
											Threading::SharedLock lock(m_sceneLookupMapMutex);
											auto it = m_sceneLookupMap.Find(sceneAssetGuid);
											Assert(it != m_sceneLookupMap.end());
											it->second = rootEntry;
										}

										TraverseAssets(rootEntry, worldTransform);
									}
									else
									{
										m_failedAny = true;
									}

									if (m_dependencyCount.FetchSubtract(1) == 1)
									{
										Threading::JobRunnerThread::GetCurrent()->Queue(*this);
									}
								}
							);
							if (pJob != nullptr)
							{
								pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
							}
						}
						else
						{
							const Optional<const Asset::DatabaseEntry*> pSceneAssetEntry = m_assetDatabase.GetAssetEntry(scene.m_sceneAssetGuid);
							if (UNLIKELY(!pSceneAssetEntry.IsValid()))
							{
								m_failedAny = true;
								return;
							}

							Serialization::Data sceneAssetData(pSceneAssetEntry->m_path);
							RootHierarchyEntry rootEntry;
							Serialization::Deserialize(sceneAssetData, rootEntry);

							{
								Threading::SharedLock lock(m_sceneLookupMapMutex);
								auto it = m_sceneLookupMap.Find(scene.m_sceneAssetGuid);
								Assert(it != m_sceneLookupMap.end());
								it->second = rootEntry;
							}

							TraverseAssets(rootEntry, worldTransform);
						}
					}
				},
				[](const ComponentTypes::SimpleComponent&)
				{
				},
				[this, &entry](const ComponentTypes::StaticMesh& staticMesh)
				{
					FindOrEmplaceMaterialInstance(staticMesh.m_materialInstanceAssetGuid);

					IO::Path meshBinaryAssetPath = System::Get<Asset::Manager>().GetAssetBinaryPath(staticMesh.m_meshAssetGuid);
					if (meshBinaryAssetPath.IsEmpty())
					{
						const Optional<const Asset::DatabaseEntry*> pMeshAssetEntry = m_assetDatabase.GetAssetEntry(staticMesh.m_meshAssetGuid);
						meshBinaryAssetPath = IO::Path{pMeshAssetEntry.IsValid() ? pMeshAssetEntry->GetBinaryFilePath() : IO::PathView{}};
					}
					if (meshBinaryAssetPath.HasElements())
					{
						FindOrEmplaceMesh(staticMesh.m_meshAssetGuid, staticMesh.m_materialInstanceAssetGuid, entry.m_name);
					}
				},
				[this, &entry](const ComponentTypes::SkinnedMesh& skinnedMesh)
				{
					FindOrEmplaceMaterialInstance(skinnedMesh.m_materialInstanceAssetGuid);

					IO::Path meshBinaryAssetPath = System::Get<Asset::Manager>().GetAssetBinaryPath(skinnedMesh.m_meshAssetGuid);
					if (meshBinaryAssetPath.IsEmpty())
					{
						const Optional<const Asset::DatabaseEntry*> pMeshAssetEntry = m_assetDatabase.GetAssetEntry(skinnedMesh.m_meshAssetGuid);
						meshBinaryAssetPath = IO::Path{pMeshAssetEntry.IsValid() ? pMeshAssetEntry->GetBinaryFilePath() : IO::PathView{}};
					}
					if (meshBinaryAssetPath.HasElements())
					{
						FindOrEmplaceMesh(skinnedMesh.m_meshAssetGuid, skinnedMesh.m_materialInstanceAssetGuid, entry.m_name);
					}
				},
				[](const ComponentTypes::SkeletonMesh&)
				{
				},
				[](const ComponentTypes::Physics::BoxCollider&)
				{
				},
				[](const ComponentTypes::Physics::CapsuleCollider&)
				{
				},
				[](const ComponentTypes::Physics::SphereCollider&)
				{
				},
				[](const ComponentTypes::Physics::InfinitePlaneCollider&)
				{
				},
				[](const ComponentTypes::Physics::MeshCollider&)
				{
				},
				[]()
				{
				}
			);

			for (const HierarchyEntry& childEntry : entry.m_children)
			{
				TraverseAssets(childEntry, worldTransform);
			}
		}

		template<typename Callback>
		void LoadAsset(
			const Guid assetGuid,
			const IO::PathView path,
			Callback&& callback,
			const Math::Range<size> dataRange = Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
		)
		{
			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			if (assetManager.HasAsset(assetGuid))
			{
				Threading::Job* pJob = assetManager.RequestAsyncLoadAssetPath(
					assetGuid,
					path,
					Threading::JobPriority::AssetCompilation,
					[callback = Forward<Callback>(callback)](const ConstByteView data) mutable
					{
						callback(data);
					},
					{},
					dataRange
				);
				if (pJob != nullptr)
				{
					pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
				}
			}
			else
			{
				const Optional<const Asset::DatabaseEntry*> pAssetEntry = m_assetDatabase.GetAssetEntry(assetGuid);
				if (UNLIKELY(!pAssetEntry.IsValid()))
				{
					callback({});
					return;
				}

				FixedSizeVector<char, IO::File::SizeType> fileContents;
				{
					IO::File file(pAssetEntry->m_path, IO::AccessModeFlags::ReadBinary);
					if (UNLIKELY(!file.IsValid()))
					{
						callback({});
						return;
					}

					fileContents = FixedSizeVector<char, IO::File::SizeType>(Memory::ConstructWithSize, Memory::Uninitialized, file.GetSize());
					if (UNLIKELY(!file.ReadIntoView(fileContents.GetView())))
					{
						callback({});
						return;
					}
				}

				return callback(fileContents.GetSubView(dataRange.GetMinimum(), dataRange.GetSize()));
			}
		}

		template<typename Callback>
		void LoadAssetMetadata(const Guid assetGuid, Callback&& callback)
		{
			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			IO::Path assetPath = assetManager.GetAssetPath(assetGuid);
			if (assetPath.IsEmpty())
			{
				assetPath = m_assetDatabase.GetAssetEntry(assetGuid)->m_path;
			}
			LoadAsset(assetGuid, assetPath, Forward<Callback>(callback));
		}

		template<typename Callback>
		void LoadAssetBinary(const Guid assetGuid, Callback&& callback)
		{
			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			IO::Path assetPath = assetManager.GetAssetBinaryPath(assetGuid);
			if (assetPath.IsEmpty())
			{
				assetPath = IO::Path{m_assetDatabase.GetAssetEntry(assetGuid)->GetBinaryFilePath()};
			}
			LoadAsset(assetGuid, assetPath, Forward<Callback>(callback));
		}

		void PopulateScene()
		{
			const uint32 materialCount = m_materialInstanceLookupMap.GetSize();
			const uint32 textureCount = m_textureLookupMap.GetSize();
			const uint32 meshCount = m_meshLookupMap.GetSize();

			m_exportedScene.mNumMaterials = materialCount;
			m_exportedScene.mMaterials = new aiMaterial*[materialCount];

			m_exportedScene.mNumTextures = textureCount;
			m_exportedScene.mTextures = new aiTexture*[textureCount];

			m_exportedScene.mNumMeshes = meshCount;
			m_exportedScene.mMeshes = new aiMesh*[meshCount];

			if (m_lights.HasElements())
			{
				m_exportedScene.mNumLights = m_lights.GetSize();
				m_exportedScene.mLights = new aiLight*[m_exportedScene.mNumLights];
				ArrayView<aiLight*>{m_exportedScene.mLights, m_exportedScene.mNumLights}.CopyFrom(m_lights.GetView());
			}

			if (m_cameras.HasElements())
			{
				m_exportedScene.mNumCameras = m_cameras.GetSize();
				m_exportedScene.mCameras = new aiCamera*[m_exportedScene.mNumCameras];
				ArrayView<aiCamera*>{m_exportedScene.mCameras, m_exportedScene.mNumCameras}.CopyFrom(m_cameras.GetView());
			}

			Asset::Manager& assetManager = System::Get<Asset::Manager>();

			for (auto it = m_materialInstanceLookupMap.begin(), endIt = m_materialInstanceLookupMap.end(); it != endIt; ++it)
			{
				if (it->second.asset.IsValid())
				{
					auto materialIt = m_materialLookupMap.Find(it->second.asset.m_materialAssetGuid);
					Assert(materialIt != m_materialLookupMap.end());
					const String materialInstanceName{assetManager.GetAssetName(it->first)};
					if (UNLIKELY(!SceneExporterJob::AddMaterial(
								m_exportedScene,
								materialInstanceName.GetZeroTerminated(),
								it->second.asset,
								materialIt->second.asset,
								m_textureLookupMap,
								it->second.materialIndex
							)))
					{
						m_failedAny = true;
					}
				}
				else
				{
					m_failedAny = true;
				}
			}

			uint32 nextTextureIndex = 0;
			for (auto it = m_textureLookupMap.begin(), endIt = m_textureLookupMap.end(); it != endIt; ++it)
			{
				if (it->second.pTexture != nullptr)
				{
					m_exportedScene.mTextures[nextTextureIndex++] = it->second.pTexture;
				}
				else
				{
					m_failedAny = true;
				}
			}
			m_exportedScene.mNumTextures = nextTextureIndex;

			for (auto it = m_meshLookupMap.begin(), endIt = m_meshLookupMap.end(); it != endIt; ++it)
			{
				const SceneExporterJob::MeshKey meshKey = it->first;
				const SceneExporterJob::MeshInfo& meshInfo = it->second;
				const auto materialIt = m_materialInstanceLookupMap.Find(meshKey.materialInstanceGuid);
				Assert(materialIt != m_materialInstanceLookupMap.end());
				const uint32 materialIndex = materialIt->second.materialIndex;

				m_dependencyCount++;
				LoadAssetBinary(
					meshKey.meshGuid,
					[this, meshName = String(meshInfo.name), meshIndex = meshInfo.index, materialIndex](const ConstByteView data)
					{
						if (LIKELY(data.HasElements()))
						{
							if (UNLIKELY(!SceneExporterJob::AddMesh(m_exportedScene, meshName, data, meshIndex, materialIndex)))
							{
								m_failedAny = true;
							}
						}
						else
						{
							m_failedAny = true;
						}

						if (m_dependencyCount.FetchSubtract(1) == 1)
						{
							Threading::JobRunnerThread::GetCurrent()->Queue(*this);
						}
					}
				);
			}
		}

		void FindOrEmplaceMaterialInstance(Guid materialInstanceGuid)
		{
			{
				Threading::SharedLock lock(m_materialInstanceLookupMapMutex);
				if (m_materialInstanceLookupMap.Contains(materialInstanceGuid))
				{
					return;
				}
			}

			Threading::UniqueLock lock(m_materialInstanceLookupMapMutex);
			auto it = m_materialInstanceLookupMap.Find(materialInstanceGuid);
			if (it == m_materialInstanceLookupMap.end())
			{
				const uint32 materialIndex = m_nextMaterialIndex++;
				m_materialInstanceLookupMap.Emplace(materialInstanceGuid, MaterialInstanceInfo{materialIndex});
				lock.Unlock();

				m_dependencyCount++;
				LoadAssetMetadata(
					materialInstanceGuid,
					[this, materialInstanceGuid](const ConstByteView data)
					{
						if (LIKELY(data.HasElements()))
						{
							Asset::Manager& assetManager = System::Get<Asset::Manager>();

							Serialization::Data materialInstanceData(
								ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
							);

							Guid materialAssetGuid;
							{
								Threading::SharedLock lock(m_materialInstanceLookupMapMutex);
								auto it = m_materialInstanceLookupMap.Find(materialInstanceGuid);
								Assert(it != m_materialInstanceLookupMap.end());

								Rendering::MaterialInstanceAsset& materialInstanceAsset = it->second.asset =
									Rendering::MaterialInstanceAsset(materialInstanceData, assetManager.GetAssetPath(materialInstanceGuid));
								materialAssetGuid = materialInstanceAsset.m_materialAssetGuid;
							}

							FindOrEmplaceMaterial(materialAssetGuid, materialInstanceGuid);
						}
						else
						{
							m_failedAny = true;
						}

						if (m_dependencyCount.FetchSubtract(1) == 1)
						{
							Threading::JobRunnerThread::GetCurrent()->Queue(*this);
						}
					}
				);
			}
		}

		void FindOrEmplaceMaterial(Guid materialGuid, Guid requestingMaterialInstanceGuid)
		{
			{
				Threading::SharedLock lock(m_materialLookupMapMutex);
				auto it = m_materialLookupMap.Find(materialGuid);
				if (it != m_materialLookupMap.end())
				{
					Threading::UniqueLock callbackLock(it->second.callbackMutex);
					if (it->second.asset.IsValid())
					{
						Assert(it->second.requestingMaterialInstances.GetSize() == 0);
						it->second.requestingMaterialInstances.EmplaceBack(requestingMaterialInstanceGuid);
						OnMaterialLoaded(it->second);
					}
					else
					{
						it->second.requestingMaterialInstances.EmplaceBack(requestingMaterialInstanceGuid);
					}
					return;
				}
			}

			Threading::UniqueLock lock(m_materialLookupMapMutex);
			auto it = m_materialLookupMap.Find(materialGuid);
			if (it != m_materialLookupMap.end())
			{
				Threading::UniqueLock callbackLock(it->second.callbackMutex);
				if (it->second.asset.IsValid())
				{
					Assert(it->second.requestingMaterialInstances.GetSize() == 0);
					it->second.requestingMaterialInstances.EmplaceBack(requestingMaterialInstanceGuid);
					OnMaterialLoaded(it->second);
				}
				else
				{
					it->second.requestingMaterialInstances.EmplaceBack(requestingMaterialInstanceGuid);
				}
			}
			else
			{
				it = m_materialLookupMap.Emplace(materialGuid, MaterialInfo{});
				it->second.requestingMaterialInstances.EmplaceBack(requestingMaterialInstanceGuid);
				lock.Unlock();

				m_dependencyCount++;
				LoadAssetMetadata(
					materialGuid,
					[this, materialGuid](const ConstByteView data)
					{
						if (LIKELY(data.HasElements()))
						{
							Serialization::Data materialData(
								ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
							);
							Asset::Manager& assetManager = System::Get<Asset::Manager>();
							{
								Threading::SharedLock lock(m_materialLookupMapMutex);
								auto it = m_materialLookupMap.Find(materialGuid);
								Assert(it != m_materialLookupMap.end());

								Threading::UniqueLock callbackLock(it->second.callbackMutex);
								it->second.asset = Rendering::MaterialAsset(materialData, assetManager.GetAssetPath(materialGuid));
								OnMaterialLoaded(it->second);
							}
						}
						else
						{
							m_failedAny = true;
						}

						if (m_dependencyCount.FetchSubtract(1) == 1)
						{
							Threading::JobRunnerThread::GetCurrent()->Queue(*this);
						}
					}
				);
			}
		}
		void FindOrEmplaceMesh(const Guid meshGuid, const Guid materialInstanceGuid, const ConstStringView name)
		{
			{
				Threading::SharedLock lock(m_meshLookupMapMutex);
				if (m_meshLookupMap.Contains(MeshKey{meshGuid, materialInstanceGuid}))
				{
					return;
				}
			}

			Threading::UniqueLock lock(m_meshLookupMapMutex);
			auto it = m_meshLookupMap.Find(MeshKey{meshGuid, materialInstanceGuid});
			if (it == m_meshLookupMap.end())
			{
				m_meshLookupMap.Emplace(MeshKey{meshGuid, materialInstanceGuid}, MeshInfo{m_nextMeshIndex++, String{name}});
			}
		}
		bool TryEmplaceScene(Guid sceneGuid)
		{
			{
				Threading::SharedLock lock(m_sceneLookupMapMutex);
				if (m_sceneLookupMap.Contains(sceneGuid))
				{
					return false;
				}
			}

			Threading::UniqueLock lock(m_sceneLookupMapMutex);
			auto it = m_sceneLookupMap.Find(sceneGuid);
			if (it != m_sceneLookupMap.end())
			{
				return false;
			}
			else
			{
				m_sceneLookupMap.Emplace(sceneGuid, RootHierarchyEntry{});
				return true;
			}
		}
		void OnMaterialLoaded(MaterialInfo& materialInfo)
		{
			const ArrayView<const Rendering::MaterialAsset::DescriptorBinding, uint8> descriptorBindings =
				materialInfo.asset.GetDescriptorBindings();

			for (const Guid materialInstanceGuid : materialInfo.requestingMaterialInstances)
			{
				Threading::SharedLock lock(m_materialInstanceLookupMapMutex);
				auto materialInstanceIt = m_materialInstanceLookupMap.Find(materialInstanceGuid);
				Assert(materialInstanceIt != m_materialInstanceLookupMap.end());

				const MaterialInstanceInfo& materialInstanceInfo = materialInstanceIt->second;
				const ArrayView<const Rendering::MaterialInstanceAsset::DescriptorContent, uint8> descriptorContents =
					materialInstanceInfo.asset.m_descriptorContents.GetView();
				for (const Rendering::MaterialAsset::DescriptorBinding& descriptorBinding : descriptorBindings)
				{
					const uint8 descriptorIndex = descriptorBindings.GetIteratorIndex(&descriptorBinding);
					const Rendering::MaterialInstanceAsset::DescriptorContent& descriptorContent = descriptorContents[descriptorIndex];

					switch (descriptorBinding.m_type)
					{
						case Rendering::DescriptorContentType::Texture:
						{
							const Guid textureGuid = descriptorContent.GetTextureAssetGuid();
							FindOrEmplaceTexture(textureGuid);
						}
						break;
						case Rendering::DescriptorContentType::Invalid:
							break;
					}
				}
			}

			materialInfo.requestingMaterialInstances.Clear();
		}

		void FindOrEmplaceTexture(Guid textureGuid)
		{
			{
				Threading::SharedLock lock(m_textureLookupMapMutex);
				if (m_textureLookupMap.Contains(textureGuid))
				{
					return;
				}
			}

			Threading::UniqueLock lock(m_textureLookupMapMutex);
			auto it = m_textureLookupMap.Find(textureGuid);
			if (it == m_textureLookupMap.end())
			{
				m_textureLookupMap.Emplace(textureGuid, TextureInfo{});
				lock.Unlock();

				m_dependencyCount++;
				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				IO::Path assetPath = assetManager.GetAssetPath(textureGuid);
				if (assetPath.IsEmpty())
				{
					assetPath = m_assetDatabase.GetAssetEntry(textureGuid)->m_path;
				}
				// Start by loading metadata
				LoadAsset(
					textureGuid,
					assetPath,
					[this, textureGuid, assetPath](const ConstByteView data) mutable
					{
						if (LIKELY(data.HasElements()))
						{
							Serialization::Data textureData(
								ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
							);

							Rendering::TextureAsset textureAsset(textureData, IO::Path(assetPath));

							// Now load the binary
							IO::Path binaryFilePath = Asset::Asset::CreateBinaryFilePath(
								assetPath,
								Rendering::TextureAsset::GetBinaryFileExtension(Rendering::TextureAsset::BinaryType::BC)
							);

							const Rendering::TextureAsset::BinaryInfo& binaryInfo =
								textureAsset.GetBinaryAssetInfo(Rendering::TextureAsset::BinaryType::BC);

							const Rendering::TextureAsset::MipInfo& mipInfo = binaryInfo.GetMipInfoView()[0];
							const Math::Range<size> mipSize = Math::Range<size>::Make(mipInfo.m_offset, mipInfo.m_size);

							LoadAsset(
								textureGuid,
								binaryFilePath,
								[this, textureGuid, assetPath = Move(assetPath), textureAsset = Move(textureAsset), binaryFilePath](const ConstByteView data
						    ) mutable
								{
									if (LIKELY(data.HasElements()))
									{
										Threading::SharedLock lock(m_textureLookupMapMutex);
										auto it = m_textureLookupMap.Find(textureGuid);
										Assert(it != m_textureLookupMap.end());

										aiTexture* pTexture = new aiTexture();

										const Rendering::TextureAsset::BinaryInfo& binaryInfo =
											textureAsset.GetBinaryAssetInfo(Rendering::TextureAsset::BinaryType::BC);

										const Math::Vector2ui resolution = textureAsset.GetResolution();
										gli::texture2d texture(static_cast<gli::format>(binaryInfo.GetFormat()), gli::extent2d{resolution.x, resolution.y}, 1);
										if (LIKELY((ByteView{reinterpret_cast<ByteType*>(texture.data()), texture.size()}
								                  .CopyFrom(ConstByteView{data.GetData(), data.GetDataSize()}))))
										{
											const Rendering::Format targetFormat = Rendering::Format::R8G8B8A8_UNORM_PACK8;
											const Rendering::FormatInfo& targetFormatInfo = Rendering::GetFormatInfo(targetFormat);
											gli::texture2d converted = gli::convert(texture, static_cast<gli::format>(targetFormat));

											const uint32 bitsPerPixel = targetFormatInfo.GetBitsPerPixel();
											const uint32 bytesPerPixel = bitsPerPixel / 8;

											Vector<ByteType> targetData;
											// Write the image data
											int result = stbi_write_png_to_func(
												[](void* context, void* pData, const int size)
												{
													Vector<ByteType>& data = *reinterpret_cast<Vector<ByteType>*>(context);

													data.CopyEmplaceRange(
														data.end(),
														Memory::Uninitialized,
														ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(pData), (uint32)size}
													);
												},
												&targetData,
												resolution.x,
												resolution.y,
												targetFormatInfo.m_componentCount,
												converted.data(),
												resolution.x * bytesPerPixel
											);
											if (result == 1)
											{
												pTexture->mWidth = targetData.GetSize();
												pTexture->mHeight = 0;
												pTexture->achFormatHint[0] = 'p';
												pTexture->achFormatHint[1] = 'n';
												pTexture->achFormatHint[2] = 'g';
												pTexture->achFormatHint[3] = '\0';

												aiTexel* pData = new aiTexel[pTexture->mWidth];
												Memory::CopyWithoutOverlap(pData, targetData.GetData(), targetData.GetDataSize());
												pTexture->pcData = pData;

												IO::Path path = IO::Path::Merge(Guid::Generate().ToString().GetView(), MAKE_PATH(".png"));
												String texturePath(path.GetView().GetStringView());
												pTexture->mFilename.Set(texturePath.GetZeroTerminated());

												it->second.pTexture = pTexture;
												it->second.path = Move(path);
											}
											else
											{
												m_failedAny = true;
											}
										}
										else
										{
											m_failedAny = true;
										}
									}
									else
									{
										m_failedAny = true;
									}

									if (m_dependencyCount.FetchSubtract(1) == 1)
									{
										Threading::JobRunnerThread::GetCurrent()->Queue(*this);
									}
								},
								mipSize
							);
						}
						else
						{
							m_failedAny = true;

							if (m_dependencyCount.FetchSubtract(1) == 1)
							{
								Threading::JobRunnerThread::GetCurrent()->Queue(*this);
							}
						}
					}
				);
			}
		}
	private:
		aiScene m_exportedScene;
		const ExportedCallback m_callback;
		IO::Path m_targetFormat;
		const RootHierarchyEntry m_rootEntry;
		const Asset::Database m_assetDatabase;

		Threading::Atomic<bool> m_failedAny{false};
		Threading::Atomic<uint32> m_dependencyCount{0};

		Threading::SharedMutex m_materialInstanceLookupMapMutex;
		MaterialInstanceMap m_materialInstanceLookupMap;
		Threading::SharedMutex m_materialLookupMapMutex;
		MaterialMap m_materialLookupMap;
		Threading::Atomic<uint32> m_nextMaterialIndex{0};

		Threading::SharedMutex m_textureLookupMapMutex;
		TextureMap m_textureLookupMap;
		Threading::Atomic<uint32> m_nextTextureIndex{0};

		Threading::SharedMutex m_meshLookupMapMutex;
		MeshMap m_meshLookupMap;
		Threading::Atomic<uint32> m_nextMeshIndex{0};
		Threading::SharedMutex m_sceneLookupMapMutex;
		SceneMap m_sceneLookupMap;

		Threading::Mutex m_lightsMutex;
		Vector<aiLight*> m_lights;
		Threading::Mutex m_camerasMutex;
		Vector<aiCamera*> m_cameras;

		enum class State : uint8
		{
			TraversingAssets,
			AwaitingTraversalFinish,
			AwaitingScenePopulationFinish
		};
		State m_state{State::TraversingAssets};
	};
#endif

	Threading::Job* SceneObjectCompiler::Export(
		ExportedCallback&& callback,
		const EnumFlags<Platform::Type>,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::PathView targetFormat,
		[[maybe_unused]] const Asset::Context& context
	)
	{
#if SUPPORT_ASSIMP
		RootHierarchyEntry rootEntry;
		Serialization::Deserialize(assetData, rootEntry);

		return new SceneExporterJob(
			Forward<ExportedCallback>(callback),
			Forward<Asset::Asset>(asset),
			targetFormat,
			Move(rootEntry),
			context.ComputeFullDependencyDatabase()
		);
#else
		callback({}, targetFormat);
		return nullptr;
#endif
	}

	Threading::Job* MeshCompiler::Export(
		ExportedCallback&& callback,
		const EnumFlags<Platform::Type>,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::PathView targetFormat,
		[[maybe_unused]] const Asset::Context& context
	)
	{
#if SUPPORT_ASSIMP
		RootHierarchyEntry rootEntry;
		Serialization::Deserialize(assetData, rootEntry);

		Optional<ComponentTypes::StaticMesh> staticMeshComponentType = rootEntry.FindFirstComponentOfTypeRecursive<ComponentTypes::StaticMesh>(
		);
		if (!staticMeshComponentType.IsValid())
		{
			callback({}, targetFormat);
			return nullptr;
		}

		aiScene exportedScene;
		const String assetName(asset.GetName().GetView());
		exportedScene.mName.Set(assetName.GetZeroTerminated());

		const uint32 propertyCount = 3;
		exportedScene.mMetaData = aiMetadata::Alloc(propertyCount);

		exportedScene.mMetaData->Set(0, "UnitScaleFactor", 100.0); // Convert meters to centimeters
		exportedScene.mMetaData->Set(1, "UpAxis", 2);
		exportedScene.mMetaData->Set(2, "FrontAxis", 1);

		exportedScene.mRootNode = new aiNode();

		const uint32 materialCount = 1;
		exportedScene.mNumMaterials = materialCount;
		exportedScene.mMaterials = new aiMaterial*[materialCount];

		const Asset::Database assetDatabase = context.ComputeFullDependencyDatabase();

		{
			const Optional<const Asset::DatabaseEntry*> pMaterialInstanceAssetEntry =
				assetDatabase.GetAssetEntry(staticMeshComponentType->m_materialInstanceAssetGuid);
			if (UNLIKELY(!pMaterialInstanceAssetEntry.IsValid()))
			{
				callback({}, targetFormat);
				return nullptr;
			}

			Serialization::Data materialInstanceData(pMaterialInstanceAssetEntry->m_path);
			Rendering::MaterialInstanceAsset materialInstanceAsset(materialInstanceData, IO::Path(pMaterialInstanceAssetEntry->m_path));

			const Optional<const Asset::DatabaseEntry*> pMaterialAssetEntry =
				assetDatabase.GetAssetEntry(materialInstanceAsset.m_materialAssetGuid);
			if (UNLIKELY(!pMaterialAssetEntry.IsValid()))
			{
				callback({}, targetFormat);
				return nullptr;
			}

			Serialization::Data materialData(pMaterialAssetEntry->m_path);
			Rendering::MaterialAsset materialAsset(materialData, IO::Path(pMaterialAssetEntry->m_path));

			SceneExporterJob::TextureMap textureMap;
			// TODO: Populate

			const String materialInstanceName{pMaterialInstanceAssetEntry->GetName()};
			if (UNLIKELY(!SceneExporterJob::AddMaterial(
						exportedScene,
						materialInstanceName.GetZeroTerminated(),
						materialInstanceAsset,
						materialAsset,
						textureMap,
						0
					)))
			{
				callback({}, targetFormat);
				return nullptr;
			}
		}

		const uint32 meshCount = 1;
		exportedScene.mNumMeshes = meshCount;
		exportedScene.mMeshes = new aiMesh*[meshCount];

		const IO::Path meshPartBinaryPath =
			Asset::Asset::CreateBinaryFilePath(asset.GetMetaDataFilePath(), MeshPartAssetType::AssetFormat.binaryFileExtension);
		const IO::File binaryFile(meshPartBinaryPath, IO::AccessModeFlags::ReadBinary);
		if (!binaryFile.IsValid())
		{
			callback({}, targetFormat);
			return nullptr;
		}

		Vector<ByteType, IO::File::SizeType> meshContents(Memory::ConstructWithSize, Memory::Uninitialized, binaryFile.GetSize());
		if (!binaryFile.ReadIntoView(meshContents.GetView()))
		{
			callback({}, targetFormat);
			return nullptr;
		}

		if (UNLIKELY(!SceneExporterJob::AddMesh(exportedScene, assetName.GetZeroTerminated(), meshContents.GetView(), 0, 0)))
		{
			callback({}, targetFormat);
			return nullptr;
		}

		exportedScene.mRootNode->mNumMeshes = 1;
		exportedScene.mRootNode->mMeshes = new unsigned int[1];
		exportedScene.mRootNode->mMeshes[0] = 0;

		UniquePtr<Assimp::Exporter> pExporter = UniquePtr<Assimp::Exporter>::Make();

		// assimp format identifiers are the extension in lowercase without the dot
		String formatID{targetFormat.GetStringView().GetSubstringFrom(1)};
		formatID.MakeLower();

		const unsigned int preprocessingFlags{aiProcess_GlobalScale | aiProcess_EmbedTextures | aiProcess_RemoveRedundantMaterials};
		const aiExportDataBlob* pExportDataBlob = pExporter->ExportToBlob(&exportedScene, formatID.GetZeroTerminated(), preprocessingFlags);
		if (pExportDataBlob != nullptr)
		{
			Assert(pExportDataBlob->next == nullptr, "TODO: return sub-files");

			callback(ConstByteView{reinterpret_cast<const ByteType*>(pExportDataBlob->data), pExportDataBlob->size}, targetFormat);
			return nullptr;
		}

		callback({}, targetFormat);
		return nullptr;
#else
		callback({}, targetFormat);
		return nullptr;
#endif
	}
}
