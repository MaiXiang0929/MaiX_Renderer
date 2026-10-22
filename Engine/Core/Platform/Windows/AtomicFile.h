// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <string>
// Writes a sibling temporary file and replaces the destination only after flush.
bool WriteTextFileAtomically(const std::filesystem::path&, const std::string&, std::string& error, bool overwrite = true);
