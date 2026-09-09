#include <defhelper.hpp>
#include <run.hpp>

#include "header.hpp"

TOP("Top.hpp");
PROJECT(".");

QUERY(register_file, RegisterFileSnapshot);

SIMULATION() {
    RegisterFileSnapshot snapshot = register_file();
    (void)snapshot;
}
