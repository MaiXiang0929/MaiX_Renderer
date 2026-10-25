// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <cstdint>

// Persistent 128-bit content keys are unrelated to runtime ActorId/resource handles.
bool IsContentGuid(const std::string& value);
std::string NewContentGuid();
