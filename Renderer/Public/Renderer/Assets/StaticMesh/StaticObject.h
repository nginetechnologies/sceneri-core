#pragma once

#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexTextureCoordinate.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexColors.h>
#include <Renderer/Index.h>

#include <Common/EnumFlagOperators.h>
#include <Common/EnumFlags.h>
#include <Common/Math/Primitives/BoundingBox.h>
#include <Common/Math/Radius.h>
#include <Common/Math/ForwardDeclarations/Quaternion.h>
#include <Common/Math/CoreNumericTypes.h>
#include <Common/Memory/Allocators/DynamicAllocator.h>
#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>

namespace ngine::IO
{
	struct FileView;
}

namespace ngine::Rendering
{
	struct VertexNormals;

	inline static constexpr uint8 MaximumVertexColorCountOnDisk = 4;

	enum class StaticObjectFlags : uint16
	{
		None = 0,
		UInt8Indices = 1 << 0,
		UInt16Indices = 1 << 1,
		HalfPointPrecisionPositions = 1 << 2,
		HalfPointPrecisionTextureCoordinates = 1 << 3,
		IsVertexColorSlotUsedFirst = 1 << 4,
		IsVertexColorSlotUsedLast = IsVertexColorSlotUsedFirst << (MaximumVertexColorCountOnDisk - 1),
		HasVertexColorSlotAlphaFirst = IsVertexColorSlotUsedLast << 1,
		HasVertexColorSlotAlphaLast = HasVertexColorSlotAlphaFirst << (MaximumVertexColorCountOnDisk - 1)
	};
	ENUM_FLAG_OPERATORS(StaticObjectFlags);

	enum class FloatPrecision : uint8
	{
		Half,
		Single
	};

	[[nodiscard]] constexpr FloatPrecision GetFloatPrecision(const EnumFlags<StaticObjectFlags> flags, const StaticObjectFlags value)
	{
		return flags.IsSet(value) ? FloatPrecision::Half : FloatPrecision::Single;
	}

	enum class IndexType : uint8
	{
		UInt8,
		UInt16,
		UInt32
	};

	[[nodiscard]] constexpr IndexType GetIndexType(const EnumFlags<StaticObjectFlags> flags)
	{
		if (flags.IsSet(StaticObjectFlags::UInt8Indices))
		{
			return IndexType::UInt8;
		}
		else if (flags.IsSet(StaticObjectFlags::UInt16Indices))
		{
			return IndexType::UInt16;
		}
		return IndexType::UInt32;
	}

	struct StaticObject
	{
	protected:
		struct WrittenBounds
		{
			WrittenBounds() = default;

			inline WrittenBounds(const Math::BoundingBox boundingBox)
				: m_min{boundingBox.GetMinimum().x, boundingBox.GetMinimum().y, boundingBox.GetMinimum().z}
				, m_max{boundingBox.GetMaximum().x, boundingBox.GetMaximum().y, boundingBox.GetMaximum().z}
			{
			}

			[[nodiscard]] inline operator Math::BoundingBox() const
			{
				return Math::BoundingBox({m_min[0], m_min[1], m_min[2]}, {m_max[0], m_max[1], m_max[2]});
			}

			float m_min[3];
			float m_max[3];
		};
	public:
		using ChunkSizeType = uint16;

		using VersionType = uint16;
		inline static constexpr VersionType Version = 0u;
		inline static constexpr VersionType FirstCompatibleVersion = 0u;
		inline static constexpr VersionType LastCompatibleVersion = 0u;

		using VertexPosition = Rendering::VertexPosition;
		using VertexNormals = Rendering::VertexNormals;
		using VertexTextureCoordinate = Rendering::VertexTextureCoordinate;

		using AllocatorType = Memory::DynamicAllocator<ByteType, uint32>;

		StaticObject() = default;
		StaticObject(const StaticObject&) = default;
		StaticObject& operator=(const StaticObject&) = default;
		StaticObject(StaticObject&& other)
			: m_vertexCount(other.m_vertexCount)
			, m_indexCount(other.m_indexCount)
			, m_flags(other.m_flags)
			, m_data(Move(other.m_data))
			, m_boundingBox(other.m_boundingBox)
		{
			other.m_vertexCount = 0;
			other.m_indexCount = 0;
			other.m_boundingBox = Math::Zero;
		}
		StaticObject& operator=(StaticObject&& other)
		{
			m_vertexCount = other.m_vertexCount;
			m_indexCount = other.m_indexCount;
			m_flags = Move(other.m_flags);
			m_data = Move(other.m_data);
			m_boundingBox = other.m_boundingBox;

			other.m_vertexCount = 0;
			other.m_indexCount = 0;
			other.m_boundingBox = Math::Zero;
			return *this;
		}

		[[nodiscard]] static uint32 CalculateDataSize(const Index vertexCount, const Index indexCount, const uint8 vertexColorCount);

		template<typename Type>
		[[nodiscard]] static uint32 CalculateDataOffset(const Index vertexCount, const Index indexCount, const uint8 vertexColorCount);

		StaticObject(Memory::ReserveType, const Index vertexCapacity, const Index indexCapacity, const uint8 vertexColorCount = 0)
			: m_data(Memory::Reserve, CalculateDataSize(vertexCapacity, indexCapacity, vertexColorCount))
		{
		}

		StaticObject(
			Memory::ConstructWithSizeType,
			Memory::UninitializedType,
			const Index vertexCount,
			const Index indexCount,
			const EnumFlags<StaticObjectFlags> flags = {}
		)
			: m_vertexCount(vertexCount)
			, m_indexCount(indexCount)
			, m_flags(flags)
			, m_data(Memory::Reserve, CalculateDataSize(vertexCount, indexCount, GetUsedVertexColorSlotCount()))
		{
		}

		void Reserve(const Index vertexCapacity, const Index indexCapacity, const uint8 usedVertexColorSlotCount)
		{
			const uint32 requiredCapacity = CalculateDataSize(vertexCapacity, indexCapacity, usedVertexColorSlotCount);
			if (m_data.GetCapacity() < requiredCapacity)
			{
				m_data.Allocate(static_cast<uint32>(float(requiredCapacity) * 2.2));
			}
		}

		void Resize(Memory::UninitializedType, const Index vertexCount, const Index indexCount, const EnumFlags<StaticObjectFlags> flags = {})
		{
			m_vertexCount = vertexCount;
			m_indexCount = indexCount;
			m_flags = flags;

			Reserve(vertexCount, indexCount, GetUsedVertexColorSlotCount());
		}

		using Flags = StaticObjectFlags;

		StaticObject(ConstByteView data);

		[[nodiscard]] Math::BoundingBox GetBoundingBox() const
		{
			return m_boundingBox;
		}

		[[nodiscard]] Index GetVertexCount() const
		{
			return m_vertexCount;
		}
		[[nodiscard]] Index GetIndexCount() const
		{
			return m_indexCount;
		}

		[[nodiscard]] bool IsValid() const
		{
			return (m_vertexCount != 0) & (m_indexCount != 0);
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS size GetDataSize() const
		{
			return CalculateDataSize(m_vertexCount, m_indexCount, GetUsedVertexColorSlotCount());
		}

		template<typename ElementType>
		[[nodiscard]] PURE_LOCALS_AND_POINTERS ArrayView<ElementType, Index> GetVertexElementView() LIFETIME_BOUND;
		template<typename ElementType>
		[[nodiscard]] PURE_LOCALS_AND_POINTERS ArrayView<const ElementType, Index> GetVertexElementView() const LIFETIME_BOUND;
		[[nodiscard]] PURE_LOCALS_AND_POINTERS ArrayView<Index, Index> GetIndices() LIFETIME_BOUND;
		[[nodiscard]] PURE_LOCALS_AND_POINTERS ArrayView<const Index, Index> GetIndices() const LIFETIME_BOUND;

		void TransformRotation(const Math::Quaternionf quaternion);

		void WriteToFile(const IO::FileView outputFile) const;

		[[nodiscard]] Math::BoundingBox CalculateBoundingBox() const;

		void CalculateAndSetBoundingBox()
		{
			m_boundingBox = CalculateBoundingBox();
		}

		[[nodiscard]] uint8 GetUsedVertexColorSlotCount() const;
		[[nodiscard]] uint8 GetUsedVertexAlphaColorSlotCount() const;
	protected:
		friend struct StaticMesh;

		Index m_vertexCount = 0;
		Index m_indexCount = 0;
		EnumFlags<StaticObjectFlags> m_flags;
		AllocatorType m_data;

		Math::BoundingBox m_boundingBox{Math::Radiusf(0.1_meters)};
	};
}
