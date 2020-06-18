#pragma once

#include "VQLib/C++/VQDataTypes.h"
#include "Support/PNGLoader.h"

#include "ConstexprSqrt.h"

#include <cassert>

namespace IV1 {

template<size_t blockW, size_t blockH>
struct BlockImage {
    const size_t nBlocksX, nBlocksY;
    const size_t actualW, actualH;
    static constexpr size_t channels = 3;
    FlexMatrix<float, blockW * blockH * channels> data;

    BlockImage(VQLib::Support::RGB8Image image)
    : nBlocksX((image.width + blockW - 1) / blockW)
    , nBlocksY((image.height + blockH - 1) / blockH)
    , actualW(image.width)
    , actualH(image.height) {

        // If the image's dimensions aren't a multiple of the block
        //  dimensions, we have to pad the image. I'm choosing a
        //  mirrored-repeat strategy here, to minimize discontinuities.
        if (actualW % blockW != 0 || actualH % blockH != 0) {
            VQLib::Support::RGB8Image newImage;
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

    VQLib::Support::RGB8Image toRGB8Image() const {
        VQLib::Support::RGB8Image image;
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

        VQLib::Support::RGB8Image croppedImage;
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


} // namespace IV1