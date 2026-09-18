/*
 * A simple libpng example program
 * http://zarb.org/~gc/html/libpng.html
 *
 * Modified by Yoshimasa Niwa to make it much simpler
 * and support all defined color_type.
 *
 * To build, use the next instruction on OS X.
 * $ brew install libpng
 * $ clang -lz -lpng16 libpng_test.c
 *
 * Copyright 2002-2010 Guillaume Cottenceau.
 *
 * This software may be freely redistributed under the terms
 * of the X11 license.
 *
 */

#include <iostream>
#include <fstream>
#include <png.h>
#include <cstring>


int width, height;
png_byte color_type;
png_byte bit_depth;
png_bytep *row_pointers = NULL;

void read_png_file(char *filename) {
  FILE *fp = fopen(filename, "rb");

  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png) abort();

  png_infop info = png_create_info_struct(png);
  if(!info) abort();

  if(setjmp(png_jmpbuf(png))) abort();

  png_init_io(png, fp);

  png_read_info(png, info);

  width      = png_get_image_width(png, info);
  height     = png_get_image_height(png, info);
  color_type = png_get_color_type(png, info);
  bit_depth  = png_get_bit_depth(png, info);

  if(bit_depth == 16)
    png_set_strip_16(png);

  if(color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png);

  if(png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  if(color_type == PNG_COLOR_TYPE_RGB ||
     color_type == PNG_COLOR_TYPE_GRAY ||
     color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

  if(color_type == PNG_COLOR_TYPE_GRAY ||
     color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  png_read_update_info(png, info);

  if (row_pointers) abort();

  row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
  for(int y = 0; y < height; y++) {
    row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png,info));
  }

  png_read_image(png, row_pointers);

  fclose(fp);

  png_destroy_read_struct(&png, &info, NULL);
}

void write_png_file(char *filename) {
  int y;

  FILE *fp = fopen(filename, "wb");
  if(!fp) abort();

  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) abort();

  png_infop info = png_create_info_struct(png);
  if (!info) abort();

  if (setjmp(png_jmpbuf(png))) abort();

  png_init_io(png, fp);

  png_set_IHDR(
    png,
    info,
    width, height,
    8,
    PNG_COLOR_TYPE_RGBA,
    PNG_INTERLACE_NONE,
    PNG_COMPRESSION_TYPE_DEFAULT,
    PNG_FILTER_TYPE_DEFAULT
  );
  png_write_info(png, info);

  if (!row_pointers) abort();

  png_write_image(png, row_pointers);
  png_write_end(png, NULL);

  for(int y = 0; y < height; y++) {
    free(row_pointers[y]);
  }
  free(row_pointers);

  fclose(fp);

  png_destroy_write_struct(&png, &info);
}

png_byte calculate_grayscale(png_bytep px) {
  double red = px[0];
  double green = px[1];
  double blue = px[2];

  // Use the standard formula for converting to greyscale based on luminosity
  double grayscale = 0.299 * red + 0.587 * green + 0.114 * blue;

  return (png_byte)grayscale; // Cast the grayscale value to a byte
}



png_bytep calculate_average(png_bytep row, int x, int y, int channels) {
  int sumRed = 0, sumGreen = 0, sumBlue = 0;
  int neighborCount = 0;


  for (int dy = -2; dy <= 2; dy++) {
    int ny = y + dy;
    if (ny >= 0 && ny < height) {
      for (int dx = -2; dx <= 2; dx++) {
        int nx = x + dx;
        if (nx >= 0 && nx < width) {
          png_bytep neighbor = &(row_pointers[ny][nx * channels]);
          sumRed += neighbor[0];
          sumGreen += neighbor[1];
          sumBlue += neighbor[2];
          neighborCount++;
        }
      }
    }
  }


  png_bytep result = (png_bytep)malloc(channels * sizeof(png_byte));
  if (neighborCount > 0) {
    result[0] = (png_byte)(sumRed / neighborCount);
    result[1] = (png_byte)(sumGreen / neighborCount);
    result[2] = (png_byte)(sumBlue / neighborCount);
  } else {
    memcpy(result, &(row[x * channels]), channels * sizeof(png_byte));
  }
  return result;
}





void process_png_file() {
  for (int y = 0; y < height; y++) {
    png_bytep row = row_pointers[y];
    for (int x = 0; x < width; x++) {
      png_bytep px = &(row[x * 4]);


    png_bytep average_color = calculate_average(row, x, y, 4);
      px[0] = average_color[0]; 
      px[1] = average_color[1]; 
      px[2] = average_color[2]; 

      free(average_color); 
    }
  }
}

int main(int argc, char *argv[]) {
  if(argc != 3) abort();

  read_png_file(argv[1]);
  process_png_file();
  write_png_file(argv[2]);

  return 0;
}
