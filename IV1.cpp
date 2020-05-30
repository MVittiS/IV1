// Implementation of IV1 (Image-VQ 1, or "Ivy-One") codec in C++, based off of MatLAB source.

#define VQLIB_VERBOSE_OUTPUT 1

#include "VQLib/C++/VQAlgorithm.h"
#include "Support/PNGLoader.h"
#include "ConstexprSqrt.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <type_traits>

using namespace VQLib;

template<size_t blockW, size_t blockH>
struct BlockImage {
    const size_t nBlocksX, nBlocksY;
    const size_t actualW, actualH;
    static constexpr size_t channels = 3;
    FlexMatrix<float, blockW * blockH * channels> data;

    BlockImage(Support::RGB8Image image)
    : nBlocksX((image.width + blockW - 1) / blockW)
    , nBlocksY((image.height + blockH - 1) / blockH)
    , actualW(image.width)
    , actualH(image.height) {

        // If the image's dimensions aren't a multiple of the block
        //  dimensions, we have to pad the image. I'm choosing a
        //  mirrored-repeat strategy here, to minimize discontinuities.
        if (actualW % blockW != 0 || actualH % blockH != 0) {
            Support::RGB8Image newImage;
            newImage.width = nBlocksX * blockW;
            newImage.height = image.height;
            newImage.pixels.resize(newImage.width * newImage.height * channels);

            const auto columnLeftover = newImage.width - image.width;
            const auto rowLeftover = newImage.height - image.height;

            const auto rowStride = image.width * channels;
            const auto newRowStride = newImage.width * channels;

            for (auto row = 0; row != image.height; ++row) {
                std::copy_n(&image.pixels[row * rowStride], 
                    rowStride, &newImage.pixels[row * newRowStride]);
                for (auto column = 0; column != columnLeftover; ++column) {
                    newImage.pixels[row * newRowStride + rowStride + channels * column + 0] = 
                        image.pixels[(row + 1) * rowStride - (channels * (1 + column)) + 0];
                    newImage.pixels[row * newRowStride + rowStride + channels * column + 1] = 
                        image.pixels[(row + 1) * rowStride - (channels * (1 + column)) + 1];
                    newImage.pixels[row * newRowStride + rowStride + channels * column + 2] = 
                        image.pixels[(row + 1) * rowStride - (channels * (1 + column)) + 2];
                }
            }

            for (auto row = 0; row != rowLeftover; ++row) {
                std::copy_n(&newImage.pixels[(image.width - row - 1) * rowStride],
                    rowStride, &newImage.pixels[(image.width + row) * rowStride]);
            }

            image = newImage;
        }

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

        // Finally, YUV scaling.
        constexpr float weightR = constSqrt(0.2125f);
        constexpr float weightG = constSqrt(0.7154f);
        constexpr float weightB = constSqrt(0.0721f);
        for (auto& sample : data) {
            for (size_t triad = 0; triad < sample.size() / 3; ++triad) {
                sample[3 * triad + 0] *= weightR;
                sample[3 * triad + 1] *= weightG;
                sample[3 * triad + 2] *= weightB;
            }
        }
    }

    template<typename Index>
    BlockImage(const FlexMatrix<float, blockW * blockH * channels>& dict,
        const std::vector<Index>& indices, size_t nBlocksX, size_t nBlocksY)
    : nBlocksX(nBlocksX)
    , nBlocksY(nBlocksY)
    , actualW(nBlocksX * blockW)
    , actualH(nBlocksY * blockH) {
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
                        constexpr float yuvWeights[3] = {
                            1.0f / constSqrt(0.2125f),
                            1.0f / constSqrt(0.7154f),
                            1.0f / constSqrt(0.0721f)
                        };
                        for (size_t ch = 0; ch != channels; ++ch) {
                            image.pixels[((blockY * blockH + y) * image.width +
                                (blockX * blockW) + x) * 3 + ch] = std::round(std::clamp(
                                    block[(y * blockW + x) * 3 + ch] * yuvWeights[ch],   
                                    0.f, 255.f));
                        }
                    }
                }
            }
        }

        if (actualW % blockW == 0 && actualH % blockH == 0) {
            return image;
        }

        Support::RGB8Image croppedImage;
        croppedImage.width = actualW;
        croppedImage.height = actualH;
        croppedImage.pixels.resize(actualW * actualH * 3);

        const auto rowStride = image.width * channels;
        const auto croppedStride = actualW * channels;

        for (auto row = 0; row != actualH; ++row) {
            std::copy_n(&image.pixels[rowStride * row], croppedStride,
                &croppedImage.pixels[croppedStride * row]);
        }

        return croppedImage;
    }
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

struct IV1FileHeader {
    const uint8_t magic[4] = {'I', 'V', 'Y', '1'};
    uint16_t nBlocksX, nBlocksY;
    uint32_t actualW, actualH;
};

void save(const char* path,
          const FlexMatrix<float, 3>& dict0,
          const std::vector<uint16_t>& indices0,
          const FlexMatrix<float, 48>& dict1,
          const std::vector<uint16_t>& indices1,
          size_t nBlocksX, size_t nBlocksY,
          size_t imageW, size_t imageH) {

    FILE* file = fopen(path, "wb");
    IV1FileHeader header;

    header.nBlocksX = nBlocksX;
    header.nBlocksY = nBlocksY;
    header.actualW = imageW;
    header.actualH = imageH;

    fwrite(&header, 1, sizeof(header), file);

    // Reduce dict0 to uint8, then save
    for (const auto& block : dict0) {
        MatrixRow<uint8_t, 3> block8bit;
        for (auto elem = 0; elem != 3; ++elem) {
            block8bit[elem] = 255.0f * (block[elem] + 1.0f/510.f);
        }
        fwrite(block8bit.data(), 3, 1, file);
    }

    { // Reduce indices0 to uint8, then save
        std::vector<uint8_t> indices8bit(indices0.size());
        std::transform(indices0.begin(), indices0.end(), indices8bit.begin(),
            [](uint16_t in) { return (uint8_t) in; });
        
        fwrite(indices8bit.data(), indices8bit.size(), 1, file);
    }

    // Reduce dict1 to uint8, then save
    for (const auto& block : dict1) {
        MatrixRow<uint8_t, 48> block8bit;
        for (auto elem = 0; elem != 48; ++elem) {
            block8bit[elem] = 255.0f * (block[elem] + 1.0f/510.f);
        }
        fwrite(block8bit.data(), 48, 1, file);
    }

    { // Reduce indices1 to uint8, then save
        std::vector<uint8_t> indices8bit(indices1.size());
        std::transform(indices1.begin(), indices1.end(), indices8bit.begin(),
            [](uint16_t in) { return (uint8_t) in; });
        
        fwrite(indices8bit.data(), indices8bit.size(), 1, file);
    }

    fclose(file);
}    


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
