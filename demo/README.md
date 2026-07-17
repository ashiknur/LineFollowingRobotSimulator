# Demo track + follower

A full-feature demonstration for the simulator.

| File | What it is |
|---|---|
| `demo-track.png` | The track — load it with **File > Open Track...** |
| `DemoUserCode.cpp` | The follower — paste into the editor (or copy over `%LOCALAPPDATA%\LineFollowingRobotSimulator\UserCode.cpp` and press Reload) |
| `make-track.ps1` | Regenerates `demo-track.png` |
| `sim.py` | Offline replica of the app's physics/sensors used to tune the follower (needs Python + Pillow); writes `sim-path.png` with the driven path |

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
