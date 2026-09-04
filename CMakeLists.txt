cmake_minimum_required(VERSION 3.20)
project(GoldenSunRecompStarter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Link-time (interprocedural) optimization for GoldenSunRecomp + the
# gbarecomp static libraries it links. Off for gsr_bootstrap and every
# third_party/* target (imgui, toml++, gba_suite, mgba) — those keep whatever
# flags they already had. Degrades gracefully: if the toolchain can't do IPO
# (checked below, not assumed), the build proceeds without it instead of
# failing. Override with -DGSR_ENABLE_LTO=OFF.
option(GSR_ENABLE_LTO
    "Enable link-time optimization for GoldenSunRecomp and gbarecomp targets"
    ON)
set(GSR_IPO_SUPPORTED FALSE)
if(GSR_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT GSR_IPO_SUPPORTED OUTPUT GSR_IPO_SUPPORT_ERROR)
    if(NOT GSR_IPO_SUPPORTED)
        message(STATUS
            "GSR_ENABLE_LTO is ON but this toolchain reports IPO/LTO "
            "unsupported (${GSR_IPO_SUPPORT_ERROR}); building without it.")
    endif()
endif()
# Parallel LTRANS (post-analysis codegen) job count for GCC's -flto=N on the
# final GoldenSunRecomp link. This does NOT bound the whole-program-analysis
# (WPA) pass that precedes it — WPA is single-threaded in GCC and is the
# actual peak-memory phase for a binary this size (~168 MB, ~400k alias
# thunks), so raising this number buys wall-clock time on the codegen phase
# only, not memory headroom. Left at a conservative fraction of logical
# cores rather than the full core count so LTRANS workers don't compound
# against that WPA peak on a 16 GB box. Set to "" to omit the explicit -flto=N
# override, or to 1 for a fully serial (lowest-memory) LTRANS. See the LTO
# section in the delivery report for the reasoning and how to size this up.
set(GSR_LTO_JOBS "8" CACHE STRING
    "Parallel LTRANS job count (-flto=N) for the GoldenSunRecomp link when LTO is enabled; empty = compiler default")

# This starter target proves the public repository builds without a ROM/BIOS.
# The real runner is intentionally not wired until GS-009, after upstream APIs,
# symbols, overlays, and hashes are pinned and reviewed.
add_executable(gsr_bootstrap src/bootstrap_main.cpp)

target_compile_definitions(gsr_bootstrap PRIVATE
    GSR_EXPECTED_ROM_SHA1="5c4695205413df7db52b9a184815a07783999971"
)

if(MSVC)
    target_compile_options(gsr_bootstrap PRIVATE /W4 /permissive-)
else()
    target_compile_options(gsr_bootstrap PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME bootstrap_help COMMAND gsr_bootstrap --help)

add_executable(gsr_blitter_shadow_observer_test
    tests/cpp/blitter_shadow_observer_test.cpp)
target_include_directories(gsr_blitter_shadow_observer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src)
add_test(NAME blitter_shadow_observer_test
         COMMAND gsr_blitter_shadow_observer_test)

add_executable(gsr_launcher_audio_policy_test
    tests/cpp/launcher_audio_policy_test.cpp)
target_include_directories(gsr_launcher_audio_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src)
add_test(NAME launcher_audio_policy_test
         COMMAND gsr_launcher_audio_policy_test)

add_executable(gsr_launcher_test_policy_test
    tests/cpp/launcher_test_policy_test.cpp)
target_include_directories(gsr_launcher_test_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src)
add_test(NAME launcher_test_policy_test
         COMMAND gsr_launcher_test_policy_test)

add_executable(gsr_launcher_widescreen_diagnostics_policy_test
    tests/cpp/launcher_widescreen_diagnostics_policy_test.cpp)
target_include_directories(gsr_launcher_widescreen_diagnostics_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src)
add_test(NAME launcher_widescreen_diagnostics_policy_test
         COMMAND gsr_launcher_widescreen_diagnostics_policy_test)

add_executable(gsr_relocatable_writer_policy_test
    tests/cpp/relocatable_writer_policy_test.cpp)
target_include_directories(gsr_relocatable_writer_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src)
add_test(NAME relocatable_writer_policy_test
         COMMAND gsr_relocatable_writer_policy_test)

add_executable(gsr_launcher_session_id_test
    tests/cpp/launcher_session_id_test.cpp)
target_include_directories(gsr_launcher_session_id_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src)
add_test(NAME launcher_session_id_test
         COMMAND gsr_launcher_session_id_test)

add_executable(gsr_player_speed_cheat_test
    tests/cpp/player_speed_cheat_test.cpp)
target_include_directories(gsr_player_speed_cheat_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/gbarecomp/src/runtime)
add_test(NAME player_speed_cheat_test
         COMMAND gsr_player_speed_cheat_test)

add_executable(gsr_widescreen_policy_test
    tests/cpp/widescreen_policy_test.cpp)
target_include_directories(gsr_widescreen_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/gbarecomp/src/runtime)
add_test(NAME widescreen_policy_test
         COMMAND gsr_widescreen_policy_test)

add_executable(gsr_widescreen_literal_trace_test
    tests/cpp/widescreen_literal_trace_test.cpp)
target_include_directories(gsr_widescreen_literal_trace_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src)
add_test(NAME widescreen_literal_trace_test
         COMMAND gsr_widescreen_literal_trace_test)

add_executable(gsr_launcher_replay_policy_test
    tests/cpp/launcher_replay_policy_test.cpp)
target_include_directories(gsr_launcher_replay_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src)
add_test(NAME launcher_replay_policy_test
         COMMAND gsr_launcher_replay_policy_test)

find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
add_test(
    NAME symbol_corpus_validation
    COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_SOURCE_DIR}/tests
            -p test_*.py
)
set_tests_properties(symbol_corpus_validation PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME public_repository_audit
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/audit_public_repo.py
)

# GS-009 local runner. Generated translation units are derivative of the user's
# ROM and therefore stay under the ignored local/ boundary. Public/CI builds
# leave this option off and continue to build only the asset-free bootstrap.
option(GSR_BUILD_LOCAL_RUNNER
    "Build the private, generated-code Golden Sun runner"
    OFF)
set(GBARECOMP_ROOT "${CMAKE_SOURCE_DIR}/gbarecomp"
    CACHE PATH "Path to the pinned gbarecomp checkout")
set(GSR_GENERATED_DIR "${CMAKE_SOURCE_DIR}/local/gs009/main"
    CACHE PATH "Private gba_recompile output directory")
set(GSR_TRANSIENT_FUNC2D5C_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func2d5c"
    CACHE PATH "Private identity-prefixed Func_2d5c AOT variant output")
set(GSR_TRANSIENT_FUNCA37C_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_funca37c"
    CACHE PATH "Private identity-prefixed Func_a37c AOT variant output")
set(GSR_TRANSIENT_FUNC2808_03006000_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func2808_03006000"
    CACHE PATH "Private identity-prefixed Func_2808 AOT output at 0x03006000")
set(GSR_TRANSIENT_FUNC6ABC_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func6abc_pic"
    CACHE PATH "Private position-independent Func_6abc stack-thunk AOT output")
set(GSR_TRANSIENT_FUNC15E10_03003A84_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func15e10_03003a84"
    CACHE PATH "Private identity-prefixed Func_15e10 AOT output at 0x03003a84")
set(GSR_TRANSIENT_FUNCA418_03003400_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_funca418_03003400"
    CACHE PATH "Private identity-prefixed Func_a418 AOT output at 0x03003400")
set(GSR_TRANSIENT_FUNC15AFC_03003A84_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func15afc_03003a84"
    CACHE PATH "Private identity-prefixed Func_15afc AOT output at 0x03003a84")
# ONE position-independent corpus replaces the four per-base Func_2cf4 ones.
set(GSR_TRANSIENT_FUNC1B70_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func1b70_pic"
    CACHE PATH "Private position-independent Func_1b70 RAM-image AOT output")
set(GSR_TRANSIENT_FUNC9BB8_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func9bb8_pic"
    CACHE PATH "Private position-independent Func_9bb8 RAM-image AOT output")
set(GSR_TRANSIENT_FUNC15430_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func15430_pic"
    CACHE PATH "Private position-independent Func_15430 RAM-image AOT output")
set(GSR_TRANSIENT_FUNC15570_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func15570_pic"
    CACHE PATH "Private position-independent Func_15570 RAM-image AOT output")
set(GSR_TRANSIENT_FUNC158E8_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func158e8_pic"
    CACHE PATH "Private position-independent Func_158e8 RAM-image AOT output")
set(GSR_TRANSIENT_FUNC2544_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func2544_pic"
    CACHE PATH "Private position-independent Func_2544 RAM-image AOT output")
set(GSR_TRANSIENT_FUNC2CF4_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func2cf4_pic"
    CACHE PATH "Private position-independent Func_2cf4 RAM-image AOT output")
set(GSR_TRANSIENT_FUNC1DC8_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func1dc8_pic"
    CACHE PATH "Private position-independent Func_1dc8 RAM-image AOT output")
set(GSR_TRANSIENT_FUNC15AFC_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func15afc_pic"
    CACHE PATH "Private position-independent Func_15afc RAM-image AOT output")
set(GSR_TRANSIENT_FUNC15E10_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func15e10_pic"
    CACHE PATH "Private position-independent Func_15e10 RAM-image AOT output")
set(GSR_TRANSIENT_FUNC15D74_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func15d74_pic"
    CACHE PATH "Private position-independent Func_15d74 RAM-image AOT output")
set(GSR_TRANSIENT_FUNC155D0_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func155d0_pic"
    CACHE PATH "Private position-independent Func_155d0 RAM-image AOT output")
set(GSR_TRANSIENT_FUNC9BB8_SHORT_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func9bb8_short_pic"
    CACHE PATH "Private position-independent Func_9bb8 short-copy RAM-image AOT output")
set(GSR_TRANSIENT_FUNC9BB8_CODE_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_func9bb8_code_pic"
    CACHE PATH "Private position-independent Func_9bb8 code-only RAM-image AOT output")
set(GSR_TRANSIENT_FUNCB5138_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_funcb5138_pic"
    CACHE PATH "Private position-independent Func_b5138 RAM-image AOT output")
set(GSR_TRANSIENT_FUNCA418_PIC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/transient_funca418_pic"
    CACHE PATH "Private position-independent Func_a418 RAM-image AOT output")
set(GSR_SYNTH_DC8_030008D4_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/synth_dc8_030008d4"
    CACHE PATH "Private identity-prefixed synthesized Func_dc8 AOT output")
set(GSR_SYNTH_DC8_03000904_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/synth_dc8_03000904"
    CACHE PATH "Private identity-prefixed third synthesized Func_dc8 AOT output")
set(GSR_SYNTH_DC8_030008EC_DIR
    "${CMAKE_SOURCE_DIR}/local/gs011/synth_dc8_030008ec"
    CACHE PATH "Private identity-prefixed second synthesized Func_dc8 AOT output")
set(GSR_OVERLAY_ROOT
    "${CMAKE_SOURCE_DIR}/local/gs011"
    CACHE PATH "Root containing private identity-prefixed overlay_<id> AOT outputs")

if(GSR_BUILD_LOCAL_RUNNER)
    if(NOT EXISTS "${GBARECOMP_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR
            "GBARECOMP_ROOT is not a gbarecomp checkout: ${GBARECOMP_ROOT}")
    endif()
    if(NOT EXISTS "${GSR_GENERATED_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_GENERATED_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_GENERATED_DIR is not a complete private corpus: "
            "${GSR_GENERATED_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC2D5C_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC2D5C_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC2D5C_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC2D5C_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNCA37C_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNCA37C_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNCA37C_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNCA37C_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC2808_03006000_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC2808_03006000_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC2808_03006000_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC2808_03006000_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC6ABC_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC6ABC_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC6ABC_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC6ABC_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC15E10_03003A84_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC15E10_03003A84_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC15E10_03003A84_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC15E10_03003A84_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNCA418_03003400_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNCA418_03003400_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNCA418_03003400_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNCA418_03003400_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC15AFC_03003A84_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC15AFC_03003A84_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC15AFC_03003A84_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC15AFC_03003A84_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC1B70_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC1B70_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC1B70_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC1B70_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC9BB8_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC9BB8_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC9BB8_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC9BB8_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC15430_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC15430_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC15430_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC15430_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC15570_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC15570_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC15570_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC15570_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC158E8_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC158E8_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC158E8_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC158E8_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC2544_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC2544_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC2544_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC2544_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC2CF4_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC2CF4_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC2CF4_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC2CF4_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC1DC8_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC1DC8_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC1DC8_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC1DC8_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC15AFC_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC15AFC_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC15AFC_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC15AFC_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC15E10_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC15E10_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC15E10_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC15E10_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC15D74_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC15D74_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC15D74_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC15D74_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC155D0_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC155D0_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC155D0_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC155D0_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC9BB8_SHORT_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC9BB8_SHORT_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC9BB8_SHORT_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC9BB8_SHORT_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNC9BB8_CODE_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNC9BB8_CODE_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNC9BB8_CODE_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNC9BB8_CODE_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNCB5138_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNCB5138_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNCB5138_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNCB5138_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_TRANSIENT_FUNCA418_PIC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_TRANSIENT_FUNCA418_PIC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_TRANSIENT_FUNCA418_PIC_DIR is not a complete private corpus: "
            "${GSR_TRANSIENT_FUNCA418_PIC_DIR}")
    endif()
    if(NOT EXISTS "${GSR_SYNTH_DC8_030008D4_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_SYNTH_DC8_030008D4_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_SYNTH_DC8_030008D4_DIR is not a complete private corpus: "
            "${GSR_SYNTH_DC8_030008D4_DIR}")
    endif()
    if(NOT EXISTS "${GSR_SYNTH_DC8_03000904_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_SYNTH_DC8_03000904_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_SYNTH_DC8_03000904_DIR is not a complete private corpus: "
            "${GSR_SYNTH_DC8_03000904_DIR}")
    endif()
    if(NOT EXISTS "${GSR_SYNTH_DC8_030008EC_DIR}/recompiled.h" OR
       NOT EXISTS "${GSR_SYNTH_DC8_030008EC_DIR}/dispatch_table.cpp")
        message(FATAL_ERROR
            "GSR_SYNTH_DC8_030008EC_DIR is not a complete private corpus: "
            "${GSR_SYNTH_DC8_030008EC_DIR}")
    endif()
    file(STRINGS "${CMAKE_SOURCE_DIR}/config/usa/overlay-registry.inc"
        GSR_OVERLAY_REGISTRY_LINES REGEX "^GSR_OVERLAY\\(")
    set(GSR_OVERLAY_IDS)
    foreach(GSR_OVERLAY_LINE IN LISTS GSR_OVERLAY_REGISTRY_LINES)
        string(REGEX MATCH "^GSR_OVERLAY\\(([a-z0-9_]+)," _ "${GSR_OVERLAY_LINE}")
        if(NOT CMAKE_MATCH_1)
            message(FATAL_ERROR "Malformed overlay registry row: ${GSR_OVERLAY_LINE}")
        endif()
        list(APPEND GSR_OVERLAY_IDS "${CMAKE_MATCH_1}")
        set(GSR_OVERLAY_DIR "${GSR_OVERLAY_ROOT}/overlay_${CMAKE_MATCH_1}")
        if(NOT EXISTS "${GSR_OVERLAY_DIR}/recompiled.h" OR
           NOT EXISTS "${GSR_OVERLAY_DIR}/dispatch_table.cpp")
            message(FATAL_ERROR
                "Overlay registry corpus is missing or incomplete: ${GSR_OVERLAY_DIR}")
        endif()
    endforeach()

    add_subdirectory("${GBARECOMP_ROOT}" gbarecomp_build EXCLUDE_FROM_ALL)
    target_compile_definitions(gbarecomp_runtime PRIVATE
        GBARECOMP_WINDOW_TITLE="Golden Sun Recompiled")
    file(GLOB GSR_GENERATED_SOURCES CONFIGURE_DEPENDS
        "${GSR_GENERATED_DIR}/recompiled_*.cpp"
        "${GSR_GENERATED_DIR}/dispatch_table.cpp"
        "${GSR_GENERATED_DIR}/symbol_map.cpp")
    file(GLOB GSR_TRANSIENT_FUNC2D5C_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC2D5C_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC2D5C_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC2D5C_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNCA37C_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNCA37C_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNCA37C_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNCA37C_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC2808_03006000_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC2808_03006000_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC2808_03006000_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC2808_03006000_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC6ABC_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC6ABC_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC6ABC_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC6ABC_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC15E10_03003A84_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC15E10_03003A84_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC15E10_03003A84_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC15E10_03003A84_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNCA418_03003400_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNCA418_03003400_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNCA418_03003400_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNCA418_03003400_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC15AFC_03003A84_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC15AFC_03003A84_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC15AFC_03003A84_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC15AFC_03003A84_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC1B70_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC1B70_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC1B70_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC1B70_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC9BB8_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC9BB8_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC9BB8_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC9BB8_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC15430_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC15430_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC15430_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC15430_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC15570_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC15570_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC15570_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC15570_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC158E8_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC158E8_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC158E8_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC158E8_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC2544_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC2544_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC2544_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC2544_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC2CF4_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC2CF4_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC2CF4_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC2CF4_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC1DC8_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC1DC8_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC1DC8_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC1DC8_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC15AFC_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC15AFC_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC15AFC_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC15AFC_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC15E10_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC15E10_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC15E10_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC15E10_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC15D74_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC15D74_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC15D74_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC15D74_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC155D0_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC155D0_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC155D0_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC155D0_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC9BB8_SHORT_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC9BB8_SHORT_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC9BB8_SHORT_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC9BB8_SHORT_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNC9BB8_CODE_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNC9BB8_CODE_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNC9BB8_CODE_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNC9BB8_CODE_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNCB5138_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNCB5138_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNCB5138_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNCB5138_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_TRANSIENT_FUNCA418_PIC_SOURCES CONFIGURE_DEPENDS
        "${GSR_TRANSIENT_FUNCA418_PIC_DIR}/recompiled.cpp"
        "${GSR_TRANSIENT_FUNCA418_PIC_DIR}/recompiled_*.cpp"
        "${GSR_TRANSIENT_FUNCA418_PIC_DIR}/dispatch_table.cpp")
    file(GLOB GSR_SYNTH_DC8_030008D4_SOURCES CONFIGURE_DEPENDS
        "${GSR_SYNTH_DC8_030008D4_DIR}/recompiled.cpp"
        "${GSR_SYNTH_DC8_030008D4_DIR}/recompiled_*.cpp"
        "${GSR_SYNTH_DC8_030008D4_DIR}/dispatch_table.cpp")
    file(GLOB GSR_SYNTH_DC8_03000904_SOURCES CONFIGURE_DEPENDS
        "${GSR_SYNTH_DC8_03000904_DIR}/recompiled.cpp"
        "${GSR_SYNTH_DC8_03000904_DIR}/recompiled_*.cpp"
        "${GSR_SYNTH_DC8_03000904_DIR}/dispatch_table.cpp")
    file(GLOB GSR_SYNTH_DC8_030008EC_SOURCES CONFIGURE_DEPENDS
        "${GSR_SYNTH_DC8_030008EC_DIR}/recompiled.cpp"
        "${GSR_SYNTH_DC8_030008EC_DIR}/recompiled_*.cpp"
        "${GSR_SYNTH_DC8_030008EC_DIR}/dispatch_table.cpp")
    set(GSR_PROACTIVE_OVERLAY_SOURCES)
    foreach(GSR_OVERLAY_ID IN LISTS GSR_OVERLAY_IDS)
        set(GSR_OVERLAY_DIR "${GSR_OVERLAY_ROOT}/overlay_${GSR_OVERLAY_ID}")
        file(GLOB GSR_ONE_OVERLAY_SOURCES CONFIGURE_DEPENDS
            "${GSR_OVERLAY_DIR}/recompiled.cpp"
            "${GSR_OVERLAY_DIR}/recompiled_*.cpp"
            "${GSR_OVERLAY_DIR}/dispatch_table.cpp")
        list(APPEND GSR_PROACTIVE_OVERLAY_SOURCES ${GSR_ONE_OVERLAY_SOURCES})
    endforeach()

    add_executable(GoldenSunRecomp
        src/runner_main.cpp
        src/function_tracer.cpp
        ${GSR_GENERATED_SOURCES}
        ${GSR_TRANSIENT_FUNC2D5C_SOURCES}
        ${GSR_TRANSIENT_FUNCA37C_SOURCES}
        ${GSR_TRANSIENT_FUNC2808_03006000_SOURCES}
        ${GSR_TRANSIENT_FUNC6ABC_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC15E10_03003A84_SOURCES}
        ${GSR_TRANSIENT_FUNCA418_03003400_SOURCES}
        ${GSR_TRANSIENT_FUNC15AFC_03003A84_SOURCES}
        ${GSR_TRANSIENT_FUNC1B70_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC9BB8_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC15430_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC15570_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC158E8_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC2544_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC2CF4_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC1DC8_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC15AFC_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC15E10_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC15D74_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC155D0_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC9BB8_SHORT_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNC9BB8_CODE_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNCB5138_PIC_SOURCES}
        ${GSR_TRANSIENT_FUNCA418_PIC_SOURCES}
        ${GSR_SYNTH_DC8_030008D4_SOURCES}
        ${GSR_SYNTH_DC8_03000904_SOURCES}
        ${GSR_SYNTH_DC8_030008EC_SOURCES}
        ${GSR_PROACTIVE_OVERLAY_SOURCES})
    if(GSR_IPO_SUPPORTED)
        set_property(TARGET GoldenSunRecomp PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
    target_include_directories(GoldenSunRecomp PRIVATE
        "${GSR_GENERATED_DIR}"
        "${GBARECOMP_ROOT}/src/armv4t"
        "${GBARECOMP_ROOT}/src/gba"
        "${GBARECOMP_ROOT}/src/runtime"
        "${GBARECOMP_ROOT}/src/debug"
        "${CMAKE_SOURCE_DIR}/config/usa")
    target_compile_definitions(GoldenSunRecomp PRIVATE
        GBARECOMP_DEFAULT_DEBUG_PORT=19843
        GBARECOMP_DEFAULT_GAME_CONFIG="game.toml")

    if(MSVC)
        target_link_libraries(GoldenSunRecomp PRIVATE
            gbarecomp_runtime gbarecomp_debug gbarecomp_gba
            gbarecomp_armv4t)
        target_link_options(GoldenSunRecomp PRIVATE /STACK:16777216)
    else()
        target_compile_options(GoldenSunRecomp PRIVATE
            -ffunction-sections -fdata-sections)
        target_link_libraries(GoldenSunRecomp PRIVATE
            "-Wl,--start-group"
            gbarecomp_runtime gbarecomp_debug gbarecomp_gba
            gbarecomp_armv4t
            "-Wl,--end-group")
        target_link_options(GoldenSunRecomp PRIVATE -Wl,--gc-sections)
        if(MINGW OR WIN32)
            target_link_options(GoldenSunRecomp PRIVATE -Wl,--stack,16777216)
        endif()
        if(GSR_IPO_SUPPORTED AND GSR_LTO_JOBS)
            # -flto=N here overrides the plain -flto that
            # INTERPROCEDURAL_OPTIMIZATION already put on this link line,
            # asking GCC to run LTRANS across N parallel processes. See the
            # GSR_LTO_JOBS comment near the top of this file for why this
            # isn't set to the full core count.
            target_link_options(GoldenSunRecomp PRIVATE -flto=${GSR_LTO_JOBS})
        endif()
    endif()

    add_test(NAME runner_help COMMAND GoldenSunRecomp --help)

    # Split debug symbols out of the executable so the shipped binary stays
    # small while crash/hang stack traces can still resolve against the
    # separate .debug file (GNU objcopy debuglink convention).
    if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
        find_program(CMAKE_OBJCOPY objcopy)
        if(CMAKE_OBJCOPY)
            add_custom_command(TARGET GoldenSunRecomp POST_BUILD
                COMMAND "${CMAKE_OBJCOPY}" --only-keep-debug "$<TARGET_FILE:GoldenSunRecomp>" "$<TARGET_FILE:GoldenSunRecomp>.debug"
                COMMAND "${CMAKE_OBJCOPY}" --strip-all "$<TARGET_FILE:GoldenSunRecomp>"
                COMMAND "${CMAKE_OBJCOPY}" --add-gnu-debuglink="$<TARGET_FILE_NAME:GoldenSunRecomp>.debug" "$<TARGET_FILE:GoldenSunRecomp>"
                WORKING_DIRECTORY "$<TARGET_FILE_DIR:GoldenSunRecomp>"
                COMMENT "Splitting debug symbols for GoldenSunRecomp")
        else()
            message(STATUS "objcopy not found; skipping debug-symbol split for GoldenSunRecomp")
        endif()
    endif()

    if(WIN32)
        add_executable(GoldenSunLauncher WIN32
            src/launcher_main.cpp
            "${GBARECOMP_ROOT}/src/runtime/crash_handler.cpp")
        target_include_directories(GoldenSunLauncher PRIVATE
            "${GBARECOMP_ROOT}/src/runtime")
        target_compile_definitions(GoldenSunLauncher PRIVATE
            UNICODE _UNICODE NOMINMAX)
        target_link_libraries(GoldenSunLauncher PRIVATE bcrypt comdlg32 gdiplus)
        set_target_properties(GoldenSunLauncher PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
        add_dependencies(GoldenSunRecomp GoldenSunLauncher)
    endif()

    # Synthetic-fixture coverage for the code-only identity hash
    # (src/relocatable_identity.h). No ROM/BIOS needed, only gbarecomp_gba's
    # sha1.cpp - gated the same as the rest of the local runner because that
    # is the only source of gba::sha1.
    add_executable(gsr_relocatable_identity_test
        tests/cpp/relocatable_identity_test.cpp)
    target_include_directories(gsr_relocatable_identity_test PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${GBARECOMP_ROOT}/src/gba")
    target_link_libraries(gsr_relocatable_identity_test PRIVATE gbarecomp_gba)
    add_test(NAME relocatable_identity_test
             COMMAND gsr_relocatable_identity_test)

    # Synthetic-fixture coverage for the page-epoch cache core added to
    # src/relocatable_identity.h (the per-dispatch SHA-1 rehash fix). Pure
    # logic over a synthetic epoch table, no guest bus, no sha1 dependency.
    add_executable(gsr_relocatable_identity_cache_test
        tests/cpp/relocatable_identity_cache_test.cpp)
    target_include_directories(gsr_relocatable_identity_cache_test PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${GBARECOMP_ROOT}/src/gba")
    add_test(NAME relocatable_identity_cache_test
             COMMAND gsr_relocatable_identity_cache_test)
endif()
