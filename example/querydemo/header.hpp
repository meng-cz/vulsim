#pragma once

#include <defhelper.hpp>

STRUCT(ChildSnapshot) {
    uint32_t sum;
    bool can_pop;
};

STRUCT(TopSnapshot) {
    ChildSnapshot child;
    uint32_t mirror;
    bool mirror_even;
};
