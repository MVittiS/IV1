#pragma once

#include "RGB8Image.h"

namespace VQLib::Support {

using Path = const char *;

RGB8Image LoadPNG(Path);
void SavePNG(Path, const RGB8Image&);

}
