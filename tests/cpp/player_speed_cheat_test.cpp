#include <cstdint>
#include <cstdio>
#include <string>

#include "player_speed_cheat.h"
#include "player_walk_run_speed_config.h"

int main() {
    using namespace gsr::player_speed_cheat;
    using namespace gbarecomp;
    if (!matches(kWriterXPc, kAccumulatorX, 4u) ||
        !matches(kWriterYPc, kAccumulatorY, 4u) ||
        !matches(kFieldRunWriterPc, kFieldAccumulator, 4u) ||
        !matches(kFieldWalkWriterPc, kFieldAccumulator, 4u) ||
        matches(kWriterPc + 2u, kAccumulator, 4u) ||
        matches(kWriterPc, kAccumulator + 4u, 4u) ||
        matches(kWriterPc, kAccumulator, 2u) ||
        matches(0x08091674u, kFieldAccumulator, 4u) ||
        matches(0x08093CECu, kFieldAccumulator, 4u)) {
        std::printf("player_speed_cheat_test: identity mismatch\n");
        return 1;
    }
    if (transform(100u, 112u, 1u) != 112u ||
        transform(100u, 112u, 2u) != 224u ||
        transform(100u, 112u, 3u) != 336u ||
        transform(0u, 0x00008000u, 3u) != 0x00018000u ||
        transform(0u, 0x00010000u, 3u) != 0x00030000u ||
        transform(0u, 0x00018000u, 3u) != 0x00048000u ||
        transform(0xFFFFFFF0u, 0x80000010u, 2u) != 0x20u ||
        transform(0u, 0x80000010u, 3u) != 0x80000030u) {
        std::printf("player_speed_cheat_test: transform mismatch\n");
        return 1;
    }
    if (normalize_player_walk_run_speed_multiplier(0) != 1 ||
        normalize_player_walk_run_speed_multiplier(1) != 1 ||
        normalize_player_walk_run_speed_multiplier(2) != 2 ||
        normalize_player_walk_run_speed_multiplier(3) != 3 ||
        normalize_player_walk_run_speed_multiplier(4) != 1 ||
        parse_player_walk_run_speed_multiplier("1") != 1 ||
        parse_player_walk_run_speed_multiplier("2") != 2 ||
        parse_player_walk_run_speed_multiplier("3") != 3 ||
        parse_player_walk_run_speed_multiplier("4") != 0 ||
        parse_player_walk_run_speed_multiplier("2x") != 0 ||
        resolve_player_walk_run_speed_multiplier(3, true, true) != 3 ||
        resolve_player_walk_run_speed_multiplier(0, true, true) != 2 ||
        resolve_player_walk_run_speed_multiplier(0, true, false) != 1 ||
        resolve_player_walk_run_speed_multiplier(0, false, false) != 1 ||
        !parse_legacy_player_walk_run_2x("true") ||
        !parse_legacy_player_walk_run_2x("1") ||
        parse_legacy_player_walk_run_2x("false") ||
        std::string(player_walk_run_speed_persisted_value(1)) != "1" ||
        std::string(player_walk_run_speed_persisted_value(2)) != "2" ||
        std::string(player_walk_run_speed_persisted_value(3)) != "3") {
        std::printf("player_speed_cheat_test: config policy mismatch\n");
        return 1;
    }
    return 0;
}
