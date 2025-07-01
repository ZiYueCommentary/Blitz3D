#ifndef GXFONT_H
#define GXFONT_H

#include "std.h"
#include <ft2build.h>
#include <pango/pangocairo.h>
#include FT_FREETYPE_H

class gxCanvas;
class gxGraphics;

typedef IDirectDrawSurface7 ddSurf;

class gxFont {
public:
	gxFont(FT_Library ftLibrary, gxGraphics* gfx, const std::string& fn, int h, bool bold = false, bool italic = false, bool underlined = false);
	~gxFont();

	void render(gxCanvas* dest, unsigned color_argb, int x, int y, const std::string& t);

	std::pair<int, int> stringWidthHeight(const std::string& text) const;

	//ACCESSORS
	int getHeight(const std::string& text) const;							//height of font
	int getWidth(const std::string& text) const;	    //width of string
	bool isPrintable(int chr)const;				//printable char?

private:
	int height;
	PangoFontDescription* desc;
	gxGraphics* graphics;
	gxCanvas* tempCanvas;
};

#endif