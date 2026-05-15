
#pragma once

#include <defhelper.hpp>

REQUEST(output, ARG(uint32_t) data);

CHILD_INSTANCE(FIFO, fifo);
CHILD_INSTANCE(Reader, reader);

CONNECT_CR_R(reader, output, output);
CONNECT_CR_CS(reader, deq, fifo, deq);
