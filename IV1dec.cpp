// Implementation of IV1 (Image-VQ 1, or "Ivy-One") codec in C++, based off of MatLAB source.

#define VQLIB_VERBOSE_OUTPUT 1

#include "VQLib/C++/VQAlgorithm.h"

#include "IV1BlockImage.h"
#include "IV1File.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <type_traits>

using namespace VQLib;
using namespace IV1;


int main(int argc, char **args) {
    constexpr size_t blockW = 4;
    constexpr size_t blockH = 4;
    if (argc < 2) {
        printf("Usage: IV1dec(.exe) image_input.iv1 image_output.png");
    }

    IV1File inputImage(args[1]);

    BlockImage<blockW, blockH> imgDiff(inputImage.dict1, inputImage.indices1,
                   inputImage.header.nBlocksX, inputImage.header.nBlocksY);
    auto imgBase = VQDecode(inputImage.dict0, inputImage.indices0);
    imgDiff.data = BlockRGBAddMean<float, 3 * blockW * blockH>(imgDiff.data, imgBase);

    const auto decodedImage = imgDiff.toRGB8Image();

    printf("Writing to image %s...\n", args[2]);
    const auto saveImagePath = args[2];
    Support::SavePNG(saveImagePath, decodedImage);

    return 0;
}
