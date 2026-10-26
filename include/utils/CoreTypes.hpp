#pragma once
#include "utils/json.hpp"
#include <chrono>

// Shared Core types. Entrypoints include this explicitly instead of relying on
// aliases leaking from one particular component's header.
using json = nlohmann::json;
using Clock = std::chrono::steady_clock;
