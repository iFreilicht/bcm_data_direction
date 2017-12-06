//All functionality regarding colour calculations
#pragma once

#include <stdint.h>

namespace pb{
    #include <pb_encode.h>
    #include <pb_decode.h>
    #include "iris.pb.h"
}

namespace freilite{
//Struct for storing colors as simple RGB values
struct Color{
    uint8_t R;
    uint8_t G;
    uint8_t B;
};

// This is not a member function because the color class will be swapped
// out at some point
pb::Cue_Color color_to_pb_color(Color color){
    using namespace pb;

    Cue_Color pb_color = Cue_Color_init_default;
    pb_color.red = color.R;
    pb_color.green = color.G;
    pb_color.blue = color.B;

    return pb_color;
}

}