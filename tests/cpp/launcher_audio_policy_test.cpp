#include "launcher_audio_policy.h"

namespace {

bool require(bool condition) {
    return condition;
}

}  // namespace

int main() {
    using gsr::resolve_launcher_audio_policy;
    return require(
               !resolve_launcher_audio_policy(false, false, false, false)
                    .native_mp2k &&
               !resolve_launcher_audio_policy(false, true, false, false)
                    .turbo_decoupled &&
               resolve_launcher_audio_policy(true, true, false, false)
                   .native_mp2k &&
               resolve_launcher_audio_policy(true, true, false, false)
                   .turbo_decoupled &&
               !resolve_launcher_audio_policy(true, true, true, false)
                    .native_mp2k &&
               !resolve_launcher_audio_policy(true, true, true, false)
                    .turbo_decoupled &&
               !resolve_launcher_audio_policy(true, true, false, true)
                    .turbo_decoupled)
        ? 0
        : 1;
}
