# OpenBFME. GPL-3.0.
# WOTR-1.1-K4 (Godot World Engineer): the Godot extension that binds the
# WotrRemaster deterministic strategic core to Godot.
#
# The strategic core is compiled from its canonical location
# (engine/src/GameLogic/WotrRemaster, card WOTR-1.1-K2) and is NOT modified
# here: this project only adds the Godot-facing adapter.
#
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --target wotr_core
#
# The result lands in bin/ next to this file, which the .gdextension manifest
# loads. `build/` and `bin/` are repository-ignored build outputs.

cmake_minimum_required(VERSION 3.17)
project(wotr_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

get_filename_component(WOTR_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(WOTR_CORE_DIR "${WOTR_REPO_ROOT}/engine/src/GameLogic/WotrRemaster")
set(WOTR_GODOTCPP_DIR "${WOTR_REPO_ROOT}/engine/thirdparty/godot-cpp")
set(WOTR_GODOTCPP_BUILD "${WOTR_REPO_ROOT}/engine/build/thirdparty/godot-cpp")

if(NOT EXISTS "${WOTR_CORE_DIR}/State.h")
    message(FATAL_ERROR "the WotrRemaster core is missing at ${WOTR_CORE_DIR}")
endif()
if(NOT EXISTS "${WOTR_GODOTCPP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "godot-cpp is not populated; run tools/arena/build_engine.sh once")
endif()

# ---- the canonical strategic core (Strategic Core Engineer, K2) ----------------
# Compiled unchanged into a static library of its own: the core must keep its
# own exception settings and must not depend on godot-cpp.
add_library(wotr_remaster_core STATIC
    "${WOTR_CORE_DIR}/Errors.cpp"
    "${WOTR_CORE_DIR}/Hash.cpp"
    "${WOTR_CORE_DIR}/Json.cpp"
    "${WOTR_CORE_DIR}/Pack.cpp"
    "${WOTR_CORE_DIR}/Save.cpp"
    "${WOTR_CORE_DIR}/State.cpp")
target_include_directories(wotr_remaster_core PUBLIC "${WOTR_CORE_DIR}")
target_compile_features(wotr_remaster_core PUBLIC cxx_std_17)

# ---- godot-cpp -----------------------------------------------------------------
# The engine build already produced godot-cpp; reusing that exact library keeps
# the binding's headers and ABI identical to the openbfme extension's. When it
# is missing, godot-cpp is built from source for this target alone.
file(GLOB WOTR_GODOTCPP_LIBRARY
    "${WOTR_REPO_ROOT}/engine/build/bin/libgodot-cpp*.a"
    "${WOTR_GODOTCPP_BUILD}/bin/libgodot-cpp*.a"
    "${WOTR_GODOTCPP_BUILD}/libgodot-cpp*.a")
if(WOTR_GODOTCPP_LIBRARY)
    list(GET WOTR_GODOTCPP_LIBRARY 0 WOTR_GODOTCPP_LIBRARY_PATH)
    add_library(wotr_godot_cpp STATIC IMPORTED)
    set_target_properties(wotr_godot_cpp PROPERTIES
        IMPORTED_LOCATION "${WOTR_GODOTCPP_LIBRARY_PATH}"
        INTERFACE_INCLUDE_DIRECTORIES "${WOTR_GODOTCPP_DIR}/include;${WOTR_GODOTCPP_BUILD}/gen/include")
    set(WOTR_GODOTCPP_TARGET wotr_godot_cpp)
    message(STATUS "wotr_core: reusing ${WOTR_GODOTCPP_LIBRARY_PATH}")
else()
    set(GODOTCPP_API_VERSION "4.7" CACHE STRING "Godot API version for godot-cpp bindings")
    add_subdirectory("${WOTR_GODOTCPP_DIR}" godot-cpp-build EXCLUDE_FROM_ALL)
    set(WOTR_GODOTCPP_TARGET godot-cpp)
    message(STATUS "wotr_core: building godot-cpp from source")
endif()

# ---- the native tactical bridge -------------------------------------------------
# The bridge runs OpenBFME's own War of the Ring battle (the engine's
# LivingWorld::AutoResolveBattle over the mounted retail archives), so the
# extension links the engine's static core.  The engine's libraries are the
# ones tools/arena/build_engine.sh produced; their compile flags (the floating
# point ones in particular) are the ones the engine used, so the bridge runs
# the same code the engine's own extension does.
set(WOTR_ENGINE_BUILD "${WOTR_REPO_ROOT}/engine/build")
set(WOTR_ENGINE_LIBRARIES
    "${WOTR_ENGINE_BUILD}/libopenbfme_core.a"
    "${WOTR_ENGINE_BUILD}/libopenbfme_detlibm.a"
    "${WOTR_ENGINE_BUILD}/libopenbfme_lua.a"
    "${WOTR_ENGINE_BUILD}/libopenbfme_luau.a"
    "${WOTR_ENGINE_BUILD}/libopenbfme_float_chars.a")
foreach(wotr_engine_library ${WOTR_ENGINE_LIBRARIES})
    if(NOT EXISTS "${wotr_engine_library}")
        message(FATAL_ERROR
            "the native tactical bridge needs the engine build: ${wotr_engine_library} is missing "
            "(run tools/arena/build_engine.sh --core)")
    endif()
endforeach()

find_package(Threads REQUIRED)

# ---- the extension --------------------------------------------------------------
add_library(wotr_core SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/wotr_core_native.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/wotr_battle_bridge_native.cpp")
target_include_directories(wotr_core PRIVATE "${WOTR_CORE_DIR}")
target_include_directories(wotr_core PRIVATE
    "${WOTR_REPO_ROOT}/engine/src"
    "${WOTR_REPO_ROOT}/engine/thirdparty"
    "${WOTR_ENGINE_BUILD}/lua-ea/include"
    "${WOTR_ENGINE_BUILD}/luau-openbfme/public")
target_link_libraries(wotr_core PRIVATE wotr_remaster_core ${WOTR_GODOTCPP_TARGET})
target_link_libraries(wotr_core PRIVATE ${WOTR_ENGINE_LIBRARIES} Threads::Threads ${CMAKE_DL_LIBS})
# the engine's floating point flags: the bridge's arithmetic is the engine's
# arithmetic, so it must be compiled the same way (no fast math, no contraction)
target_compile_options(wotr_core PRIVATE
    -fno-fast-math -ffp-contract=off -frounding-math -fno-math-errno)
target_compile_definitions(wotr_core PRIVATE
    GDEXTENSION
    DEBUG_ENABLED
    LINUX_ENABLED
    THREADS_ENABLED
    UNIX_ENABLED)
if(APPLE)
    target_compile_definitions(wotr_core PRIVATE MACOS_ENABLED)
endif()
if(WIN32)
    target_compile_definitions(wotr_core PRIVATE WINDOWS_ENABLED)
endif()

set_target_properties(wotr_core PROPERTIES
    OUTPUT_NAME "wotr_core"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
