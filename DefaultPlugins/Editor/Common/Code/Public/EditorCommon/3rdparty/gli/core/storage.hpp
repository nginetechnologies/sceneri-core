#pragma once

// STD
#include <vector>
#include <queue>
#include <string>
#include <cassert>
#include <cmath>
#include <cstring>
#include <memory>

#include "../type.hpp"
#include "../format.hpp"

// GLM
#include <EditorCommon/3rdparty/glm/gtc/round.hpp>
#include <EditorCommon/3rdparty/glm/gtx/component_wise.hpp>
#include <EditorCommon/3rdparty/glm/gtx/integer.hpp>
#include <EditorCommon/3rdparty/glm/gtx/bit.hpp>
#include <EditorCommon/3rdparty/glm/gtx/raw_data.hpp>
#include <EditorCommon/3rdparty/glm/gtx/wrap.hpp>

static_assert(GLM_VERSION >= 97, "GLI requires at least GLM 0.9.7");

namespace gli
{
	class storage_linear
	{
	public:
		typedef extent3d extent_type;
		typedef size_t size_type;
		typedef gli::format format_type;
		typedef glm::byte data_type;

	public:
		storage_linear();

		storage_linear(
			format_type Format,
			extent_type const & Extent,
			size_type Layers,
			size_type Faces,
			size_type Levels);

		bool empty() const;
		size_type size() const; // Express is bytes
		size_type layers() const;
		size_type levels() const;
		size_type faces() const;

		size_type block_size() const;
		extent_type block_extent() const;
		extent_type block_count(size_type Level) const;
		extent_type extent(size_type Level) const;

		data_type* data();
		data_type const* const data() const;

		/// Compute the relative memory offset to access the data for a specific layer, face and level
		size_type base_offset(
			size_type Layer,
			size_type Face,
			size_type Level) const;

		/// Copy a subset of a specific image of a texture 
		void copy(
			storage_linear const& StorageSrc,
			size_t LayerSrc, size_t FaceSrc, size_t LevelSrc, extent_type const& BlockIndexSrc,
			size_t LayerDst, size_t FaceDst, size_t LevelDst, extent_type const& BlockIndexDst,
			extent_type const& BlockCount);

		size_type level_size(
			size_type Level) const;
		size_type face_size(
			size_type BaseLevel, size_type MaxLevel) const;
		size_type layer_size(
			size_type BaseFace, size_type MaxFace,
			size_type BaseLevel, size_type MaxLevel) const;

	private:
		size_type const Layers;
		size_type const Faces;
		size_type const Levels;
		size_type const BlockSize;
		extent_type const BlockCount;
		extent_type const BlockExtent;
		extent_type const Extent;
		std::vector<data_type> Data;
	};
}//namespace gli

#include "storage_linear.inl"
