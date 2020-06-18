#pragma once

#include "IV1BlockImage.h"

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

struct IV1File {
    IV1FileHeader header;
    FlexMatrix<float, 3> dict0;
    std::vector<uint16_t> indices0;
    FlexMatrix<float, 48> dict1;
    std::vector<uint16_t> indices1;

    IV1File(const char* path) {
        auto file = fopen(path, "rb");
        fread(&header, 1, sizeof(IV1FileHeader), file);

        constexpr float inv255 = 1.0f/255.0f;
        const size_t numBlocks = header.nBlocksX * header.nBlocksY;
        
        dict0.resize(256);
        // Load, then expand dict0 to float
        for (size_t idx = 0; idx != 256; ++idx) {
            MatrixRow<uint8_t, 3> block8bit;
            fread(block8bit.data(), 3, 1, file);
            for (auto elem = 0; elem != 3; ++elem) {
                dict0[idx][elem] = (block8bit[elem] * inv255) - 1.0f/510.f;
            }
        }

        { // Load, then expand indices0 to uint16
            std::vector<uint8_t> indices8bit(numBlocks);
            fread(indices8bit.data(), indices8bit.size(), 1, file);

            indices0.resize(numBlocks);
            std::transform(indices8bit.begin(), indices8bit.end(), indices0.begin(),
                [](uint8_t in) { return (uint16_t) in; });
        }

        dict1.resize(256);
        // Load, then expand dict1 to float
        for (const auto& block : dict1) {
            MatrixRow<uint8_t, 48> block8bit;
            for (auto elem = 0; elem != 48; ++elem) {
                block8bit[elem] = (block8bit[elem] * inv255 / 2.0f) - 256.0f/510.f;
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
};