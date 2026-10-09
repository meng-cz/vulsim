#pragma once
#include "module.h"

// Concrete endpoints are shared by validation, scheduling and both generators.
void materializeConcreteConnections(const shared_ptr<VulStaticModuleInstance> &root);
VulStaticModuleInstance concreteGenerationView(const VulStaticModuleInstance &module);
