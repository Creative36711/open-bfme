// OpenBFME. GPL-3.0.
// WotrRemaster deterministic strategic core: pure unit tests.
// This translation unit provides doctest's main. All other test files only
// include <doctest.h> and test_common.h.
//
// Focused standalone build (no full engine CMake graph):
//   g++ -std=c++17 -O2 -Wall -Wextra -I engine/src -I engine/thirdparty/doctest
//       -DWOTR_FIXTURE_DIR=<absolute fixture dir>
//       engine/src/GameLogic/WotrRemaster/*.cpp engine/tests/WotrRemaster/*.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <fstream>
#include <string>

#include "test_common.h"

#include "GameLogic/WotrRemaster/Pack.h"

namespace WotrRemasterTest {

std::string fixtureDir() {
#ifdef WOTR_FIXTURE_DIR
#define WOTR_STR_INNER(x) #x
#define WOTR_STR(x) WOTR_STR_INNER(x)
    return WOTR_STR(WOTR_FIXTURE_DIR);
#undef WOTR_STR
#undef WOTR_STR_INNER
#else
    return "./campaigns/fixtures/wotr_1_1";
#endif
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    INFO("cannot open fixture: " << path);
    REQUIRE(f.is_open());
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

} // namespace WotrRemasterTest
