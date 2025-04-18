// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Core/Reference.h>
#include <Core/Color.h>
#include <Math/Float2.h>
#include <TestFramework/Renderer/RenderPrimitive.h>
#include <TestFramework/Renderer/Texture.h>
#include <TestFramework/Renderer/PipelineState.h>

#include <Common/IO/Path.h>

JPH_NAMESPACE_BEGIN
class Renderer;
class Texture;

/// Font class, used to display text in 3D mode. Does variable width fonts with kerning. Font names are identical to the Windows font names.
class Font : public RefTarget<Font>
{
public:
	/// Constants
	static const int cBeginChar = ' ';                  ///< First character that is drawable in the character set
	static const int cEndChar = 256;                    ///< Last character + 1 that is drawable in the character set
	static const int cNumChars = cEndChar - cBeginChar; ///< Number of drawable characters in the character set

	/// Constructor
	Font(DebugRendering::Renderer* inRenderer, const ngine::IO::ConstZeroTerminatedPathView assetsDirectory);

	/// Create a font
	bool Create(const char* inFontName, int inCharHeight);

	/// Properties
	const String& GetFontName() const
	{
		return mFontName;
	}
	int GetCharHeight() const
	{
		return mCharHeight;
	}

	/// Get extents of a string, assuming the height of the text is 1 and with the normal aspect ratio of the font
	Float2 MeasureText(const string_view& inText) const;

	/// Draw a string at a specific location
	/// If the string is drawn with the identity matrix, it's top left will start at (0, 0, 0)
	/// The text width is in the X direction and the text height is in the Y direction and it will have a height of 1
	void DrawText3D(Mat44Arg inTransform, const string_view& inText, ColorArg inColor = Color::sWhite) const;
private:
	/// Create a primitive for a string
	bool CreateString(Mat44Arg inTransform, const string_view& inText, ColorArg inColor, DebugRendering::RenderPrimitive& ioPrimitive) const;

	struct FontVertex
	{
		Float3 mPosition;
		Float2 mTexCoord;
		Color mColor;
	};

	/// Properties of the font
	String mFontName;                     ///< Name of the font
	int mCharHeight;                      ///< Height of a character
	int mHorizontalTexels;                ///< Number of texels horizontally, determines the scale of mStartU, mWidth and mSpacing
	int mVerticalTexels;                  ///< Number of texels vertically, determines the scale of mStartV
	uint16 mStartU[cNumChars];            ///< Start U in texels
	uint16 mStartV[cNumChars];            ///< Start V in texels
	uint8 mWidth[cNumChars];              ///< Width of character in texels
	uint8 mSpacing[cNumChars][cNumChars]; ///< Spacing between characters in texels

	/// Structures used for drawing
	DebugRendering::Renderer* mRenderer;                      ///< Our renderer
	Ref<DebugRendering::Texture> mTexture;                    ///< The texture containing all characters
	unique_ptr<DebugRendering::PipelineState> mPipelineState; ///< The state used to render characters
	ngine::IO::Path mAssetsDirectory;
};
JPH_NAMESPACE_END
