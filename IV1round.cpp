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
    if (argc < 3) {
        printf("Usage: IV1compress(.exe) image_input.png image_output.png");
    }

    printf("Reading image %s...", args[1]);
    const auto imagePath = args[1];
    const auto imageBlocks = BlockImage<blockW, blockH>(Support::LoadPNG(imagePath));
    if (imageBlocks.nBlocksX == 0 || imageBlocks.nBlocksY == 0) {
        return 0;
    }

    const auto blocksPalette = BlockRGBMean<float, 3 * blockW * blockH>(imageBlocks.data);
    const auto [dictPalette, idxPalette] = VQGenerateDictFast<float, 3, uint16_t>
                (blocksPalette, 256, 1000);
    const auto imgPalette = BlockImage<1, 1>(dictPalette, idxPalette, 
                imageBlocks.nBlocksX, imageBlocks.nBlocksY);

    const auto blocksDiff = BlockRGBSubtractMean<float, 3 * blockW * blockH>(imageBlocks.data, imgPalette.data);
    const auto [dictDiff, idxDiff] = VQGenerateDictFast<float, 3 * blockW * blockH, uint16_t>(blocksDiff, 256, 1000);
    auto imgDiff = BlockImage<blockW, blockH>(dictDiff, idxDiff, 
                imageBlocks.nBlocksX, imageBlocks.nBlocksY);

    char savePath[1024];
    snprintf(savePath, 1024, "%s.iv1", args[2]);
    printf("Saving compressed outpus as %s...\n", savePath);
    save(savePath, dictPalette, idxPalette, dictDiff, idxDiff, 
        imgDiff.nBlocksX, imgDiff.nBlocksY, imgDiff.actualW, imgDiff.actualH);

    imgDiff.data = BlockRGBAddMean<float, 3 * blockW * blockH>(imgDiff.data, imgPalette.data);
    const auto decodedImage = imgDiff.toRGB8Image();

    printf("Writing to image %s...\n", args[2]);
    const auto saveImagePath = args[2];
    Support::SavePNG(saveImagePath, decodedImage);

    return 0;
}
