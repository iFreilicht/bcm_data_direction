#pragma once

#include <stdint.h>
#include <stdlib.h>
#include "Arduino.h"

#include <vector>

#include "cue.h"
#include "schedule.h"

namespace freilite{
namespace iris{
namespace storage{
    std::vector<Cue> loaded_cues;

    std::vector<delay_t> loaded_schedules;
}
}
}