#include "Fill.hpp"

#include <iomanip>
#include "hardware/dma.h"
#include "arm_acle.h"

// méret konstansok miatt
#include "textures.hpp"

std::string Fill::toString() {
    std::stringstream s;
    if (isInterpolated)
        s << "InterpolatedTextureFill ";
    else
        s << "PerspectiveTextureFill ";
    s << std::hex << texture;
    return s.str();
}
