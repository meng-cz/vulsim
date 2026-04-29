
#pragma once

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <iostream>

void sim_execute();

void sim_commit();

void sim_reset();

#define SIMULATION() void sim_main()

#define VAR(type, name) type name;

#define VAR_INIT(type, name, init) type name = init;

#define GLOBAL() inline namespace global
