#pragma once

#include <vector>

namespace VQLib::Support {

struct RGB8Image {
    size_t width, height;
    std::vector<uint8_t> pixels;
};

using Path = const char *;

RGB8Image LoadPNG(Path);
void SavePNG(Path, const RGB8Image&);



}
