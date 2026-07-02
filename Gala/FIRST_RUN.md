# First run (Mac, UE 5.4+)

1. Open `Gala.uproject` (double-click). If prompted to pick/switch engine version, accept.
   If prompted to rebuild modules (Gala, Gala* plugins), say **Yes** — first compile takes a few
   minutes.
2. The editor opens on the Entry map. Just press **Play** (⌥+P).
   `GalaGameMode` spawns the seed world (terrain + all 268 km of streets from `seed/`) and drops
   the proto car at **5591 Gala Ave**, facing the way to school.
3. Drive: **W/S** or **↑/↓** throttle/brake · **A/D** or **←/→** steer · **Space** handbrake ·
   **R** reset to the start.

What you're looking at is the M0 grey-box: real geometry, placeholder everything else. The car is
an arcade stand-in — the real substepped physics + Moza FFB are the actual M0 deliverable
(`CAR_FEEL.md`) and land next, on the Windows side for the hardware seam.

If Play shows an empty void, check the Output Log for `LogGalaSeed` errors — the loader
hard-fails loudly if `seed/` is missing or malformed.
