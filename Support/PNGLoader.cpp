#include "PNGLoader.h"
#include <png.h>

#include <cassert>
#include <algorithm>
#include <cstdio>

// Graciously adapted from https://gist.github.com/niw/5963798

namespace VQLib::Support {

RGB8Image LoadPNG(Path path) {
    static const RGB8Image nullImage = {0, 0, {}};

    FILE* file = fopen(path, "rb");
    if (!file) {
        return nullImage;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        return nullImage;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        return nullImage;
    }

    png_init_io(png, file);


    constexpr auto png_transforms = 
          PNG_TRANSFORM_STRIP_ALPHA
        | PNG_TRANSFORM_STRIP_16 
        | PNG_TRANSFORM_EXPAND;
    png_read_png(png, info, png_transforms, nullptr);
    auto rows = png_get_rows(png, info);

    RGB8Image image;
    image.width           = png_get_image_width(png, info);
    image.height          = png_get_image_height(png, info);
    //png_byte color_type   = png_get_color_type(png, info);
    //png_byte bit_depth    = png_get_bit_depth(png, info);

    const auto row_size = image.width * 3;
    image.pixels.resize(image.width * image.height * 3);
    for(size_t y = 0; y < image.height; y++) {
        std::copy(rows[y], rows[y] + row_size,
                  &image.pixels[y * row_size]);
    }
    
    fclose(file);

    png_destroy_read_struct(&png, &info, NULL);
    
    return image;
}

void SavePNG(Path path, const RGB8Image& image) {
    FILE* file = fopen(path, "wb");
    if (!file) {
        return;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        return;
    }

    png_init_io(png, file);

    // Output is 8bit depth, RGB format.
    png_set_IHDR(
        png,
        info,
        image.width, image.height,
        8,
        PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    png_bytepp rows = (png_bytepp)malloc(sizeof(png_bytep) * image.height);

    
    for (size_t y = 0; y != image.height; ++y) {
        rows[y] = (png_bytep)malloc(3 * image.width);
        for (size_t x = 0; x != image.width; ++x) {
            for (size_t ch = 0; ch != 3; ++ch) {
                rows[y][(x * 3) + ch] =
                    image.pixels[(y * image.width + x) * 3 + ch];
            }
        }
    }

    png_set_rows(png, info, rows);
    png_write_png(png, info, 0, NULL);

    for (int y = 0; y != image.height; ++y) {
        free(rows[y]);
    }
    free(rows);

    png_write_end(png, info);
    fclose(file);
    png_destroy_write_struct(&png, &info);
}

}
