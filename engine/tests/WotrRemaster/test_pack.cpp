// OpenBFME. GPL-3.0.
// WotrRemaster deterministic strategic core: pack schema tests.

#include <doctest.h>

#include "test_common.h"

#include "GameLogic/WotrRemaster/Hash.h"
#include "GameLogic/WotrRemaster/Pack.h"
#include "GameLogic/WotrRemaster/State.h"

using namespace WotrRemaster;
using namespace WotrRemasterTest;

namespace {

Pack loadValidPack() {
    auto res = loadPack(readFile(fixtureDir() + "/pack.json"));
    INFO(res.message);
    REQUIRE(res.ok());
    return res.value;
}

JsonValue rawPack() {
    return JsonValue::parse(readFile(fixtureDir() + "/pack.json"));
}

} // namespace

TEST_CASE("pack schema accepts the valid tiny pack") {
    Pack pack = loadValidPack();
    CHECK(pack.manifest.id == "wotr_1_1_tiny");
    CHECK(pack.manifest.version == 1);
    CHECK(pack.manifest.tacticalProfile == "vanilla_rotwk_201");
    CHECK(pack.manifest.tacticalMap == "helms_deep");
    REQUIRE(pack.manifest.contentHash.has_value());
    CHECK(*pack.manifest.contentHash == contentHashOf(pack));

    CHECK(pack.regions.size() == 3);
    CHECK(pack.factions.size() == 2);
    CHECK(pack.armies.size() == 2);

    CHECK(pack.regions[0].id == "west");
    CHECK(pack.regions[1].id == "crossing");
    CHECK(pack.regions[2].id == "east");
    CHECK(pack.armies[0].regionId == "west");
    CHECK(pack.armies[1].regionId == "east");
}

TEST_CASE("pack: duplicate ids are rejected") {
    JsonValue root = rawPack();
    JsonValue& regionsNode = root.objectValue.at("regions");
    regionsNode.arrayValue[1].objectValue.at("id").stringValue = "west";
    auto res = loadPack(root.canonical());
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::DuplicateId);
    CHECK(res.message.find("region") != std::string::npos);
}

TEST_CASE("pack: broken topology (unknown region reference) is rejected") {
    JsonValue root = rawPack();
    JsonValue& region0 = root.objectValue.at("regions").arrayValue[0];
    region0.objectValue.at("connections").arrayValue[0].stringValue = "nowhere";
    auto res = loadPack(root.canonical());
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::BrokenTopology);
}

TEST_CASE("pack: broken topology (asymmetric link) is rejected") {
    JsonValue root = rawPack();
    JsonValue& crossing = root.objectValue.at("regions").arrayValue[1];
    // crossing -> west link removed: west->crossing has no reverse link
    crossing.objectValue.at("connections").arrayValue.erase(
        crossing.objectValue.at("connections").arrayValue.begin());
    auto res = loadPack(root.canonical());
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::BrokenTopology);
}

TEST_CASE("pack: missing tactical profile is rejected") {
    JsonValue root = rawPack();
    root.objectValue.at("pack").objectValue.erase("tactical_profile");
    auto res = loadPack(root.canonical());
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::MissingTacticalProfile);
}

TEST_CASE("pack: unsupported tactical profile is rejected") {
    JsonValue root = rawPack();
    root.objectValue.at("pack").objectValue.at("tactical_profile").stringValue =
        "some_other_profile";
    auto res = loadPack(root.canonical());
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::MissingTacticalProfile);
}

TEST_CASE("pack: missing tactical map is rejected") {
    JsonValue root = rawPack();
    root.objectValue.at("pack").objectValue.erase("tactical_map");
    auto res = loadPack(root.canonical());
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::MissingTacticalMap);
}

TEST_CASE("pack: empty tactical map is rejected") {
    JsonValue root = rawPack();
    root.objectValue.at("pack").objectValue.at("tactical_map").stringValue = "";
    auto res = loadPack(root.canonical());
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::MissingTacticalMap);
}

TEST_CASE("pack: absolute reference is rejected") {
    JsonValue root = rawPack();
    root.objectValue.at("pack").objectValue.at("tactical_map").stringValue =
        "/home/author/maps/helms_deep";
    auto res = loadPack(root.canonical());
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::AbsoluteReference);
}

TEST_CASE("pack: windows-style absolute reference is rejected") {
    JsonValue root = rawPack();
    root.objectValue.at("pack").objectValue.at("tactical_map").stringValue =
        "C:\\Maps\\helms_deep";
    auto res = loadPack(root.canonical());
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::AbsoluteReference);
}

TEST_CASE("pack: unstable reference (.. segment) is rejected") {
    JsonValue root = rawPack();
    root.objectValue.at("pack").objectValue.at("tactical_map").stringValue =
        "../maps/helms_deep";
    auto res = loadPack(root.canonical());
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::UnstableReference);
}

TEST_CASE("pack: stable explicit references are accepted") {
    const char* refs[] = {"pack:maps/helms_deep", "retail:maps/helms_deep",
                          "custom:maps/helms_deep", "helms_deep"};
    for (const char* ref : refs) {
        JsonValue root = rawPack();
        root.objectValue.at("pack").objectValue.at("tactical_map").stringValue = ref;
        // The manifest content hash covers the old map reference; dropping it
        // lets the loader validate the reference without a hash mismatch.
        root.objectValue.at("pack").objectValue.erase("contentHash");
        auto res = loadPack(root.canonical());
        INFO("reference: " << ref);
        REQUIRE(res.ok());
    }
}

TEST_CASE("pack: content hash mismatch is rejected") {
    JsonValue root = rawPack();
    root.objectValue.at("pack").objectValue.at("contentHash").stringValue =
        std::string(64, '0');
    auto res = loadPack(root.canonical());
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::ContentHashMismatch);
}

TEST_CASE("pack: reordered source properties do not change the pack hash") {
    JsonValue root = rawPack();
    PackHash first = contentHashOf(loadValidPack());

    // Same pack, properties written in a different order at every level.
    const char* reordered = R"(
{
  "armies": [
    {"members": [{"persistentUpgrades": [], "experience": 0, "quantity": 12,
      "kind": "wotr.unit.orc", "id": "member_mordor_1"}],
     "id": "army_mordor", "factionId": "faction_mordor", "regionId": "east"},
    {"members": [{"persistentUpgrades": [], "experience": 0, "quantity": 12,
      "kind": "wotr.unit.knight", "id": "member_gondor_1"}],
     "id": "army_gondor", "factionId": "faction_gondor", "regionId": "west"}
  ],
  "factions": [
    {"name": "wotr.faction.mordor", "id": "faction_mordor"},
    {"name": "wotr.faction.gondor", "id": "faction_gondor"}
  ],
  "pack": {
    "tactical_map": "helms_deep",
    "version": 1,
    "tactical_profile": "vanilla_rotwk_201",
    "id": "wotr_1_1_tiny"
  },
  "regions": [
    {"connections": ["crossing"], "name": "wotr.region.west", "id": "west"},
    {"connections": ["east", "west"], "name": "wotr.region.crossing", "id": "crossing"},
    {"connections": ["crossing"], "name": "wotr.region.east", "id": "east"}
  ]
}
)";
    JsonValue reorderedRoot = JsonValue::parse(reordered);
    auto res = loadPack(reorderedRoot.canonical());
    INFO(res.message);
    REQUIRE(res.ok());
    CHECK(contentHashOf(res.value) == first);

    // And the initial state built from it hashes identically.
    auto s1 = initialState(loadValidPack(), 987654321);
    auto s2 = initialState(res.value, 987654321);
    REQUIRE(s1.ok());
    REQUIRE(s2.ok());
    CHECK(stateHash(s1.value) == stateHash(s2.value));
    CHECK(canonicalState(s1.value) == canonicalState(s2.value));
}

TEST_CASE("sha-256 test vectors") {
    CHECK(Sha256::hashHex("") ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(Sha256::hashHex("abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
