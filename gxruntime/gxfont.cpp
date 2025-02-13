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

	desc = pango_font_description_from_string(std::format("{0} {1}", face->family_name, h - 5).c_str());
	FT_Done_Face(face);

	tempCanvas = nullptr;
}

gxFont::~gxFont() {
	pango_font_description_free(desc);
}

const int transparentPixel = 0x4A412A;
const int opaquePixel = 0xffffff;

void gxFont::render(gxCanvas* dest, unsigned color_argb, int x, int y, const std::string& text) {
	auto [width, height] = stringWidthHeight(text); // modern!
	if(tempCanvas == nullptr || width > tempCanvas->getWidth() || height > tempCanvas->getHeight()) {
		graphics->freeCanvas(tempCanvas);
		tempCanvas = graphics->createCanvas(width, height, 0);
	}

	if((color_argb & 0xffffff) == transparentPixel) { color_argb++; }

	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_A1, width, height);
	cairo_t* cr = cairo_create(surface);
	PangoLayout* layout = pango_cairo_create_layout(cr);
	pango_layout_set_text(layout, text.c_str(), text.length());
	pango_layout_set_font_description(layout, this->desc);
	//cairo_set_antialias(cr, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_set_source_rgb(cr, RGBA_GETRED(color_argb), RGBA_GETGREEN(color_argb), RGBA_GETBLUE(color_argb));
	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);
	
	int baseline = PANGO_PIXELS(pango_layout_get_baseline(layout));

	tempCanvas->lock();
	unsigned char* data = cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int byte_index = y * stride + x / 8;
			int bit_index = x % 8;
			unsigned char pixel = (data[byte_index] >> bit_index) & 1;

			tempCanvas->setPixelFast(x, y, pixel ? color_argb : transparentPixel);
		}
	}
	
	tempCanvas->unlock();
	tempCanvas->setMask(transparentPixel);

	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	dest->blit(x, y/*todo baseline*/, tempCanvas, 0, 0, width, height, false);
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