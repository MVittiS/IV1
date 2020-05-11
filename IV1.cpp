// Implementation of IV1 (Image-VQ 1, or "Ivy-One") codec in C++, based off of MatLAB source.

#define VQLIB_VERBOSE_OUTPUT 1

#include "../VQAlgorithm.h"
#include "../Support/PNGLoader.h"

#include <algorithm>
#include <cassert>
#include <type_traits>

using namespace VQLib;

template<size_t blockW, size_t blockH>
struct BlockImage {
    size_t nBlocksX, nBlocksY;
    static constexpr size_t channels = 3;
    FlexMatrix<float, blockW * blockH * channels> data;

    BlockImage(const Support::RGB8Image& image) {
        assert(image.width % blockW == 0);
        assert(image.height % blockH == 0);

        nBlocksX = image.width / blockW;
        nBlocksY = image.height / blockH;
        data.resize(nBlocksX * nBlocksY);

        for (size_t blockY = 0; blockY != nBlocksY; ++blockY) {
            for (size_t blockX = 0; blockX != nBlocksX; ++blockX) {
                auto& block = data[blockY * nBlocksX + blockX];

                // All the code below will be vectorized, because
                //  the loop limits are constexpr.
                for (size_t y = 0; y != blockH; ++y) {
                    for (size_t x = 0; x != blockW; ++x) {
                        for (size_t ch = 0; ch != channels; ++ch) {
                            block[(y * blockW + x) * 3 + ch] = image.pixels[
                                ((blockY * blockH + y) * image.width +
                                (blockX * blockW) + x) * 3 + ch];
                        }
                    }
                }
            }
        }
    }

    template<typename Index>
    BlockImage(const FlexMatrix<float, blockW * blockH * channels>& dict,
        const std::vector<Index>& indices, size_t nBlocksX, size_t nBlocksY)
    : nBlocksX(nBlocksX)
    , nBlocksY(nBlocksY) {
        data.resize(nBlocksX * nBlocksY);
        for (size_t idx = 0; idx != nBlocksX * nBlocksY; ++idx) {
            data[idx] = dict[indices[idx]];
        }
    }

    Support::RGB8Image toRGB8Image() const {
        Support::RGB8Image image;
        image.width = blockW * nBlocksX;
        image.height = blockH * nBlocksY;
        image.pixels.resize(image.width * image.height * 3);

        for (size_t blockY = 0; blockY != nBlocksY; ++blockY) {
            for (size_t blockX = 0; blockX != nBlocksX; ++blockX) {
                auto& block = data[blockY * nBlocksX + blockX];

                for (size_t y = 0; y != blockH; ++y) {
                    for (size_t x = 0; x != blockW; ++x) {
                        for (size_t ch = 0; ch != channels; ++ch) {
                            image.pixels[((blockY * blockH + y) * image.width +
                                (blockX * blockW) + x) * 3 + ch] = std::round(std::clamp(
                                    block[(y * blockW + x) * 3 + ch],   
                                    0.f, 255.f));
                        }
                    }
                }
            }
        }

        return image;
    }

    //TODO: Function that converts to YCoCg
};

template <typename T, size_t width>
FlexMatrix<T, 3>  BlockRGBMean(const FlexMatrix<T, width>& data) {
    static_assert(width % 3 == 0, "Element width must be a multiple of 3!");

    const auto dataSize = data.size();
    FlexMatrix<T, 3> mean(dataSize);

    for(size_t idx = 0; idx != dataSize; ++idx) {
        T accRed(0);
        T accGreen(0);
        T accBlue(0);
        
        for (size_t pixel = 0; pixel != (width / 3); ++pixel) {
            accRed += data[idx][3 * pixel];
            accGreen += data[idx][3 * pixel + 1];
            accBlue += data[idx][3 * pixel + 2];
        }

        if constexpr (std::is_floating_point_v<T>) {
            constexpr T invWeight = T(3) / T(width);
            accRed *= invWeight;
            accGreen *= invWeight;
            accBlue *= invWeight;
        }
        else {
            accRed = 3 * accRed / width;
            accGreen = 3 * accGreen / width;
            accBlue = 3 * accBlue / width;
        }

        mean[idx][0] = accRed;
        mean[idx][1] = accGreen;
        mean[idx][2] = accBlue;
    }

    return mean;
}

template <typename T, size_t width>
FlexMatrix<T, width>  BlockRGBSubtractMean(
    const FlexMatrix<T, width>& data, 
    const FlexMatrix<T, 3>& mean) {

    static_assert(width % 3 == 0, "Element width must be a multiple of 3!");
    assert(data.size() == mean.size());

    const auto dataSize = data.size();
    FlexMatrix<T, width> output(dataSize);

    for (size_t idx = 0; idx != dataSize; ++idx) {
        const T red = mean[idx][0];
        const T green = mean[idx][1];
        const T blue = mean[idx][2];
        for (size_t pixel = 0; pixel != (width / 3); ++pixel) {
            output[idx][pixel * 3] = data[idx][pixel * 3] - red;
            output[idx][pixel * 3 + 1] = data[idx][pixel * 3 + 1] - green;
            output[idx][pixel * 3 + 2] = data[idx][pixel * 3 + 2] - blue;
        }
    }

    return output;
}

template <typename T, size_t width>
FlexMatrix<T, width>  BlockRGBAddMean(
    const FlexMatrix<T, width>& data, 
    const FlexMatrix<T, 3>& mean) {

    static_assert(width % 3 == 0, "Element width must be a multiple of 3!");

    const auto negativeMean = [&]{
        auto negativeMean(mean);
        const auto dataSize = mean.size();
        for (size_t idx = 0; idx != dataSize; ++idx) {
            for (size_t elem = 0; elem != 3; ++elem) {
                negativeMean[idx][elem] = -negativeMean[idx][elem];
            }
        }

        return negativeMean;
    }();

    return BlockRGBSubtractMean<T, width>(data, negativeMean);
}


int main(int argc, char **args) {
    constexpr size_t blockW = 4;
    constexpr size_t blockH = 4;
    assert(argc > 2);
    
    printf("Reading image %s...", args[1]);
    const auto imagePath = args[1];
    const auto imageBlocks = BlockImage<blockW, blockH>(Support::LoadPNG(imagePath));
    
    const auto blocksPalette = BlockRGBMean<float, 3 * blockW * blockH>(imageBlocks.data);
    const auto [dictPalette, idxPalette] = VQGenerateDictFast<float, 3, uint16_t>
                (blocksPalette, 256, 1000);
    const auto imgPalette = BlockImage<1, 1>(dictPalette, idxPalette, 
                imageBlocks.nBlocksX, imageBlocks.nBlocksY);

    const auto blocksDiff = BlockRGBSubtractMean<float, 3 * blockW * blockH>(imageBlocks.data, imgPalette.data);
    const auto [dictDiff, idxDiff] = VQGenerateDictFast<float, 3 * blockW * blockH, uint16_t>(blocksDiff, 256, 1000);
    auto imgDiff = BlockImage<blockW, blockH>(dictDiff, idxDiff, 
                imageBlocks.nBlocksX, imageBlocks.nBlocksY);

    imgDiff.data = BlockRGBAddMean<float, 3 * blockW * blockH>(imgDiff.data, imgPalette.data);
    const auto decodedImage = imgDiff.toRGB8Image();

    printf("Writing to image %s...", args[2]);
    const auto saveImagePath = args[2];
    Support::SavePNG(saveImagePath, decodedImage);

    return 0;
}
