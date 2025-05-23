#include "clear.hpp"
#include <EditorCommon/3rdparty/glm/vector_relational.hpp>

namespace gli
{
	template <typename T, qualifier P>
	inline sampler_cube_array<T, P>::sampler_cube_array(texture_type const & Texture, gli::wrap Wrap, filter Mip, filter Min, texel_type const & BorderColor)
		: sampler(Wrap, Texture.levels() > 1 ? Mip : FILTER_NEAREST, Min)
		, Texture(Texture)
		, Convert(detail::convert<texture_type, T, P>::call(this->Texture.format()))
		, BorderColor(BorderColor)
		, Filter(detail::get_filter<filter_type, detail::DIMENSION_2D, texture_type, interpolate_type, normalized_type, fetch_type, texel_type, T>(Mip, Min, is_border(Wrap)))
	{
		GLI_ASSERT(!Texture.empty());
		GLI_ASSERT(!is_compressed(Texture.format()));
		GLI_ASSERT((!std::numeric_limits<T>::is_iec559 && Mip == FILTER_NEAREST && Min == FILTER_NEAREST) || std::numeric_limits<T>::is_iec559);
	}

	template <typename T, qualifier P>
	inline typename sampler_cube_array<T, P>::texture_type const & sampler_cube_array<T, P>::operator()() const
	{
		return this->Texture;
	}

	template <typename T, qualifier P>
	inline typename sampler_cube_array<T, P>::texel_type sampler_cube_array<T, P>::texel_fetch(extent_type const & TexelCoord, size_type layer, size_type Face, size_type Level) const
	{
		GLI_ASSERT(!this->Texture.empty());
		GLI_ASSERT(this->Convert.Fetch);

		return this->Convert.Fetch(this->Texture, TexelCoord, layer, Face, Level);
	}

	template <typename T, qualifier P>
	inline void sampler_cube_array<T, P>::texel_write(extent_type const & TexelCoord, size_type layer, size_type Face, size_type Level, texel_type const & Texel)
	{
		GLI_ASSERT(!this->Texture.empty());
		GLI_ASSERT(this->Convert.Write);

		this->Convert.Write(this->Texture, TexelCoord, layer, Face, Level, Texel);
	}

	template <typename T, qualifier P>
	inline void sampler_cube_array<T, P>::clear(texel_type const & Color)
	{
		GLI_ASSERT(!this->Texture.empty());
		GLI_ASSERT(this->Convert.Write);

		detail::clear<texture_type, T, P>::call(this->Texture, this->Convert.Write, Color);
	}

	template <typename T, qualifier P>
	inline typename sampler_cube_array<T, P>::texel_type sampler_cube_array<T, P>::texture_lod(normalized_type const & SampleCoord, size_type Layer, size_type Face, level_type Level) const
	{
		GLI_ASSERT(!this->Texture.empty());
		GLI_ASSERT(std::numeric_limits<T>::is_iec559);
		GLI_ASSERT(this->Filter && this->Convert.Fetch);

		normalized_type const SampleCoordWrap(this->Wrap(SampleCoord.x), this->Wrap(SampleCoord.y));
		return this->Filter(this->Texture, this->Convert.Fetch, SampleCoordWrap, Layer, Face, Level, this->BorderColor);
	}

	template <typename T, qualifier P>
	inline void sampler_cube_array<T, P>::generate_mipmaps(filter Minification)
	{
		this->generate_mipmaps(this->Texture.base_layer(), this->Texture.max_layer(), this->Texture.base_face(), this->Texture.max_face(), this->Texture.base_level(), this->Texture.max_level(), Minification);
	}

	template <typename T, qualifier P>
	inline void sampler_cube_array<T, P>::generate_mipmaps(size_type BaseLayer, size_type MaxLayer, size_type BaseFace, size_type MaxFace, size_type BaseLevel, size_type MaxLevel, filter Minification)
	{
		GLI_ASSERT(!this->Texture.empty());
		GLI_ASSERT(!is_compressed(this->Texture.format()));
		GLI_ASSERT(this->Texture.base_layer() <= BaseLayer && BaseLayer <= MaxLayer && MaxLayer <= this->Texture.max_layer());
		GLI_ASSERT(this->Texture.base_face() <= BaseFace && BaseFace <= MaxFace && MaxFace <= this->Texture.max_face());
		GLI_ASSERT(this->Texture.base_level() <= BaseLevel && BaseLevel <= MaxLevel && MaxLevel <= this->Texture.max_level());
		GLI_ASSERT(this->Convert.Fetch && this->Convert.Write);
		GLI_ASSERT(Minification >= FILTER_FIRST && Minification <= FILTER_LAST);

		detail::generate_mipmaps_2d<texture_type, T, fetch_type, write_type, normalized_type, texel_type>(
			this->Texture, this->Convert.Fetch, this->Convert.Write, BaseLayer, MaxLayer, BaseFace, MaxFace, BaseLevel, MaxLevel, Minification);
	}
}//namespace gli

