#include "Assets/StaticMesh/StaticObject.h"

#include <Renderer/Assets/StaticMesh/VertexColors.h>
#include <Renderer/Assets/StaticMesh/VertexNormals.h>

#include <Common/Math/Half.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Vector3.h>
#include <Common/Math/Vector3/Abs.h>
#include <Common/Math/Quaternion.h>
#include <Common/Memory/Containers/ByteView.h>
#include <Common/Memory/Containers/MultiArrayView.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/IO/Log.h>

namespace ngine::Math
{
	using Vector3h = TVector3<half>;
	using Vector2h = TVector2<half>;
}

namespace ngine::Rendering
{
	using MultiView = MultiArrayView<VertexPosition, VertexNormals, VertexTextureCoordinate, VertexColors, Rendering::Index>;

	uint8 StaticObject::GetUsedVertexColorSlotCount() const
	{
		uint8 count = 0;
		for (uint8 i = 0; i < VertexColors::Size; ++i)
		{
			count += m_flags.IsSet(Flags::IsVertexColorSlotUsedFirst << i);
		}
		return count;
	}

	uint8 StaticObject::GetUsedVertexAlphaColorSlotCount() const
	{
		uint8 count = 0;
		for (uint8 i = 0; i < VertexColors::Size; ++i)
		{
			count += m_flags.IsSet(Flags::HasVertexColorSlotAlphaFirst << i);
		}
		return count;
	}

	StaticObject::StaticObject(ConstByteView data)
	{
		struct Color3Uint8
		{
			uint8 r, g, b;
		};

		[[maybe_unused]] const VersionType version = data.ReadAndSkipWithDefaultValue<VersionType>((uint16)Math::NumericLimits<VersionType>::Max
		);
		m_flags = data.ReadAndSkipWithDefaultValue<Flags>({});
		m_vertexCount = data.ReadAndSkipWithDefaultValue<const Rendering::Index>(0);
		m_indexCount = data.ReadAndSkipWithDefaultValue<const Rendering::Index>(0);

		const bool canRead = [version,
		                      flags = m_flags,
		                      vertexCount = m_vertexCount,
		                      indexCount = m_indexCount,
		                      usedVertexColorSlotCount = GetUsedVertexColorSlotCount()](const ConstByteView data)
		{
			const bool isCompatibleVersion = /*version >= FirstCompatibleVersion && */ version <= LastCompatibleVersion;
			LogWarningIf(!isCompatibleVersion, "Mesh was incompatible with importer, try re-exporting!");

			size requiredSize = 0;

			switch (GetFloatPrecision(flags, Flags::HalfPointPrecisionPositions))
			{
				case FloatPrecision::Single:
					requiredSize += sizeof(Math::UnalignedVector3<float>) * vertexCount;
					break;
				case FloatPrecision::Half:
					requiredSize += sizeof(Math::UnalignedVector3<half>) * vertexCount;
					break;
			}
			requiredSize += sizeof(VertexNormals) * vertexCount;
			switch (GetFloatPrecision(flags, Flags::HalfPointPrecisionTextureCoordinates))
			{
				case FloatPrecision::Single:
					requiredSize += sizeof(Math::UnalignedVector2<float>) * vertexCount;
					break;
				case FloatPrecision::Half:
					requiredSize += sizeof(Math::UnalignedVector2<half>) * vertexCount;
					break;
			}

			if (usedVertexColorSlotCount > 0)
			{
				for (uint8 vertexColorIndex = 0, usedVertexColorCount = Math::Min(usedVertexColorSlotCount, VertexColors::Size);
				     vertexColorIndex < usedVertexColorCount;
				     ++vertexColorIndex)
				{
					const bool hasVertexColors = flags.IsSet(Flags::IsVertexColorSlotUsedFirst << vertexColorIndex);
					if (hasVertexColors)
					{
						const bool hasVertexColorAlpha = flags.IsSet(Flags::HasVertexColorSlotAlphaFirst << vertexColorIndex);
						requiredSize += (hasVertexColorAlpha ? sizeof(Math::ColorByte) : sizeof(Color3Uint8)) * vertexCount;
					}
				}
			}

			switch (GetIndexType(flags))
			{
				case IndexType::UInt32:
					requiredSize += sizeof(uint32) * indexCount;
					break;
				case IndexType::UInt16:
					requiredSize += sizeof(uint16) * indexCount;
					break;
				case IndexType::UInt8:
					requiredSize += sizeof(uint8) * indexCount;
					break;
			}

			requiredSize += sizeof(WrittenBounds);

			return bool(isCompatibleVersion & (data.GetDataSize() >= requiredSize) & (vertexCount > 0) & (indexCount > 0));
		}(data);

		if (LIKELY(canRead))
		{
			m_data = {Memory::Reserve, CalculateDataSize(m_vertexCount, m_indexCount, GetUsedVertexColorSlotCount())};

			LogWarningIf(m_vertexCount == 0 || m_indexCount == 0, "Mesh vertex or index count invalid!");

			// Read vertex positions
			switch (GetFloatPrecision(m_flags, Flags::HalfPointPrecisionPositions))
			{
				case FloatPrecision::Single:
				{
					ArrayView<VertexPosition, Rendering::Index> positions = GetVertexElementView<VertexPosition>();
					for (Rendering::Index vertexIndex = 0; vertexIndex < m_vertexCount; ++vertexIndex)
					{
						positions[vertexIndex] = *data.ReadAndSkip<Math::UnalignedVector3<float>>();
					}
				}
				break;
				case FloatPrecision::Half:
				{
					if (LIKELY(data.GetDataSize() >= sizeof(Math::UnalignedVector3<half>) * m_vertexCount))
					{
						ArrayView<VertexPosition, Rendering::Index> positions = GetVertexElementView<VertexPosition>();
						for (Rendering::Index vertexIndex = 0; vertexIndex < m_vertexCount; ++vertexIndex)
						{
							positions[vertexIndex] = (Math::Vector3f)(Math::Vector3h)*data.ReadAndSkip<Math::UnalignedVector3<half>>();
						}
					}
				}
				break;
			}

			// Read vertex normals
			if (LIKELY(data.GetDataSize() >= sizeof(VertexNormals) * m_vertexCount))
			{
				ArrayView<VertexNormals, Rendering::Index> normals = GetVertexElementView<VertexNormals>();
				for (Rendering::Index vertexIndex = 0; vertexIndex < m_vertexCount; ++vertexIndex)
				{
					normals[vertexIndex] = *data.ReadAndSkip<VertexNormals>();
				}
			}

			// Read vertex texture coordinates
			switch (GetFloatPrecision(m_flags, Flags::HalfPointPrecisionTextureCoordinates))
			{
				case FloatPrecision::Single:
				{
					ArrayView<VertexTextureCoordinate, Rendering::Index> coordinates = GetVertexElementView<VertexTextureCoordinate>();
					for (Rendering::Index vertexIndex = 0; vertexIndex < m_vertexCount; ++vertexIndex)
					{
						coordinates[vertexIndex] = *data.ReadAndSkip<Math::UnalignedVector2<float>>();
					}
				}
				break;
				case FloatPrecision::Half:
				{
					ArrayView<VertexTextureCoordinate, Rendering::Index> coordinates = GetVertexElementView<VertexTextureCoordinate>();
					for (Rendering::Index vertexIndex = 0; vertexIndex < m_vertexCount; ++vertexIndex)
					{
						const Math::Vector2h compressedCoordinates = *data.ReadAndSkip<Math::UnalignedVector2<half>>();
						coordinates[vertexIndex] = Math::Vector2f(Math::Vector2d{Math::Vector2f(compressedCoordinates)});
					}
				}
				break;
			}

			// Read vertex colors
			const uint8 usedVertexColorSlotCount = GetUsedVertexColorSlotCount();
			if (usedVertexColorSlotCount > 0)
			{
				const EnumFlags<Flags> flags = m_flags;

				ArrayView<VertexColors, Rendering::Index> colors = GetVertexElementView<VertexColors>();
				for (uint8 vertexColorIndex = 0, usedVertexColorCount = Math::Min(usedVertexColorSlotCount, VertexColors::Size);
				     vertexColorIndex < usedVertexColorCount;
				     ++vertexColorIndex)
				{
					const bool hasVertexColors = flags.IsSet(Flags::IsVertexColorSlotUsedFirst << vertexColorIndex);
					if (!hasVertexColors)
					{
						continue;
					}

					const bool hasVertexColorAlpha = flags.IsSet(Flags::HasVertexColorSlotAlphaFirst << vertexColorIndex);
					if (hasVertexColorAlpha)
					{
						for (Rendering::Index vertexIndex = 0; vertexIndex < m_vertexCount; ++vertexIndex)
						{
							colors[vertexIndex][vertexColorIndex] = *data.ReadAndSkip<Math::ColorByte>();
						}
					}
					else
					{
						for (Rendering::Index vertexIndex = 0; vertexIndex < m_vertexCount; ++vertexIndex)
						{
							const Color3Uint8 color = *data.ReadAndSkip<Color3Uint8>();
							colors[vertexIndex][vertexColorIndex] = Math::ColorByte{color.r, color.g, color.b, 0};
						}
					}
				}
			}

			switch (GetIndexType(m_flags))
			{
				case IndexType::UInt32:
				{
					if constexpr (TypeTraits::IsSame<uint32, Rendering::Index>)
					{
						[[maybe_unused]] const bool readIndices = data.ReadIntoViewAndSkip(GetIndices());
						LogWarningIf(!readIndices, "Could not read mesh indices");
					}
					else
					{
						ExpectUnreachable();
					}
				}
				break;
				case IndexType::UInt16:
				{
					if constexpr (TypeTraits::IsSame<uint16, Rendering::Index>)
					{
						[[maybe_unused]] const bool readIndices = data.ReadIntoViewAndSkip(GetIndices());
						LogWarningIf(!readIndices, "Could not read mesh indices");
					}
					else
					{
						if (LIKELY(data.GetDataSize() >= sizeof(uint16) * m_indexCount))
						{
							ArrayView<Index, Rendering::Index> indices = GetIndices();
							for (Rendering::Index index = 0; index < m_indexCount; ++index)
							{
								indices[index] = *data.ReadAndSkip<uint16>();
							}
						}
					}
				}
				break;
				case IndexType::UInt8:
				{
					if (LIKELY(data.GetDataSize() >= sizeof(uint8) * m_indexCount))
					{
						static_assert(!TypeTraits::IsSame<uint8, Rendering::Index>);
						ArrayView<Index, Rendering::Index> indices = GetIndices();
						for (Rendering::Index index = 0; index < m_indexCount; ++index)
						{
							indices[index] = *data.ReadAndSkip<uint8>();
						}
					}
				}
				break;
			}

			m_boundingBox = *data.ReadAndSkip<const WrittenBounds>();
		}
		else
		{
			LogWarning("Could not read mesh data!");
			m_vertexCount = 0;
			m_indexCount = 0;
			m_boundingBox = Math::Radiusf(0.1_meters);
		}
	}

	void StaticObject::WriteToFile(const IO::FileView outputFile) const
	{
		outputFile.Write(Version);

		const Rendering::Index maximumIndex = m_vertexCount - 1;

		// Check if we can reduce the size of indices
		EnumFlags<Flags> flags = [](const Rendering::Index maximumIndex)
		{
			if (maximumIndex <= Math::NumericLimits<uint8>::Max)
			{
				return Flags::UInt8Indices;
			}
			else if (maximumIndex <= Math::NumericLimits<uint16>::Max)
			{
				return Flags::UInt16Indices;
			}
			return Flags{};
		}(maximumIndex);

		// Check if we can compress positions without losing precision
		flags |= [](const ArrayView<const VertexPosition, Rendering::Index> vertices)
		{
			EnumFlags<Flags> result = Flags::HalfPointPrecisionPositions;

			for (const VertexPosition& __restrict vertexPosition : vertices)
			{
				// If the position can't be represented as 16-bit float, set the single point flag
				const typename VertexPosition::BoolType in16BitRange = (vertexPosition >= Math::Vector3f{(float)Math::NumericLimits<half>::Min}) &
				                                                       (vertexPosition <= Math::Vector3f{(float)Math::NumericLimits<half>::Max});
				result &= ~(Flags::HalfPointPrecisionPositions * !in16BitRange.AreAllSet());

				// Check if the compressed epsilon converted back retains enough precision
				const Math::Vector3h compressed = (Math::Vector3h)vertexPosition;

				const Math::Vector3f decompressedPosition = (Math::Vector3f)compressed;

				const Math::Vector3f delta = Math::Abs(decompressedPosition - vertexPosition);
				const bool isWithinEpsilon = (delta <= Math::Vector3f{Math::NumericLimits<float>::Epsilon}).AreAllSet();
				result &= ~(Flags::HalfPointPrecisionPositions * !isWithinEpsilon);
			}

			return result.GetFlags();
		}(GetVertexElementView<VertexPosition>());

		// Check if we can compress texture coordinates without losing precision
		flags |= [](const ArrayView<const VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinates)
		{
			EnumFlags<Flags> result = Flags::HalfPointPrecisionTextureCoordinates;

			for (const VertexTextureCoordinate& __restrict textureCoordinates : vertexTextureCoordinates)
			{
				// Check if the compressed epsilon converted back retains enough precision
				const Math::Vector2h compressedCordinate = (Math::Vector2h)(textureCoordinates);

				const Math::Vector2d decompressedCoordinate = (Math::Vector2d)(Math::Vector2f)compressedCordinate;
				const Math::Vector2f renormalizedCoordinate = Math::Vector2f(decompressedCoordinate);

				const bool isWithinEpsilon = renormalizedCoordinate.IsEquivalentTo(textureCoordinates, Math::NumericLimits<float>::Epsilon);
				result &= ~((Flags::HalfPointPrecisionTextureCoordinates) * !isWithinEpsilon);
			}

			return result.GetFlags();
		}(GetVertexElementView<VertexTextureCoordinate>());

		flags |= m_flags;

		outputFile.Write(flags);

		outputFile.Write(m_vertexCount);
		outputFile.Write(m_indexCount);

		// Write vertex positions
		switch (GetFloatPrecision(flags, Flags::HalfPointPrecisionPositions))
		{
			case FloatPrecision::Single:
			{
				FixedSizeVector<Math::UnalignedVector3<float>, Rendering::Index>
					positions(Memory::ConstructWithSize, Memory::Uninitialized, m_vertexCount);
				ArrayView<Math::UnalignedVector3<float>, Rendering::Index> positionsView = positions.GetView();

				for (const VertexPosition& __restrict vertexPosition : GetVertexElementView<VertexPosition>())
				{
					positionsView[0] = (Math::UnalignedVector3<float>)vertexPosition;
					positionsView++;
				}

				outputFile.Write(positions.GetView());
			}
			break;
			case FloatPrecision::Half:
			{
				FixedSizeVector<Math::UnalignedVector3<half>, Rendering::Index>
					positions(Memory::ConstructWithSize, Memory::Uninitialized, m_vertexCount);
				ArrayView<Math::UnalignedVector3<half>, Rendering::Index> positionsView = positions.GetView();

				for (const VertexPosition& __restrict vertexPosition : GetVertexElementView<VertexPosition>())
				{
					const Math::Vector3h compressedPosition = (Math::Vector3h)vertexPosition;

					positionsView[0] = (Math::UnalignedVector3<half>)compressedPosition;
					positionsView++;
				}

				outputFile.Write(positions.GetView());
			}
			break;
		}

		// Write vertex normals
		{
			FixedSizeVector<VertexNormals, Rendering::Index> normals(Memory::ConstructWithSize, Memory::Uninitialized, m_vertexCount);
			ArrayView<VertexNormals, Rendering::Index> normalsView = normals.GetView();

			for (const VertexNormals& __restrict vertexNormals : GetVertexElementView<VertexNormals>())
			{
				normalsView[0] = vertexNormals;
				normalsView++;
			}

			outputFile.Write(normals.GetView());
		}

		// Write texture coordiantes
		switch (GetFloatPrecision(flags, Flags::HalfPointPrecisionTextureCoordinates))
		{
			case FloatPrecision::Single:
			{
				outputFile.Write(GetVertexElementView<VertexTextureCoordinate>());
			}
			break;
			case FloatPrecision::Half:
			{
				FixedSizeVector<Math::UnalignedVector2<half>, Rendering::Index>
					textureCoordinates(Memory::ConstructWithSize, Memory::Uninitialized, m_vertexCount);
				ArrayView<Math::UnalignedVector2<half>, Rendering::Index> textureCoordinatesView = textureCoordinates.GetView();

				for (const VertexTextureCoordinate& __restrict vertexTextureCoordinates : GetVertexElementView<VertexTextureCoordinate>())
				{
					textureCoordinatesView[0] = (Math::Vector2h)(vertexTextureCoordinates);
					textureCoordinatesView++;
				}

				outputFile.Write(textureCoordinates.GetView());
			}
			break;
		}

		// Write vertex colors
		const uint8 usedVertexColorSlotCount = GetUsedVertexColorSlotCount();
		if (usedVertexColorSlotCount > 0)
		{
			const uint8 usedVertexAlphaSlotCount = GetUsedVertexAlphaColorSlotCount();
			const ArrayView<const VertexColors, Index> vertexColors = GetVertexElementView<VertexColors>();

			FixedSizeVector<uint8> writtenVertexColors(
				Memory::ConstructWithSize,
				Memory::Uninitialized,
				m_vertexCount * (usedVertexColorSlotCount * 3 + usedVertexAlphaSlotCount) + 1
			);
			ArrayView<uint8> colors = writtenVertexColors;

			if (usedVertexColorSlotCount == usedVertexAlphaSlotCount)
			{
				for (uint8 usedVertexColorIndex = 0; usedVertexColorIndex < usedVertexColorSlotCount; ++usedVertexColorIndex)
				{
					for (const VertexColors& __restrict vertexColorArray : vertexColors)
					{
						const Math::ColorByte color = vertexColorArray[usedVertexColorIndex];
						colors[0] = color.r;
						colors[1] = color.g;
						colors[2] = color.b;
						colors[3] = color.a;
						colors += 4;
					}
				}
			}
			else
			{
				uint8 usedVertexColorIndex = 0;
				for (uint8 vertexColorIndex = 0; vertexColorIndex < VertexColors::Size; ++vertexColorIndex)
				{
					for (const VertexColors& __restrict vertexColorArray : vertexColors)
					{
						const Math::ColorByte color = vertexColorArray[usedVertexColorIndex];
						colors[0] = color.r;
						colors[1] = color.g;
						colors[2] = color.b;

						const bool hasAlpha = flags.IsSet(Flags::HasVertexColorSlotAlphaFirst << vertexColorIndex);
						colors[3] = color.a;

						colors += 3 + hasAlpha;
					}
				}
			}

			outputFile.Write(writtenVertexColors.GetView().GetSubView(0, writtenVertexColors.GetSize() - 1));
		}

		switch (GetIndexType(flags))
		{
			case IndexType::UInt32:
			{
				if constexpr (TypeTraits::IsSame<uint32, Rendering::Index>)
				{
					outputFile.Write(GetIndices());
				}
				else
				{
					ExpectUnreachable();
				}
			}
			break;
			case IndexType::UInt16:
			{
				if constexpr (TypeTraits::IsSame<uint16, Rendering::Index>)
				{
					outputFile.Write(GetIndices());
				}
				else
				{
					FixedSizeVector<uint16, Rendering::Index> indices(Memory::ConstructWithSize, Memory::Uninitialized, GetIndexCount());
					ArrayView<uint16, Rendering::Index> indexView = indices.GetView();

					for (const Rendering::Index index : GetIndices())
					{
						indexView[0] = (uint16)index;
						indexView++;
					}

					outputFile.Write(indices.GetView());
				}
			}
			break;
			case IndexType::UInt8:
			{
				static_assert(!TypeTraits::IsSame<uint8, Rendering::Index>);
				FixedSizeVector<uint8, Rendering::Index> indices(Memory::ConstructWithSize, Memory::Uninitialized, GetIndexCount());
				ArrayView<uint8, Rendering::Index> indexView = indices.GetView();

				for (const Rendering::Index index : GetIndices())
				{
					indexView[0] = (uint8)index;
					indexView++;
				}

				outputFile.Write(indices.GetView());
			}
			break;
		}

		WrittenBounds bounds = m_boundingBox;
		outputFile.Write(ConstByteView::Make(bounds));
	}

	Math::BoundingBox StaticObject::CalculateBoundingBox() const
	{
		const ArrayView<const VertexPosition, Index> vertices = GetVertexElementView<VertexPosition>();
		if (vertices.HasElements())
		{
			Math::BoundingBox boundingBox(vertices[0]);

#if USE_AVX
			using VectorizedType = Math::Vectorization::Packed<float, 8>;
			constexpr uint8 stepCount = sizeof(VectorizedType) / sizeof(float);

			const Index numVertices = vertices.GetSize() - 1u;
			const Index remainder = numVertices % stepCount;
			const Index maximumBatchCount = numVertices - remainder;

			VectorizedType minimumValues[3];
			VectorizedType maximumValues[3];

			for (uint8 i = 0; i < 3; ++i)
			{
				minimumValues[i] = VectorizedType{boundingBox.GetMinimum()[i]};
				maximumValues[i] = VectorizedType{boundingBox.GetMaximum()[i]};
			}

			for (decltype(vertices)::ConstPointerType it = vertices.begin() + 1, end = vertices.begin() + maximumBatchCount + 1; it != end;
			     it += stepCount)
			{
				for (uint8 i = 0; i < 3; ++i)
				{
					VectorizedType
						value((*it)[i], (*(it + 1))[i], (*(it + 2))[i], (*(it + 3))[i], (*(it + 4))[i], (*(it + 5))[i], (*(it + 6))[i], (*(it + 7))[i]);
					minimumValues[i] = Math::Min(minimumValues[i], value);
					maximumValues[i] = Math::Max(maximumValues[i], value);
				}
			}

			Math::Vector3f min = Math::Zero, max = Math::Zero;

			for (uint8 i = 0; i < 3; ++i)
			{
				for (uint8 i2 = 0; i2 < (stepCount - 1); i2++)
				{
					constexpr int mask = 0b10010011;
					minimumValues[i] = Math::Min(minimumValues[i], (VectorizedType)_mm256_shuffle_ps(minimumValues[i], minimumValues[i], mask));
					maximumValues[i] = Math::Max(maximumValues[i], (VectorizedType)_mm256_shuffle_ps(maximumValues[i], maximumValues[i], mask));
				}

				minimumValues[i].StoreSingle(min[i]);
				maximumValues[i].StoreSingle(max[i]);
			}

			boundingBox.Expand(min);
			boundingBox.Expand(max);

			for (decltype(vertices)::ConstPointerType it = vertices.end() - 1 - remainder, end = vertices.end(); it != end; it++)
			{
				boundingBox.Expand(*it);
			}
#else
			for (decltype(vertices)::ConstPointerType it = vertices.begin() + 1, end = vertices.end(); it < end; it++)
			{
				boundingBox.Expand(*it);
			}
#endif

			return boundingBox;
		}
		else
		{
			return Math::BoundingBox{Math::Zero};
		}
	}

	template<typename Type>
	[[nodiscard]] /*static*/ uint32
	StaticObject::CalculateDataOffset(const Index vertexCount, const Index indexCount, const uint8 vertexColorCount)
	{
		return (uint32)MultiView::CalculateDataOffset<MultiView::FirstTypeIndex<Type>>(
			{vertexCount, vertexCount, vertexCount, vertexCount * vertexColorCount, indexCount}
		);
	}

	[[nodiscard]] /*static*/ uint32
	StaticObject::CalculateDataSize(const Index vertexCount, const Index indexCount, const uint8 vertexColorCount)
	{
		return (uint32)MultiView::CalculateDataSize({vertexCount, vertexCount, vertexCount, vertexCount * vertexColorCount, indexCount});
	}

	template<typename ElementType>
	[[nodiscard]] PURE_LOCALS_AND_POINTERS ArrayView<ElementType, Index> StaticObject::GetVertexElementView() LIFETIME_BOUND
	{
		const uint32 offset = CalculateDataOffset<ElementType>(m_vertexCount, m_indexCount, GetUsedVertexColorSlotCount());
		Assert(
			m_data.GetView().IsWithinBounds(
				AllocatorType::ConstView{m_data.GetData() + offset, static_cast<AllocatorType::SizeType>(sizeof(ElementType) * m_vertexCount)}
			) ||
			m_vertexCount == 0
		);
		return {reinterpret_cast<ElementType*>(m_data.GetData() + offset), m_vertexCount};
	}
	template<typename ElementType>
	[[nodiscard]] PURE_LOCALS_AND_POINTERS ArrayView<const ElementType, Index> StaticObject::GetVertexElementView() const LIFETIME_BOUND
	{
		const uint32 offset = CalculateDataOffset<ElementType>(m_vertexCount, m_indexCount, GetUsedVertexColorSlotCount());
		Assert(
			m_data.GetView().IsWithinBounds(
				AllocatorType::ConstView{m_data.GetData() + offset, static_cast<AllocatorType::SizeType>(sizeof(ElementType) * m_vertexCount)}
			) ||
			m_vertexCount == 0
		);
		return {reinterpret_cast<const ElementType*>(m_data.GetData() + offset), m_vertexCount};
	}

	template ArrayView<VertexPosition, Index> PURE_LOCALS_AND_POINTERS StaticObject::GetVertexElementView<VertexPosition>();
	template ArrayView<const VertexPosition, Index> PURE_LOCALS_AND_POINTERS StaticObject::GetVertexElementView<VertexPosition>() const;
	template ArrayView<VertexNormals, Index> PURE_LOCALS_AND_POINTERS StaticObject::GetVertexElementView<VertexNormals>();
	template ArrayView<const VertexNormals, Index> PURE_LOCALS_AND_POINTERS StaticObject::GetVertexElementView<VertexNormals>() const;
	template ArrayView<VertexTextureCoordinate, Index> PURE_LOCALS_AND_POINTERS StaticObject::GetVertexElementView<VertexTextureCoordinate>();
	template ArrayView<const VertexTextureCoordinate, Index>
		PURE_LOCALS_AND_POINTERS StaticObject::GetVertexElementView<VertexTextureCoordinate>() const;
	template ArrayView<VertexColors, Index> PURE_LOCALS_AND_POINTERS StaticObject::GetVertexElementView<VertexColors>();
	template ArrayView<const VertexColors, Index> PURE_LOCALS_AND_POINTERS StaticObject::GetVertexElementView<VertexColors>() const;

	[[nodiscard]] PURE_LOCALS_AND_POINTERS ArrayView<Index, Index> StaticObject::GetIndices() LIFETIME_BOUND
	{
		const uint32 offset = CalculateDataOffset<Index>(m_vertexCount, m_indexCount, GetUsedVertexColorSlotCount());
		Assert(
			m_data.GetView().IsWithinBounds(
				AllocatorType::ConstView{m_data.GetData() + offset, static_cast<AllocatorType::SizeType>(sizeof(Index) * m_indexCount)}
			) ||
			m_indexCount == 0
		);
		return {reinterpret_cast<Index*>(m_data.GetData() + offset), m_indexCount};
	}
	[[nodiscard]] PURE_LOCALS_AND_POINTERS ArrayView<const Index, Index> StaticObject::GetIndices() const LIFETIME_BOUND
	{
		const uint32 offset = CalculateDataOffset<Index>(m_vertexCount, m_indexCount, GetUsedVertexColorSlotCount());
		Assert(
			m_data.GetView().IsWithinBounds(
				AllocatorType::ConstView{m_data.GetData() + offset, static_cast<AllocatorType::SizeType>(sizeof(Index) * m_indexCount)}
			) ||
			m_vertexCount == 0
		);
		return {reinterpret_cast<const Index*>(m_data.GetData() + offset), m_indexCount};
	}

	void StaticObject::TransformRotation(const Math::Quaternionf quaternion)
	{
		const ArrayView<VertexPosition, Rendering::Index> positions = GetVertexElementView<VertexPosition>();
		for (VertexPosition& __restrict vertexPosition : positions)
		{
			vertexPosition = quaternion.TransformDirection(vertexPosition);
		}

		const ArrayView<VertexNormals, Rendering::Index> normals = GetVertexElementView<VertexNormals>();
		for (VertexNormals& __restrict vertexNormals : normals)
		{
			vertexNormals.normal = quaternion.TransformDirection((Math::Vector3f)vertexNormals.normal);
			vertexNormals.tangent = quaternion.TransformDirection((Math::Vector3f)vertexNormals.tangent);
		}
	}
}
