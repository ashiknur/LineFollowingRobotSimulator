# Demo track + follower

A full-feature demonstration for the simulator.

| File | What it is |
|---|---|
| `demo-track.png` | The track — load it with **File > Open Track...** |
| `demo-track2.png` | Second track, circuit-diagram style: sine squiggle with a loop, zigzag column, diamond, two overlapping circles, dashed-rectangle passage, separate start/end squares. Start: X=100 Y=630 Angle=-90 |
| `DemoUserCode.cpp` | The follower — paste into the editor (or copy over `%LOCALAPPDATA%\LineFollowingRobotSimulator\UserCode.cpp` and press Reload) |
| `make-track.ps1` | Regenerates `demo-track.png` |
| `make-track2.ps1` | Regenerates `demo-track2.png` |
| `sim.py` | Offline replica of the app's physics/sensors used to tune the follower (needs Python + Pillow); writes `sim-path.png` with the driven path |

## Checkpoints & scoring (Stats menu)

The **Stats** menu opens the Run Statistics panel:

- **Checkpoints — place them with the mouse.** Pick the **Checkpoint** tool
  in the Drawing Tools panel (or *Stats > Place Checkpoint with Mouse*),
  then on the canvas **click where the checkpoint goes and drag outward to
  aim its direction**; an arrow and the live angle follow the cursor, and
  releasing places it. A click with no drag keeps the angle in the panel
  field. **Right-click a marker deletes it.** Click the button again — or
  pick any drawing tool — to leave the mode. Drawing is paused while
  placing, so a stray stroke can't land on the track.
- Checkpoints can also be typed exactly as X / Y / Angle in the Stats panel
  (or click *Use Robot Pos* to capture the robot's current pose) and added
  with *Add Checkpoint*.
- Markers draw as numbered circles with a direction arrow; orange = not yet
  reached, green = reached. The robot auto-reaches a checkpoint by driving
  within 40 px of it. The **last checkpoint is the end point**.
- **Skip (Ctrl+K)** teleports to the next checkpoint; **Restart (Ctrl+R)**
  goes back to the last reached one (or the start). Both are counted.
- A run starts when the sim starts (or *New Run*) and finishes when the
  robot stands still for 2 seconds. Paused time is not counted.
- Score (all five weights editable in the panel):
  `score = b − s·skips − r·restarts − t + c (no skip/restart) + st (stopped at end)`

## Track features

Start/end is the filled 90 px black square (bigger than the robot). The lap,
counterclockwise: long straight → 180° curve (radius 120) → dotted line →
90° corner → 45° angle → **inverted section** (white line through a black
patch) → right triangle (two solid sides; dotted hypotenuse) → vertical
return crossing **two circles** perpendicularly → two more 90° corners →
back into the same square, where the robot stops.

## Running the demo

1. **File > Open Track...** → `demo-track.png`
2. Sensors: **5 sensors, gap 10 px**
3. Robot Start Position: **X=95, Y=590, Angle=0** → *Reset Robot Pos*
4. **Run** — the lap takes about 75 seconds and ends with the robot
   stopped on the square.

## How the follower works

- Follows the **minority color**: dark line on white ground; white line when
  the dark majority has an interior white gap (`[1,1,0,1,1]`) — that's the
  inverted section.
- **Wide bar** (3+ dark sensors) = crossing a perpendicular feature (corner
  branch, circle outline, square edge): drive straight through, remember
  which side its mass was on.
- **Loss right after a bar** = a 90° corner: latch a pivot toward the bar
  side until the line is re-centered (capped below 180° so it can never
  turn around). Loss with no bar = dotted gap: coast straight.
- **Sustained all-dark** after leaving the start = the end square: stop.
