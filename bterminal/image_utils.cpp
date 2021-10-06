#include <malloc.h>
#include <memory.h>
#include <png.h>
#include "image_utils.h"

static void setRGB(uint8_t row[3], uint16_t hexValue)
{
    unsigned r = (hexValue & 0xF800) >> 11;
    unsigned g = (hexValue & 0x07E0) >> 5;
    unsigned b = hexValue & 0x001F;

    r = (r * 255) / 31;
    g = (g * 255) / 63;
    b = (b * 255) / 31;

    row[0] = r;
    row[1] = g;
    row[2] = b;
}

static int WriteImage(const char* filename, int width, int height, const void* buffer, int bpp)
{
	int code = 0;
	FILE *fp = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_bytep row = NULL;
	
	// Open file for writing (binary mode)
	fp = fopen(filename, "wb");
	if (fp == NULL) {
		fprintf(stderr, "Could not open file %s for writing\n", filename);
		code = 1;
		goto finalise;
	}

	// Initialize write structure
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		fprintf(stderr, "Could not allocate write struct\n");
		code = 1;
		goto finalise;
	}

	// Initialize info structure
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		fprintf(stderr, "Could not allocate info struct\n");
		code = 1;
		goto finalise;
	}

	// Setup Exception handling
	if (setjmp(png_jmpbuf(png_ptr))) {
		fprintf(stderr, "Error during png creation\n");
		code = 1;
		goto finalise;
	}

	png_init_io(png_ptr, fp);

	// Write header (8 bit colour depth)
	png_set_IHDR(png_ptr, info_ptr, width, height,
			8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

/*
	// Set title
	if (title != NULL) {
		png_text title_text;
		title_text.compression = PNG_TEXT_COMPRESSION_NONE;
		title_text.key = "Title";
		title_text.text = title;
		png_set_text(png_ptr, info_ptr, &title_text, 1);
	}
*/
	png_write_info(png_ptr, info_ptr);

	// Allocate memory for one row (3 bytes per pixel - RGB)
	row = (png_bytep) malloc(3 * width * sizeof(png_byte));

	// Write image data
	if(bpp==24)
	{
		const uint8_t* buf8 = (const uint8_t*)buffer;
		for (int y=0 ; y<height ; y++) {
			memcpy(row, buf8 + y*width*3, width*3);
			png_write_row(png_ptr, row);
		}
	} else
	if(bpp==16)
	{
		const uint16_t* buf16 = (const uint16_t*)buffer;
		for (int y=0 ; y<height ; y++) {
			for (int x=0 ; x<width ; x++) {
				setRGB(&(row[x*3]), buf16[y*width + x]);
			}
			png_write_row(png_ptr, row);
		}
	}

	// End write
	png_write_end(png_ptr, NULL);

	finalise:
	if (fp != NULL) fclose(fp);
	if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
	if (row != NULL) free(row);

	return code;
}

int WriteImage16(const char* filename, int width, int height, const void* buffer)
{
	return WriteImage(filename, width, height, buffer, 16);
}

int WriteImage24(const char* filename, int width, int height, const void* buffer)
{
	return WriteImage(filename, width, height, buffer, 24);
}