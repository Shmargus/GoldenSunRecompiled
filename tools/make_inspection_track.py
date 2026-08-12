#!/usr/bin/env python3
"""Generate a GBARECOMP_INPUT_REPLAY track for Golden Sun visual inspection.

Format is the one GBARECOMP_INPUT_RECORD writes: "frame,0xNNNN" rows, emitted
only when the value changes. KEYINPUT is ACTIVE LOW - a clear bit is a press.

Goal track: hold nothing through the BIOS intro, tap through the title, file
select, name entry and opening text until the party is controllable, then walk
a box and open/close the menus.
"""
import sys

A, B, SEL, START, RIGHT, LEFT, UP, DOWN, R, L = (1 << i for i in range(10))
NONE = 0x03FF


def released(*bits):
    """KEYINPUT value with the given buttons pressed (active low)."""
    v = NONE
    for b in bits:
        v &= ~b
    return v & 0x03FF


class Track:
    def __init__(self):
        self.rows = []   # (frame, value)
        self.frame = 0

    def idle(self, n):
        self._emit(NONE)
        self.frame += n

    def tap(self, *bits, hold=8, gap=14, times=1):
        """Edge-triggered menus need the release, so every tap is hold+gap."""
        for _ in range(times):
            self._emit(released(*bits))
            self.frame += hold
            self._emit(NONE)
            self.frame += gap

    def hold(self, *bits, n=60):
        self._emit(released(*bits))
        self.frame += n
        self._emit(NONE)

    def _emit(self, value):
        if self.rows and self.rows[-1][1] == value:
            return
        self.rows.append((self.frame, value))

    def write(self, path):
        with open(path, "w", newline="") as f:
            for frame, value in self.rows:
                f.write("%d,0x%04X\n" % (frame, value))


t = Track()

# 1. BIOS intro + publisher logos: no input at all (the intro must run real).
t.idle(620)

# 2. Title screen -> file select -> new game. Start and A both advance
#    different prompts, so alternate rather than betting on one.
for _ in range(6):
    t.tap(START)
    t.tap(A)

# 3. Name entry. The default name is pre-filled, so confirming is all that is
#    needed; Start is the usual accept, with A taps in case focus is on END.
for _ in range(8):
    t.tap(START)
    t.tap(A, times=2)

# 4. Opening text / cutscene: sustained A tapping.
t.tap(A, times=60)

# 5. Walk a box, so movement and scrolling are both visible.
for _ in range(2):
    t.hold(DOWN, n=45)
    t.idle(10)
    t.hold(RIGHT, n=45)
    t.idle(10)
    t.hold(UP, n=45)
    t.idle(10)
    t.hold(LEFT, n=45)
    t.idle(10)

# 6. Menus: Start opens the main menu, B backs out; Select is the party menu.
t.tap(START); t.idle(60)
t.tap(A);     t.idle(60)
t.tap(B);     t.idle(30)
t.tap(B);     t.idle(30)
t.tap(SEL);   t.idle(60)
t.tap(B);     t.idle(30)

# 7. Walk again afterwards, to prove the menus were exited cleanly.
t.hold(RIGHT, n=60)
t.idle(60)

# 8. The prologue locks the menu, and it is long. Keep advancing text, walking
#    between rooms, and retrying the menu until it actually opens.
for _ in range(12):
    t.tap(A, times=40)
    t.hold(DOWN, n=50); t.idle(8)
    t.hold(RIGHT, n=50); t.idle(8)
    t.hold(UP, n=50); t.idle(8)
    t.hold(LEFT, n=50); t.idle(8)
    t.tap(START); t.idle(70)          # try the main menu
    t.tap(B); t.idle(20)              # back out if it opened
    t.tap(SEL); t.idle(70)            # try the party/summon menu
    t.tap(B); t.idle(20)

out = sys.argv[1] if len(sys.argv) > 1 else "gs_track.csv"
t.write(out)
print("wrote %s: %d rows, %d frames" % (out, len(t.rows), t.frame))
