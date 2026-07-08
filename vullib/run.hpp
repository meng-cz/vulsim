
#pragma once

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <iostream>

void sim_nextcycle();

void sim_reset();

void sim_exit();

#define SIMULATION() void sim_main()

#define GLOBAL() inline namespace global

#define TOP(relapath)

#define PROJECT(relapath)
