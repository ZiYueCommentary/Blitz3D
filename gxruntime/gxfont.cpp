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
	height = h;

	FT_Face face;
	if (FT_New_Face(ftLibrary,
		fn.c_str(),
		0,
		&face)) {
		RTEX(std::format("Failed to load file: {}", fn).c_str());
	}

	tempCanvas = nullptr;
	desc = pango_font_description_from_string(std::format("{0} {1}", face->family_name, h - 5).c_str());
	FT_Done_Face(face);
}

gxFont::~gxFont() {
	pango_font_description_free(desc);
	this->graphics->freeCanvas(tempCanvas);
}

const int transparentPixel = 0x4A412A;
const int opaquePixel = 0xffffff;

void gxFont::render(gxCanvas* dest, unsigned color_argb, int x, int y, const std::string& text) {
	auto [width, height] = stringWidthHeight(text); // modern!
	if (width <= 0 || height <= 0) return;

	if (!tempCanvas || tempCanvas->getWidth() < width || tempCanvas->getHeight() < height) {
		this->graphics->freeCanvas(tempCanvas);
		tempCanvas = this->graphics->createCanvas(width, height, 0);
	}

	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_A1, width, height);
	cairo_t* cr = cairo_create(surface);
	PangoLayout* layout = pango_cairo_create_layout(cr);
	pango_layout_set_text(layout, text.c_str(), text.length());
	pango_layout_set_font_description(layout, this->desc);
	pango_cairo_show_layout(cr, layout);
	unsigned char* data = cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface);

	tempCanvas->lock();
	for (int y = 0; y < height; y++) {
		int line = y * stride;
		for (int x = 0; x < width; x++) {
			unsigned char pixel = (data[line + x / 8] >> (x % 8)) & 1;

			tempCanvas->setPixelFast(x, y, pixel ? color_argb : transparentPixel);
		}
	}
	tempCanvas->setMask(transparentPixel);
	tempCanvas->unlock();
	dest->blit(x, y, tempCanvas, 0, 0, width, height, false);

	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

std::pair<int, int> gxFont::stringWidthHeight(const std::string& text) const {
	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_A1, 0, 0);
	cairo_t* cr = cairo_create(surface);
	PangoLayout* layout = pango_cairo_create_layout(cr);
	pango_layout_set_text(layout, text.c_str(), text.length());
	pango_layout_set_font_description(layout, this->desc);
	
	int width, height;
	pango_layout_get_size(layout, &width, &height);
	width = PANGO_PIXELS(width);
	height = PANGO_PIXELS(height);

	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	return std::make_pair(width, height);
}

int gxFont::getHeight(const std::string& text) const {
	auto [width, height] = stringWidthHeight(text);
	return height;
}

int gxFont::getWidth(const std::string& text) const {
	auto [width, height] = stringWidthHeight(text);
	return width;
}

bool gxFont::isPrintable(int chr)const {
	return true;
}