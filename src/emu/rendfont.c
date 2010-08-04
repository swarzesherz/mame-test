/***************************************************************************

    rendfont.c

    Rendering system font management.

    Copyright Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

***************************************************************************/

#include "emu.h"
#include "rendfont.h"
#include "rendutil.h"
#include "emuopts.h"
#include "zlib.h"

#ifdef UI_COLOR_DISPLAY
//mamep: for ui_get_rgb_color()
#include "ui.h"
#endif /* UI_COLOR_DISPLAY */

//mamep: embedded CJK and non-CJK font
#include "uismall11.fh"
#include "uismall14.fh"

//mamep: embedded command glyph
#include "uicmd11.fh"
#include "uicmd14.fh"


/***************************************************************************
    CONSTANTS
***************************************************************************/

#define FONT_FORMAT_TEXT		1
#define FONT_FORMAT_CACHED		2

#define CACHED_CHAR_SIZE		12
#define CACHED_HEADER_SIZE		16
#define CACHED_BDF_HASH_SIZE	1024

//mamep: command.dat symbols assigned to Unicode PUA U+E000
#define COMMAND_UNICODE	(0xe000)
#define MAX_GLYPH_FONT	(150)

#ifdef UI_COLOR_DISPLAY
//mamep: for color glyph
#define COLOR_BUTTONS	(90)
#endif /* UI_COLOR_DISPLAY */



/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

/* a render_font contains information about a single character in a font */
typedef struct _render_font_char render_font_char;
struct _render_font_char
{
	INT32				width;				/* width from this character to the next */
	INT32				xoffs, yoffs;		/* X and Y offset from baseline to top,left of bitmap */
	INT32				bmwidth, bmheight;	/* width and height of bitmap */
	const char *		rawdata;			/* pointer to the raw data for this one */
	bitmap_t *			bitmap;				/* pointer to the bitmap containing the raw data */
	render_texture *	texture;			/* pointer to a texture for rendering and sizing */
#ifdef UI_COLOR_DISPLAY
	//mamep: for color glyph
	int			color;
#endif /* UI_COLOR_DISPLAY */
};


/* a render_font contains information about a font */
/*typedef struct _render_font render_font; -- defined in rendfont.h */
struct _render_font
{
	int					format;				/* format of font data */
	int					height;				/* height of the font, from ascent to descent */
	int					yoffs;				/* y offset from baseline to descent */
	float				scale;				/* 1 / height precomputed */
	render_font_char *	chars[256];			/* array of character subtables */
	const char *		rawdata;			/* pointer to the raw data for the font */
	UINT64				rawsize;			/* size of the raw font data */
	//mamep: for command glyph
	render_font *		cmd;
};



/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/

static void render_font_char_expand(render_font *font, render_font_char *ch);
static int render_font_load_cached_bdf(render_font *font, const char *filename);
static int render_font_load_bdf(render_font *font);
static int render_font_load_cached(render_font *font, mame_file *file, UINT32 hash);
static int render_font_save_cached(render_font *font, const char *filename, UINT32 hash);



/***************************************************************************
    INLINE FUNCTIONS
***************************************************************************/

/*-------------------------------------------------
    next_line - return a pointer to the start of
    the next line
-------------------------------------------------*/

INLINE const char *next_line(const char *ptr)
{
	/* scan forward until we hit the end or a carriage return */
	while (*ptr != 13 && *ptr != 10 && *ptr != 0) ptr++;

	/* if we hit the end, return NULL */
	if (*ptr == 0)
		return NULL;

	/* eat the trailing linefeed if present */
	if (*++ptr == 10)
		ptr++;
	return ptr;
}


/*-------------------------------------------------
    get_char - return a pointer to a character
    in a font, expanding if necessary
-------------------------------------------------*/

INLINE render_font_char *get_char(render_font *font, unicode_char chnum)
{
	static render_font_char dummy_char;
	render_font_char *chtable;
	render_font_char *ch;

	/* grab the table; if none, return the dummy character */
	chtable = font->chars[chnum / 256];
	if (chtable == NULL)
	{
		//mamep: make table for command glyph
		if (chnum >= COMMAND_UNICODE && chnum < COMMAND_UNICODE + MAX_GLYPH_FONT)
		{
			chtable = global_alloc_array_clear(render_font_char, 256);
			font->chars[chnum / 256] = chtable;
		}
		else
			return &dummy_char;
	}

	/* if the character isn't generated yet, do it now */
	ch = &chtable[chnum % 256];
	if (ch->bitmap == NULL)
	{
#ifdef UI_COLOR_DISPLAY
		//mamep: color glyph
		#include "cmdtable.c"

		if (chnum >= COMMAND_UNICODE && chnum < COMMAND_UNICODE + COLOR_BUTTONS)
			ch->color = color_table[chnum - COMMAND_UNICODE];
#endif /* UI_COLOR_DISPLAY */

		//mamep: command glyph support
		if (font->cmd && chnum >= COMMAND_UNICODE && chnum < COMMAND_UNICODE + MAX_GLYPH_FONT)
		{
			render_font_char *glyph_ch = get_char(font->cmd, chnum);
			float scale = font->height / font->cmd->height;

			ch->width = (int)(glyph_ch->width * scale + 0.5f);
			ch->xoffs = (int)(glyph_ch->xoffs * scale + 0.5f);
			ch->yoffs = (int)(glyph_ch->yoffs * scale + 0.5f);
			ch->bmwidth = (int)(glyph_ch->bmwidth * scale + 0.5f);
			ch->bmheight = (int)(glyph_ch->bmheight * scale + 0.5f);

			ch->bitmap = global_alloc(bitmap_t(ch->bmwidth, ch->bmheight, BITMAP_FORMAT_ARGB32));
			render_texture_hq_scale(ch->bitmap, glyph_ch->bitmap, NULL, NULL);

			ch->texture = render_texture_alloc(render_texture_hq_scale, NULL);
			render_texture_set_bitmap(ch->texture, ch->bitmap, NULL, TEXFORMAT_ARGB32, NULL);
		}
		else
			render_font_char_expand(font, ch);
	}

	/* return the resulting character */
	return ch;
}



/***************************************************************************
    RENDER FONTS
***************************************************************************/

//mamep: allocate command glyph font
static render_font *render_font_alloc_command_glyph(int height)
{
	file_error filerr;
	mame_file *ramfile;
	render_font *font;

	/* allocate and clear memory */
	font = global_alloc_clear(render_font);

	if (height >= 14)
		filerr = mame_fopen_ram(font_uicmd14, sizeof(font_uicmd14), OPEN_FLAG_READ, &ramfile);
	else
		filerr = mame_fopen_ram(font_uicmd11, sizeof(font_uicmd11), OPEN_FLAG_READ, &ramfile);

	if (filerr == FILERR_NONE)
	{
		render_font_load_cached(font, ramfile, 0);
		mame_fclose(ramfile);
	}

	return font;
}

/*-------------------------------------------------
    render_font_alloc - allocate a new font
    and load the BDF file
-------------------------------------------------*/

render_font *render_font_alloc(const char *filename)
{
	file_error filerr;
	mame_file *ramfile;
	render_font *font;

	/* allocate and clear memory */
	font = global_alloc_clear(render_font);

	/* attempt to load the cached version of the font first */
	if (filename != NULL)
	{
		int loaded = 0;
		astring filename_local(ui_lang_info[lang_get_langcode()].name, PATH_SEPARATOR, filename);
//		mame_printf_warning("%s\n", filename_local);

	 	if (filename_local.len() > 0 && render_font_load_cached_bdf(font, filename_local) == 0)
			loaded++;
		else
		{
			/* if we failed, clean up and realloc */
			render_font_free(font);
			font = global_alloc_clear(render_font);

			if (render_font_load_cached_bdf(font, filename) == 0)
				loaded++;
		}

		if (loaded)
		{
			//mamep: allocate command glyph font
			font->cmd = render_font_alloc_command_glyph(font->height);
			return font;
		}
	}

	/* if we failed, clean up and realloc */
	render_font_free(font);
	font = global_alloc_clear(render_font);

	/* load the raw data instead */
	//mamep: embedded CJK font
	switch (lang_get_langcode())
	{
	case UI_LANG_JA_JP:
	case UI_LANG_ZH_CN:
	case UI_LANG_ZH_TW:
	case UI_LANG_KO_KR:
		filerr = mame_fopen_ram(font_uismall14, sizeof(font_uismall14), OPEN_FLAG_READ, &ramfile);
		break;

	default:
		filerr = mame_fopen_ram(font_uismall11, sizeof(font_uismall11), OPEN_FLAG_READ, &ramfile);
	}

	if (filerr == FILERR_NONE)
	{
		render_font_load_cached(font, ramfile, 0);
		mame_fclose(ramfile);
	}

	//mamep: allocate command glyph font
	font->cmd = render_font_alloc_command_glyph(font->height);
	return font;
}


/*-------------------------------------------------
    render_font_free - free an allocated font and
    all of its owned subobjects
-------------------------------------------------*/

void render_font_free(render_font *font)
{
	int tablenum;

	//mamep: free command glyph font
	if (font->cmd)
	{
		render_font_free(font->cmd);
		font->cmd = NULL;
	}

	/* free all the subtables */
	for (tablenum = 0; tablenum < 256; tablenum++)
		if (font->chars[tablenum] != NULL)
		{
			int charnum;

			/* loop over characters */
			for (charnum = 0; charnum < 256; charnum++)
			{
				render_font_char *ch = &font->chars[tablenum][charnum];
				if (ch->texture != NULL)
					render_texture_free(ch->texture);
				global_free(ch->bitmap);
			}

			/* free the subtable itself */
			global_free(font->chars[tablenum]);
		}

	/* free the raw data and the size itself */
	if (font->rawdata != NULL)
		global_free((void *)font->rawdata);
	global_free(font);
}


/*-------------------------------------------------
    render_font_char_expand - expand the raw data
    for a character into a bitmap
-------------------------------------------------*/

static void render_font_char_expand(render_font *font, render_font_char *ch)
{
	const char *ptr = ch->rawdata;
	UINT8 accum = 0, accumbit = 7;
	int x, y;
	rgb_t color = MAKE_ARGB(0xff,0xff,0xff,0xff);

#ifdef UI_COLOR_DISPLAY
	//mamep: for color glyph
	if (ch->color)
		color = ui_get_rgb_color(ch->color);
#endif /* UI_COLOR_DISPLAY */

	/* punt if nothing there */
	if (ch->bmwidth == 0 || ch->bmheight == 0 || ch->rawdata == NULL)
		return;

	/* allocate a new bitmap of the size we need */
	ch->bitmap = global_alloc(bitmap_t(ch->bmwidth, font->height, BITMAP_FORMAT_ARGB32));
	bitmap_fill(ch->bitmap, NULL, 0);

	/* extract the data */
	for (y = 0; y < ch->bmheight; y++)
	{
		int desty = y + font->height + font->yoffs - ch->yoffs - ch->bmheight;
		UINT32 *dest = (desty >= 0 && desty < font->height) ? BITMAP_ADDR32(ch->bitmap, desty, 0) : NULL;

		/* text format */
		if (font->format == FONT_FORMAT_TEXT)
		{
			/* loop over bytes */
			for (x = 0; x < ch->bmwidth; x += 4)
			{
				int bits = -1;

				/* scan for the next hex digit */
				while (*ptr != 13 && bits == -1)
				{
					if (*ptr >= '0' && *ptr <= '9')
						bits = *ptr++ - '0';
					else if (*ptr >= 'A' && *ptr <= 'F')
						bits = *ptr++ - 'A' + 10;
					else if (*ptr >= 'a' && *ptr <= 'f')
						bits = *ptr++ - 'a' + 10;
					else
						ptr++;
				}

				/* expand the four bits */
				if (dest != NULL)
				{
					*dest++ = (bits & 8) ? color : MAKE_ARGB(0x00,0xff,0xff,0xff);
					*dest++ = (bits & 4) ? color : MAKE_ARGB(0x00,0xff,0xff,0xff);
					*dest++ = (bits & 2) ? color : MAKE_ARGB(0x00,0xff,0xff,0xff);
					*dest++ = (bits & 1) ? color : MAKE_ARGB(0x00,0xff,0xff,0xff);
				}
			}

			/* advance to the next line */
			ptr = next_line(ptr);
		}

		/* cached format */
		else if (font->format == FONT_FORMAT_CACHED)
		{
			for (x = 0; x < ch->bmwidth; x++)
			{
				if (accumbit == 7)
					accum = *ptr++;
				if (dest != NULL)
					*dest++ = (accum & (1 << accumbit)) ? color : MAKE_ARGB(0x00,0xff,0xff,0xff);
				accumbit = (accumbit - 1) & 7;
			}
		}
	}

	/* wrap a texture around the bitmap */
	ch->texture = render_texture_alloc(render_texture_hq_scale, NULL);
	render_texture_set_bitmap(ch->texture, ch->bitmap, NULL, TEXFORMAT_ARGB32, NULL);
}


/*-------------------------------------------------
    render_font_get_pixel_height - return the
    height of the font in pixels
-------------------------------------------------*/

INT32 render_font_get_pixel_height(render_font *font)
{
	return font->height;
}


/*-------------------------------------------------
    render_font_get_char_texture_and_bounds -
    return the texture for a character and compute
    the bounds of the final bitmap
-------------------------------------------------*/

render_texture *render_font_get_char_texture_and_bounds(render_font *font, float height, float aspect, unicode_char chnum, render_bounds *bounds)
{
	render_font_char *ch = get_char(font, chnum);
	float scale = font->scale * height;

	/* on entry, assume x0,y0 are the top,left coordinate of the cell and add */
	/* the character bounding box to that position */
	bounds->x0 += (float)ch->xoffs * scale * aspect;

	/* compute x1,y1 from there based on the bitmap size */
	bounds->x1 = bounds->x0 + (float)ch->bmwidth * scale * aspect;
	bounds->y1 = bounds->y0 + (float)font->height * scale;

	/* return the texture */
	return ch->texture;
}


/*-------------------------------------------------
    render_font_draw_string_to_bitmap - draw a
    string to a bitmap
-------------------------------------------------*/

void render_font_get_scaled_bitmap_and_bounds(render_font *font, bitmap_t *dest, float height, float aspect, unicode_char chnum, rectangle *bounds)
{
	render_font_char *ch = get_char(font, chnum);
	float scale = font->scale * height;
	INT32 origwidth, origheight;

	/* on entry, assume x0,y0 are the top,left coordinate of the cell and add */
	/* the character bounding box to that position */
	bounds->min_x = (float)ch->xoffs * scale * aspect;
	bounds->min_y = 0;

	/* compute x1,y1 from there based on the bitmap size */
	bounds->max_x = bounds->min_x + (float)ch->bmwidth * scale * aspect;
	bounds->max_y = bounds->min_y + (float)font->height * scale;

	/* if the bitmap isn't big enough, bail */
	if (dest->width < bounds->max_x - bounds->min_x || dest->height < bounds->max_y - bounds->min_y)
		return;

	/* scale the font */
	origwidth = dest->width;
	origheight = dest->height;
	dest->width = bounds->max_x - bounds->min_x;
	dest->height = bounds->max_y - bounds->min_y;
	render_texture_hq_scale(dest, ch->bitmap, NULL, NULL);
	dest->width = origwidth;
	dest->height = origheight;
}


/*-------------------------------------------------
    render_font_char_width - return the width of
    a character at the given height
-------------------------------------------------*/

float render_font_get_char_width(render_font *font, float height, float aspect, unicode_char ch)
{
	return (float)get_char(font, ch)->width * font->scale * height * aspect;
}

//mamep: to render as fixed-width font
float render_font_get_char_width_no_margin(render_font *font, float height, float aspect, unicode_char ch)
{
	return (float)(get_char(font, ch)->width - 1) * font->scale * height * aspect;
}


/*-------------------------------------------------
    render_font_string_width - return the width of
    a string at the given height
-------------------------------------------------*/

float render_font_get_string_width(render_font *font, float height, float aspect, const char *string)
{
	const unsigned char *ptr;
	int totwidth = 0;

	/* loop over the string and accumulate widths */
	for (ptr = (const unsigned char *)string; *ptr != 0; ptr++)
		totwidth += get_char(font, *ptr)->width;

	/* scale the final result based on height */
	return (float)totwidth * font->scale * height * aspect;
}


/*-------------------------------------------------
    render_font_get_utf8string_width - return the
    width of a UTF8-encoded string at the given
    height
-------------------------------------------------*/

float render_font_get_utf8string_width(render_font *font, float height, float aspect, const char *utf8string)
{
	int length = strlen(utf8string);
	unicode_char uchar;
	int totwidth = 0;
	int count = 0;
	int offset;

	/* loop over the string and accumulate widths */
	for (offset = 0; offset < length; offset += count)
	{
		count = uchar_from_utf8(&uchar, utf8string + offset, length - offset);
		if (count == -1)
			break;
		if (uchar < 0x10000)
			totwidth += get_char(font, uchar)->width;
	}

	/* scale the final result based on height */
	return (float)totwidth * font->scale * height * aspect;
}


/*-------------------------------------------------
    render_font_load_cached_bdf - attempt to load
    a cached version of the BDF font 'filename';
    if that fails, fall back on the regular BDF
    loader and create a new cached version
-------------------------------------------------*/

static int render_font_load_cached_bdf(render_font *font, const char *filename)
{
	file_error filerr;
	char *cachedname = NULL;
	char *data = NULL;
	mame_file *cachefile;
	mame_file *file;
	int result = 1;
	UINT32 bytes;
	UINT32 hash;

	/* first try to open the BDF itself */
	filerr = mame_fopen(SEARCHPATH_FONT, filename, OPEN_FLAG_READ, &file);
	if (filerr != FILERR_NONE)
		return 1;

	/* determine the file size and allocate memory */
	font->rawsize = mame_fsize(file);
	data = global_alloc_array_clear(char, font->rawsize + 1);

	/* read and hash the first chunk */
	bytes = mame_fread(file, data, MIN(CACHED_BDF_HASH_SIZE, font->rawsize));
	if (bytes != MIN(CACHED_BDF_HASH_SIZE, font->rawsize))
		goto error;
	hash = crc32(0, (const UINT8*)data, bytes) ^ (UINT32)font->rawsize;

	/* create the cached filename */
	cachedname = mame_strdup(filename);
	if (cachedname == NULL)
		goto error;

	/* change the 'F' to a 'C' on the extension */
	cachedname[strlen(cachedname) - 1] -= 3;

	/* attempt to load a cached version of the font */
	filerr = mame_fopen(SEARCHPATH_FONT, cachedname, OPEN_FLAG_READ, &cachefile);
	if (filerr == FILERR_NONE)
	{
		result = render_font_load_cached(font, cachefile, hash);
		mame_fclose(cachefile);
	}
	if (result != 0)
	{
		/* if that failed, read the rest of the font and parse it */
		if (bytes < font->rawsize)
		{
			UINT32 read = mame_fread(file, data + bytes, font->rawsize - bytes);
			if (read != font->rawsize - bytes)
				goto error;
		}

		/* NULL-terminate the data and attach it to the font */
		data[font->rawsize] = 0;
		font->rawdata = data;

		/* load the BDF */
		result = render_font_load_bdf(font);

		/* if we loaded okay, create a cached one */
		if (result == 0)
			render_font_save_cached(font, cachedname, hash);
	}
	else
		global_free(data);

	/* close the file */
	osd_free(cachedname);
	mame_fclose(file);
	return result;

error:
	/* close the file */
	if (cachedname != NULL)
		osd_free(cachedname);
	if (data != NULL)
		global_free(data);
	mame_fclose(file);
	return 1;
}


/*-------------------------------------------------
    render_font_load_bdf - parse and load a BDF
    font
-------------------------------------------------*/

static int render_font_load_bdf(render_font *font)
{
	const char *ptr;
	int charcount = 0;

	/* set the format to text */
	font->format = FONT_FORMAT_TEXT;

	/* first find the FONTBOUNDINGBOX tag */
	for (ptr = font->rawdata; ptr != NULL; ptr = next_line(ptr))
	{
		int dummy1, dummy2;

		/* we only care about a tiny few fields */
		if (strncmp(ptr, "FONTBOUNDINGBOX ", 16) == 0)
		{
			if (sscanf(ptr + 16, "%d %d %d %d", &dummy1, &font->height, &dummy2, &font->yoffs) != 4)
				return 1;
			break;
		}
	}

	/* compute the scale factor */
	font->scale = 1.0f / (float)font->height;

	/* now scan for characters */
	for ( ; ptr != NULL; ptr = next_line(ptr))
	{
		/* stop at ENDFONT */
		if (strncmp(ptr, "ENDFONT", 7) == 0)
			break;

		/* once we hit a STARTCHAR, parse until the end */
		if (strncmp(ptr, "STARTCHAR ", 10) == 0)
		{
			int bmwidth = -1, bmheight = -1, xoffs = -1, yoffs = -1;
			const char *rawdata = NULL;
			int charnum = -1;
			int width = -1;
			int dummy1;

			/* scan for interesting per-character tags */
			for ( ; ptr != NULL; ptr = next_line(ptr))
			{
				/* ENCODING tells us which character */
				if (strncmp(ptr, "ENCODING ", 9) == 0)
				{
					if (sscanf(ptr + 9, "%d", &charnum) != 1)
						return 1;
				}

				/* DWIDTH tells us the width to the next character */
				else if (strncmp(ptr, "DWIDTH ", 7) == 0)
				{
					if (sscanf(ptr + 7, "%d %d", &width, &dummy1) != 2)
						return 1;
				}

				/* BBX tells us the height/width of the bitmap and the offsets */
				else if (strncmp(ptr, "BBX ", 4) == 0)
				{
					if (sscanf(ptr + 4, "%d %d %d %d", &bmwidth, &bmheight, &xoffs, &yoffs) != 4)
						return 1;
				}

				/* BITMAP is the start of the data */
				else if (strncmp(ptr, "BITMAP", 6) == 0)
				{
					/* stash the raw pointer and scan for the end of the character */
					for (rawdata = ptr = next_line(ptr); ptr != NULL && strncmp(ptr, "ENDCHAR", 7) != 0; ptr = next_line(ptr)) ;
					break;
				}
			}

			/* if we have everything, allocate a new character */
			if (charnum >= 0 && charnum < 65536 && rawdata != NULL && bmwidth >= 0 && bmheight >= 0)
			{
				render_font_char *ch;

				/* if we don't have a subtable yet, make one */
				if (font->chars[charnum / 256] == NULL)
					font->chars[charnum / 256] = global_alloc_array_clear(render_font_char, 256);

				/* fill in the entry */
				ch = &font->chars[charnum / 256][charnum % 256];
				ch->width = width;
				ch->bmwidth = bmwidth;
				ch->bmheight = bmheight;
				ch->xoffs = xoffs;
				ch->yoffs = yoffs;
				ch->rawdata = rawdata;
			}

			/* some progress for big fonts */
			if (++charcount % 256 == 0)
				mame_printf_warning(_("Loading BDF font... (%d characters loaded)\n"), charcount);
		}
	}

	/* make sure all the numbers are the same width */
	if (font->chars[0] != NULL)
	{
		int maxwidth = 0;
		for (int ch = '0'; ch <= '9'; ch++)
			if (font->chars[0][ch].bmwidth > maxwidth)
				maxwidth = font->chars[0][ch].width;
		for (int ch = '0'; ch <= '9'; ch++)
			font->chars[0][ch].width = maxwidth;
	}

	return 0;
}


/*-------------------------------------------------
    render_font_load_cached - load a font in
    cached format
-------------------------------------------------*/

static int render_font_load_cached(render_font *font, mame_file *file, UINT32 hash)
{
	UINT8 header[CACHED_HEADER_SIZE];
	UINT64 offset, filesize;
	UINT8 *data = NULL;
	UINT32 bytes_read;
	int numchars;
	int chindex;

	/* get the file size */
	filesize = mame_fsize(file);

	/* first read the header */
	bytes_read = mame_fread(file, header, CACHED_HEADER_SIZE);
	if (bytes_read != CACHED_HEADER_SIZE)
		goto error;

	/* validate the header */
	if (header[0] != 'f' || header[1] != 'o' || header[2] != 'n' || header[3] != 't')
		goto error;
	if (header[4] != (UINT8)(hash >> 24) || header[5] != (UINT8)(hash >> 16) || header[6] != (UINT8)(hash >> 8) || header[7] != (UINT8)hash)
		goto error;
	font->height = (header[8] << 8) | header[9];
	font->scale = 1.0f / (float)font->height;
	font->yoffs = (INT16)((header[10] << 8) | header[11]);
	numchars = (header[12] << 24) | (header[13] << 16) | (header[14] << 8) | header[15];
	if (filesize - CACHED_HEADER_SIZE < numchars * CACHED_CHAR_SIZE)
		goto error;

	/* now read the rest of the data */
	data = global_alloc_array(UINT8, filesize - CACHED_HEADER_SIZE);
	bytes_read = mame_fread(file, data, filesize - CACHED_HEADER_SIZE);
	if (bytes_read != filesize - CACHED_HEADER_SIZE)
		goto error;

	/* extract the data from the data */
	offset = numchars * CACHED_CHAR_SIZE;
	for (chindex = 0; chindex < numchars; chindex++)
	{
		const UINT8 *info = &data[chindex * CACHED_CHAR_SIZE];
		int chnum = (info[0] << 8) | info[1];
		render_font_char *ch;

		/* if we don't have a subtable yet, make one */
		if (font->chars[chnum / 256] == NULL)
			font->chars[chnum / 256] = global_alloc_array_clear(render_font_char, 256);

		/* fill in the entry */
		ch = &font->chars[chnum / 256][chnum % 256];
		ch->width = (info[2] << 8) | info[3];
		ch->xoffs = (INT16)((info[4] << 8) | info[5]);
		ch->yoffs = (INT16)((info[6] << 8) | info[7]);
		ch->bmwidth = (info[8] << 8) | info[9];
		ch->bmheight = (info[10] << 8) | info[11];
		ch->rawdata = (char *)data + offset;

		/* advance the offset past the character */
		offset += (ch->bmwidth * ch->bmheight + 7) / 8;
		if (offset > filesize - CACHED_HEADER_SIZE)
			goto error;
	}

	/* reuse the chartable as a temporary buffer */
	font->format = FONT_FORMAT_CACHED;
	font->rawdata = (char *)data;
	return 0;

error:
	if (data != NULL)
		global_free(data);
	return 1;
}


/*-------------------------------------------------
    render_font_save_cached - save a font in
    cached format
-------------------------------------------------*/

static int render_font_save_cached(render_font *font, const char *filename, UINT32 hash)
{
	file_error filerr;
	render_font_char *ch;
	UINT32 bytes_written;
	UINT8 *tempbuffer;
	UINT8 *chartable;
	mame_file *file;
	int numchars;
	UINT8 *dest;
	int tableindex;
	int chnum;

	mame_printf_warning(_("Generating cached BDF font...\n"));

	/* attempt to open the file */
	filerr = mame_fopen(SEARCHPATH_FONT, filename, OPEN_FLAG_WRITE | OPEN_FLAG_CREATE, &file);
	if (filerr != FILERR_NONE)
		return 1;

	/* determine the number of characters */
	numchars = 0;
	for (chnum = 0; chnum < 65536; chnum++)
	{
		render_font_char *chtable = font->chars[chnum / 256];
		if (chtable != NULL)
		{
			ch = &chtable[chnum % 256];
			if (ch->width > 0)
				numchars++;
		}
	}

	/* allocate an array to hold the character data */
	chartable = global_alloc_array_clear(UINT8, numchars * CACHED_CHAR_SIZE);

	/* allocate a temp buffer to compress into */
	tempbuffer = global_alloc_array(UINT8, 65536);

	/* write the header */
	dest = tempbuffer;
	*dest++ = 'f';
	*dest++ = 'o';
	*dest++ = 'n';
	*dest++ = 't';
	*dest++ = hash >> 24;
	*dest++ = hash >> 16;
	*dest++ = hash >> 8;
	*dest++ = hash & 0xff;
	*dest++ = font->height >> 8;
	*dest++ = font->height & 0xff;
	*dest++ = font->yoffs >> 8;
	*dest++ = font->yoffs & 0xff;
	*dest++ = numchars >> 24;
	*dest++ = numchars >> 16;
	*dest++ = numchars >> 8;
	*dest++ = numchars & 0xff;
	assert(dest - tempbuffer == CACHED_HEADER_SIZE);
	bytes_written = mame_fwrite(file, tempbuffer, dest - tempbuffer);
	if (bytes_written != dest - tempbuffer)
		goto error;

	/* write the empty table to the beginning of the file */
	bytes_written = mame_fwrite(file, chartable, numchars * CACHED_CHAR_SIZE);
	if (bytes_written != numchars * CACHED_CHAR_SIZE)
		goto error;

	/* loop over all characters */
	tableindex = 0;
	for (chnum = 0; chnum < 65536; chnum++)
	{
		ch = get_char(font, chnum);
		if (ch != NULL && ch->width > 0)
		{
			UINT8 accum, accbit;
			int x, y;

			/* write out a bit-compressed bitmap if we have one */
			if (ch->bitmap != NULL)
			{
				/* write the data to the tempbuffer */
				dest = tempbuffer;
				accum = 0;
				accbit = 7;

				/* bit-encode the character data */
				for (y = 0; y < ch->bmheight; y++)
				{
					int desty = y + font->height + font->yoffs - ch->yoffs - ch->bmheight;
					const UINT32 *src = (desty >= 0 && desty < font->height) ? BITMAP_ADDR32(ch->bitmap, desty, 0) : NULL;
					for (x = 0; x < ch->bmwidth; x++)
					{
						if (src != NULL && RGB_ALPHA(src[x]) != 0)
							accum |= 1 << accbit;
						if (accbit-- == 0)
						{
							*dest++ = accum;
							accum = 0;
							accbit = 7;
						}
					}
				}

				/* flush any extra */
				if (accbit != 7)
					*dest++ = accum;

				/* write the data */
				bytes_written = mame_fwrite(file, tempbuffer, dest - tempbuffer);
				if (bytes_written != dest - tempbuffer)
					goto error;

				/* free the bitmap and texture */
				if (ch->texture != NULL)
					render_texture_free(ch->texture);
				ch->texture = NULL;
				global_free(ch->bitmap);
				ch->bitmap = NULL;
			}

			/* compute the table entry */
			dest = &chartable[tableindex++ * CACHED_CHAR_SIZE];
			*dest++ = chnum >> 8;
			*dest++ = chnum & 0xff;
			*dest++ = ch->width >> 8;
			*dest++ = ch->width & 0xff;
			*dest++ = ch->xoffs >> 8;
			*dest++ = ch->xoffs & 0xff;
			*dest++ = ch->yoffs >> 8;
			*dest++ = ch->yoffs & 0xff;
			*dest++ = ch->bmwidth >> 8;
			*dest++ = ch->bmwidth & 0xff;
			*dest++ = ch->bmheight >> 8;
			*dest++ = ch->bmheight & 0xff;
		}
	}

	/* seek back to the beginning and rewrite the table */
	mame_fseek(file, CACHED_HEADER_SIZE, SEEK_SET);
	bytes_written = mame_fwrite(file, chartable, numchars * CACHED_CHAR_SIZE);
	if (bytes_written != numchars * CACHED_CHAR_SIZE)
		goto error;

	/* all done */
	mame_fclose(file);
	global_free(tempbuffer);
	global_free(chartable);
	return 0;

error:
	mame_fclose(file);
	osd_rmfile(filename);
	global_free(tempbuffer);
	global_free(chartable);
	return 1;
}


void convert_command_glyph(char *s, int buflen)
{
	unicode_char uchar;
	char *d;
	int ucharcount;
	int len;
	int i, j;

#include "cmdplus.c"

	d = global_alloc_array(char, buflen * sizeof (*d));
	if (d == NULL)
	{
		*s = '\0';
		return;
	}

	len = strlen(s);
	for (i = j = 0; i < len; )
	{
		struct fix_command_t *fixcmd = NULL;

		ucharcount = uchar_from_utf8(&uchar, s + i, len - i);
		if (ucharcount == -1)
			break;

		if (ucharcount != 1)
			goto process_next;

		if (s[i]  == '\\' && s[i + 1] == 'n')
		{
			uchar = '\n';
			goto process_next;
		}

#ifdef COMMAND_CONVERT_TEXT
		if (s[i] == COMMAND_CONVERT_TEXT)
		{
			struct fix_strings_t *fixtext = convert_text;

			if (s[i]  == s[i + 1])
			{
				i++;
				goto process_next;
			}

			while (fixtext->glyph_code)
			{
				if (!fixtext->glyph_str_len)
					fixtext->glyph_str_len = strlen(fixtext->glyph_str);

				if (strncmp(fixtext->glyph_str, s + i + 1, fixtext->glyph_str_len) == 0)
				{
					uchar = fixtext->glyph_code + COMMAND_UNICODE;
					i += strlen(fixtext->glyph_str);
					goto process_next;
				}

				fixtext++;
			}
		}
#endif /* COMMAND_CONVERT_TEXT */

#ifdef COMMAND_DEFAULT_TEXT
		if (s[i] == COMMAND_DEFAULT_TEXT)
			fixcmd = default_text;
#endif /* COMMAND_DEFAULT_TEXT */

#ifdef COMMAND_EXPAND_TEXT
		if (s[i] == COMMAND_EXPAND_TEXT)
			fixcmd = expand_text;
#endif /* COMMAND_EXPAND_TEXT */

		if (fixcmd)
		{
			if (s[i]  == s[i + 1])
			{
				i++;
				goto process_next;
			}

			while (fixcmd->glyph_code)
			{
				if (s[i + 1] == fixcmd->glyph_char)
				{
					uchar = fixcmd->glyph_code + COMMAND_UNICODE;
					i++;
					goto process_next;
				}

				fixcmd++;
			}
		}

process_next:
		i += ucharcount;

		ucharcount = utf8_from_uchar(d + j, buflen - j - 1, uchar);
		if (ucharcount == -1)
			break;

		j += ucharcount;
	}

	d[j] = '\0';

	strcpy(s, d);
	global_free(d);
}
