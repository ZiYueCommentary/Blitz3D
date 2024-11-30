#include "std.h"
#include "gxfont.h"

#include "gxcanvas.h"
#include "gxgraphics.h"
#include "gxutf8.h"
#include "../bbruntime/bbsys.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <freetype/ftsynth.h>

gxFont::gxFont(FT_Library ftLibrary, gxGraphics* gfx, const std::string& fn, int h, bool bold, bool italic, bool underlined) {
	graphics = gfx;
	filename = fn;
	height = h;
	this->bold = bold;
	this->italic = italic;
	this->underlined = underlined;

	if (FT_New_Face(ftLibrary,
		filename.c_str(),
		0,
		&freeTypeFace)) {
		RTEX(std::format("Failed to load file: {}", fn).c_str());
	}

	FT_Set_Pixel_Sizes(freeTypeFace,
		0,
		height);

	glyphData.clear();
	atlases.clear();

	glyphHeight = height;
	renderAtlas('T');
	std::map<int, GlyphData>::iterator it = glyphData.find('T');
	if(it != glyphData.end()) {
		const GlyphData& gd = it->second;

		glyphHeight = gd.srcRect[3];
		glyphRenderOffset = -gd.drawOffset[1];
	}

	tCanvasHeight = (glyphHeight * 40) / 10;
	glyphRenderBaseline = (glyphHeight * 3 / 10);
	glyphRenderOffset += glyphRenderBaseline;

	tempCanvas = nullptr;
}

gxFont::~gxFont() {
	for(int i = 0; i < atlases.size(); i++) {
		graphics->freeCanvas(atlases[i]);
	}

	FT_Done_Face(freeTypeFace);
}

const int transparentPixel = 0x4A412A;
const int opaquePixel = 0xffffff;

void gxFont::renderAtlas(int chr) {
	bool needsNewAtlas = false;

	int startChr = chr - 1024;
	if(startChr < 0) { startChr = 0; }
	int endChr = startChr + 2048;

	bool* buffer = nullptr;
	int x = -1; int y = -1;
	int maxHeight = -1;
	for(int i = startChr; i < endChr; i++) {
		std::map<int, GlyphData>::iterator it = glyphData.find(i);
		if(it == glyphData.end()) {
			long glyphIndex = FT_Get_Char_Index(freeTypeFace, i);
			FT_Load_Glyph(freeTypeFace,
				(FT_UInt)glyphIndex,
				FT_LOAD_TARGET_MONO);
			if(glyphIndex != 0) {
				if (bold)
				{
					FT_GlyphSlot_Embolden(freeTypeFace->glyph);
				}

				if (italic)
				{
					FT_GlyphSlot_Oblique(freeTypeFace->glyph);
				}

				FT_Render_Glyph(freeTypeFace->glyph,
					FT_RENDER_MODE_MONO);
				unsigned char* glyphBuffer = freeTypeFace->glyph->bitmap.buffer;
				int glyphPitch = freeTypeFace->glyph->bitmap.pitch;
				int glyphWidth = freeTypeFace->glyph->bitmap.width;
				int glyphHeight = freeTypeFace->glyph->bitmap.rows;

				if(glyphWidth > 0 && glyphHeight > 0) {
					if(buffer == nullptr) {
						buffer = new bool[atlasDims * atlasDims];
						for(int j = 0; j < atlasDims * atlasDims; j++) {
							buffer[j] = false;
						}
						x = 1; y = 1; maxHeight = 0;
					}

					if(x + glyphWidth + 1 > atlasDims - 1) {
						x = 1; y += maxHeight + 1;
						maxHeight = 0;
					}
					if(y + glyphHeight + 1 > atlasDims - 1) {
						needsNewAtlas = true;
						break;
					}
					if(glyphHeight > maxHeight) maxHeight = glyphHeight;

					int bitPitch = glyphPitch * 8;
					for(int j = 0; j < glyphPitch * glyphHeight; j++) {
						for(int k = 0; k < 8; k++) {
							if((j * 8 + k) % bitPitch >= glyphWidth) continue;
							int bufferPos = x + y * atlasDims;
							bufferPos += (j * 8 + k) % bitPitch + ((j / glyphPitch) * atlasDims);
							buffer[bufferPos] = (glyphBuffer[j] & (1 << (7 - k))) > 0;
						}
					}

					GlyphData gd;
					gd.atlasIndex = (int)atlases.size();
					gd.horizontalAdvance = freeTypeFace->glyph->metrics.horiAdvance >> 6;
					gd.drawOffset[0] = -freeTypeFace->glyph->bitmap_left;
					gd.drawOffset[1] = freeTypeFace->glyph->bitmap_top - ((height * 10) / 14);
					gd.srcRect[0] = x;
					gd.srcRect[1] = y;
					gd.srcRect[2] = glyphWidth;
					gd.srcRect[3] = glyphHeight;

					if(glyphWidth > maxWidth) { maxWidth = glyphWidth; }

					x += glyphWidth + 1;

					glyphData.emplace(i, gd);
				}
				else {
					GlyphData gd;
					gd.atlasIndex = -1;
					gd.horizontalAdvance = freeTypeFace->glyph->metrics.horiAdvance >> 6;
					glyphData.emplace(i, gd);
				}
			}
			else {
				GlyphData gd;
				gd.atlasIndex = -1;
				gd.horizontalAdvance = freeTypeFace->glyph->metrics.horiAdvance >> 6;
				glyphData.emplace(i, gd);
			}
		}
	}

	if(buffer != nullptr) {
		gxCanvas* newAtlas = graphics->createCanvas(atlasDims, atlasDims, 0);
		newAtlas->lock();
		for(int y = 0; y < atlasDims; y++) {
			for(int x = 0; x < atlasDims; x++)
				newAtlas->setPixelFast(x, y, buffer[x + (y * atlasDims)] ? opaquePixel : transparentPixel);
		}
		newAtlas->unlock();
		newAtlas->setMask(0xffffff);
		newAtlas->backup();
		atlases.push_back(newAtlas);
		delete[] buffer;
	}

	if(needsNewAtlas) renderAtlas(chr);
}

void gxFont::render(gxCanvas* dest, unsigned color_argb, int x, int y, const std::string& text) {
	int width = stringWidth(text);
	if(tempCanvas == nullptr || width > tempCanvas->getWidth()) {
		graphics->freeCanvas(tempCanvas);
		tempCanvas = graphics->createCanvas(width, tCanvasHeight, 0);
		tempCanvas->setMask(transparentPixel);
	}

	if((color_argb & 0xffffff) == transparentPixel) { color_argb++; }
	tempCanvas->setColor(transparentPixel);
	tempCanvas->rect(0, 0, width, tCanvasHeight, true);
	tempCanvas->setColor(color_argb);

	int t_x = 0;

	for(int i = 0; i < text.size();) {
		int codepointLen = UTF8::measureCodepoint(text[i]);
		int chr = UTF8::decodeCharacter(text.c_str(), i);
		std::map<int, GlyphData>::iterator it = glyphData.find(chr);
		if(it == glyphData.end()) {
			renderAtlas(chr);
			it = glyphData.find(chr);
		}

		if(it != glyphData.end()) {
			const GlyphData& gd = it->second;

			if(gd.atlasIndex >= 0) {
				tempCanvas->rect(t_x - gd.drawOffset[0], glyphRenderBaseline - gd.drawOffset[1], gd.srcRect[2], gd.srcRect[3], true);
				tempCanvas->blit(t_x - gd.drawOffset[0], glyphRenderBaseline - gd.drawOffset[1], atlases[gd.atlasIndex], gd.srcRect[0], gd.srcRect[1], gd.srcRect[2], gd.srcRect[3], false);
			}
			t_x += gd.horizontalAdvance;
		}
		i += codepointLen;
	}

	if (underlined)
	{
		tempCanvas->rect(0, static_cast<int>(getBaselinePosition() + getUnderlinePosition()), width, max(1, static_cast<int>(getUnderlineThickness())), true);
	}

	dest->blit(x, y - glyphRenderOffset, tempCanvas, 0, 0, width, tCanvasHeight, false);
}

int gxFont::charWidth(int chr) {
	std::map<int, GlyphData>::iterator it = glyphData.find(chr);
	if(it == glyphData.end()) {
		renderAtlas(chr);
		it = glyphData.find(chr);
	}
	return it->second.srcRect[2];
}

int gxFont::charAdvance(int chr) {
	std::map<int, GlyphData>::iterator it = glyphData.find(chr);
	if(it == glyphData.end()) {
		renderAtlas(chr);
		it = glyphData.find(chr);
	}
	return it != glyphData.end() ? it->second.horizontalAdvance : 0;
}

int gxFont::stringWidth(const std::string& text) {
	int width = 0;

	for(int i = 0; i < text.size();) {
		int codepointLen = UTF8::measureCodepoint(text[i]);
		int chr = UTF8::decodeCharacter(text.c_str(), i);
		std::map<int, GlyphData>::iterator it = glyphData.find(chr);
		if(it == glyphData.end()) {
			renderAtlas(chr);
			it = glyphData.find(chr);
		}

		if(it != glyphData.end()) {
			width += it->second.horizontalAdvance;
		}
		i += codepointLen;
	}

	return width;
}

int gxFont::getWidth()const {
	return maxWidth;
}

int gxFont::getHeight()const {
	return glyphHeight;
}

int gxFont::getRenderOffset()const {
	return glyphRenderOffset;
}

int gxFont::getWidth(const std::string& text) {
	return stringWidth(text);
}

bool gxFont::isPrintable(int chr)const {
	return glyphData.find(chr) != glyphData.end();
}

float gxFont::getBaselinePosition() const
{
	return static_cast<float>(freeTypeFace->size->metrics.ascender) / float{1 << 6};
}

float gxFont::getUnderlinePosition()const
{
	return -static_cast<float>(FT_MulFix(freeTypeFace->underline_position, freeTypeFace->size->metrics.y_scale)) / float{1 << 6};
}

float gxFont::getUnderlineThickness()const
{
	return max(1.0f, static_cast<float>(FT_MulFix(freeTypeFace->underline_thickness, freeTypeFace->size->metrics.y_scale)) / float{1 << 6});
}