/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#define LOGTAG "ResFace"

#include <imagine/util/strings.h>
#include <imagine/util/bits.h>
#include <imagine/resource/face/ResourceFace.hh>
#include <imagine/logger/logger.h>

#ifdef CONFIG_RESOURCE_FONT_FREETYPE
#include <imagine/resource/font/ResourceFontFreetype.hh>
	#ifdef CONFIG_PACKAGE_FONTCONFIG
	#include <fontconfig/fontconfig.h>
	#endif
#elif defined CONFIG_RESOURCE_FONT_ANDROID
#include <imagine/resource/font/ResourceFontAndroid.hh>
#elif defined CONFIG_RESOURCE_FONT_UIKIT
#include <imagine/resource/font/ResourceFontUIKit.hh>
#endif

// definitions for the Unicode Basic Multilingual Plane (BMP)
static const uint unicodeBmpChars = 0xFFFE;

// location & size of the surrogate/private chars
static const uint unicodeBmpPrivateStart = 0xD800, unicodeBmpPrivateEnd = 0xF8FF;
static const uint unicodeBmpPrivateChars = 0x2100;

static const uint unicodeBmpUsedChars = unicodeBmpChars - unicodeBmpPrivateChars;

static const uint glyphTableEntries = ResourceFace::supportsUnicode ? unicodeBmpUsedChars : numDrawableAsciiChars;

static CallResult mapCharToTable(uint c, uint &tableIdx);

bool ResourceFace::initGlyphTable()
{
	logMsg("allocating glyph table, %d entries", glyphTableEntries);
	mem_free(glyphTable);
	glyphTable = (GlyphEntry*)mem_calloc(1, sizeof(GlyphEntry) * glyphTableEntries);
	if(!glyphTable)
	{
		logErr("out of memory");
		return false;
	}
	usedGlyphTableBits = 0;
	return true;
}

void ResourceFace::freeCaches(uint32 purgeBits)
{
	auto tableBits = usedGlyphTableBits;
	iterateTimes(32, i)
	{
		if((tableBits & 1) && (purgeBits & 1))
		{
			logMsg("purging glyphs from table range %d/31", i);
			int firstChar = i << 11;
			iterateTimesFromStart(2048, firstChar, c)
			{
				uint tableIdx;
				if(mapCharToTable(c, tableIdx) != OK)
				{
					//logMsg( "%c not a known drawable character, skipping", c);
					continue;
				}
				glyphTable[tableIdx].glyph.deinit();
			}
			unsetBits(usedGlyphTableBits, IG::bit(i));
		}
		tableBits >>= 1;
		purgeBits >>= 1;
	}
}

ResourceFace *ResourceFace::load(const char *path, FontSettings *set)
{
	ResourceFont *font = nullptr;
	#ifdef CONFIG_RESOURCE_FONT_FREETYPE
	font = ResourceFontFreetype::load(path);
	#endif
	if(!font)
		return nullptr;

	return create(font, set);
}

ResourceFace *ResourceFace::load(GenericIO io, FontSettings *set)
{
	ResourceFont *font = nullptr;
	#ifdef CONFIG_RESOURCE_FONT_FREETYPE
	font = ResourceFontFreetype::load(std::move(io));
	#endif
	if(!font)
		return nullptr;

	return create(font, set);
}

ResourceFace *ResourceFace::loadSystem(FontSettings *set)
{
	#ifdef CONFIG_RESOURCE_FONT_ANDROID
	auto *font = ResourceFontAndroid::loadSystem();
	if(!font)
		return nullptr;
	return create(font, set);
	#elif defined CONFIG_RESOURCE_FONT_UIKIT
	auto *font = ResourceFontUIKit::loadSystem();
	if(!font)
		return nullptr;
	return create(font, set);
	#elif defined CONFIG_ENV_WEBOS
	return load("/usr/share/fonts/PreludeCondensed-Medium.ttf", set);
	#else
		#ifdef CONFIG_PACKAGE_FONTCONFIG
		logMsg("locating system fonts with fontconfig");
		// Let fontconfig handle loading specific fonts on-demand
		auto font = ResourceFontFreetype::load();
		return create(font, set);
		#else
		return loadAsset("Vera.ttf", set);
		#endif
	#endif
}

ResourceFace *ResourceFace::create(ResourceFace *face, FontSettings *set)
{
	return create(face->font, set);
}

ResourceFace *ResourceFace::create(ResourceFont *font, FontSettings *set)
{
	ResourceFace *inst = new ResourceFace;
	if(!inst)
	{
		logErr("out of memory");
		return nullptr;
	}

	inst->font = font;
	if(!inst->initGlyphTable())
	{
		delete inst;
		return nullptr;
	}

	if(set)
	{
		inst->settings = *set;
		inst->settings.process();
		font->newSize(inst->settings, inst->faceSize);
	}
	else
	{
		inst->settings.pixelHeight = 0;
		inst->settings.pixelWidth = 0;;
	}

	return inst;
}

void ResourceFace::free()
{
	font->freeSize(faceSize);
	iterateTimes(glyphTableEntries, i)
	{
		glyphTable[i].glyph.deinit();
	}
	mem_free(glyphTable);
	delete this;
}

uint ResourceFace::nominalHeight() const
{
	return nominalHeight_;
}

void ResourceFace::calcNominalHeight()
{
	//logMsg("calcNominalHeight");
	GlyphEntry *mGly = glyphEntry('M');
	GlyphEntry *gGly = glyphEntry('g');

	assert(mGly != NULL && gGly != NULL);

	nominalHeight_ = mGly->metrics.ySize + (gGly->metrics.ySize/2);
}

CallResult ResourceFace::applySettings(FontSettings set)
{
	set.process();
	if(set.pixelWidth < font->minUsablePixels())
		set.pixelWidth = font->minUsablePixels();
	if(set.pixelHeight < font->minUsablePixels())
		set.pixelHeight = font->minUsablePixels();

	if(set != settings)
	{
		if(settings.areValid())
		{
			logMsg("flushing glyph cache");
			font->freeSize(faceSize);
			iterateTimes(glyphTableEntries, i)
			{
				glyphTable[i].glyph.deinit();
			}
		}

		settings = set;
		font->newSize(settings, faceSize);
		if(!initGlyphTable())
		{
			bug_exit("couldn't re-allocate glyph table");
		}
		calcNominalHeight();
		return OK;
	}
	else
		return RESOURCE_FACE_SETTINGS_UNCHANGED;
}

//int ResourceFace::maxDescender () { font->applySize(faceSize); return font->currentFaceDescender(); }
//int ResourceFace::maxAscender () { font->applySize(faceSize); return font->currentFaceAscender(); }

CallResult ResourceFace::writeCurrentChar(IG::Pixmap &out)
{
	auto src = font->charBitmap();
	//logDMsg("copying char %dx%d, pitch %d to dest %dx%d, pitch %d", src.x, src.y, src.pitch, out.x, out.y, out.pitch);
	assert(src.w() != 0 && src.h() != 0 && src.pixel({}));
	#if defined __ANDROID__
	if(!src.pitchBytes()) // Hack for JXD S7300B which returns y = x, and pitch = 0
	{
		logWarn("invalid pitch returned for char bitmap");
		src = {{out.size(), out.format()}, src.pixel({}), {out.pitchBytes(), IG::Pixmap::BYTE_UNITS}};
	}
	#endif
	out.write(src, {});
	//memset ( out->data, 0xFF, 16 ); // test by filling with white
	font->unlockCharBitmap();
	return OK;
}

CallResult ResourceFace::cacheChar(int c, int tableIdx)
{
	if(glyphTable[tableIdx].metrics.ySize == -1)
	{
		// failed to previously cache char
		return INVALID_PARAMETER;
	}
	GlyphMetrics metrics;
	// make sure applySize() has been called on the font object first
	if(font->activeChar(c, metrics) != OK)
	{
		// mark failed attempt
		glyphTable[tableIdx].metrics.ySize = -1;
		return INVALID_PARAMETER;
	}
	//logMsg("setting up table entry %d", tableIdx);
	glyphTable[tableIdx].metrics = metrics;
	auto img = GfxGlyphImage(this, &glyphTable[tableIdx]);
	glyphTable[tableIdx].glyph.init(img, false);
	usedGlyphTableBits |= IG::bit((c >> 11) & 0x1F); // use upper 5 BMP plane bits to map in range 0-31
	//logMsg("used table bits 0x%X", usedGlyphTableBits);
	return OK;
}

static CallResult mapCharToTable(uint c, uint &tableIdx)
{
	if(ResourceFace::supportsUnicode)
	{
		//logMsg("mapping char 0x%X", c);
		if(c < unicodeBmpChars && charIsDrawableUnicode(c))
		{
			if(c < unicodeBmpPrivateStart)
			{
				tableIdx = c;
				return OK;
			}
			else if(c > unicodeBmpPrivateEnd)
			{
				tableIdx = c - unicodeBmpPrivateChars; // surrogate & private chars are a hole in the table
				return OK;
			}
			else
			{
				return INVALID_PARAMETER;
			}
		}
		else return INVALID_PARAMETER;
	}
	else
	{
		if(charIsDrawableAscii(c))
		{
			tableIdx = c - firstDrawableAsciiChar;
			return OK;
		}
		else return INVALID_PARAMETER;
	}
}

// TODO: update for unicode
CallResult ResourceFace::precache(const char *string)
{
	assert(settings.areValid());
	font->applySize(faceSize);
	for(int i = 0, c = string[i]; c != 0; c = string[++i])
	{
		uint tableIdx;
		if(mapCharToTable(c, tableIdx) != OK)
		{
			//logMsg( "%c not a known drawable character, skipping", c);
			continue;
		}
		if(glyphTable[tableIdx].glyph)
		{
			//logMsg( "%c already cached", c);
			continue;
		}

		logMsg("precaching char %c", c);
		cacheChar(c, tableIdx);
	}
	return OK;
}

GlyphEntry *ResourceFace::glyphEntry(int c)
{
	assert(settings.areValid());
	uint tableIdx;
	if(mapCharToTable(c, tableIdx) != OK)
		return nullptr;
	assert(tableIdx < glyphTableEntries);
	if(!glyphTable[tableIdx].glyph)
	{
		font->applySize(faceSize);
		if(cacheChar(c, tableIdx) != OK)
			return nullptr;
		logMsg("char 0x%X was not in table, cached", c);
	}

	return &glyphTable[tableIdx];
}

CallResult GfxGlyphImage::write(IG::Pixmap &dest)
{
	return face->writeCurrentChar(dest);
}

IG::Pixmap GfxGlyphImage::lockPixmap()
{
	return face->charBitmap();
}

void GfxGlyphImage::unlockPixmap()
{
	face->unlockCharBitmap();
}
