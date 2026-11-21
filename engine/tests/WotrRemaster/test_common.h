// OpenBFME. GPL-3.0.
// Common helpers for the WotrRemaster pure tests.

#pragma once

#include <string>

namespace WotrRemasterTest {

// Directory holding campaigns/fixtures/wotr_1_1 (passed via -DWOTR_FIXTURE_DIR).
std::string fixtureDir();

// Read a file whole.
std::string readFile(const std::string& path);

} // namespace WotrRemasterTest
