# Offline simulator that replicates the app's physics + sensor model exactly,
# so the demo follower can be verified/tuned without the GUI.
#
#   physics  : Robot::update  (differential drive, midpoint integration)
#   sensors  : Sensor::readSensors (offset 30 px, N sensors, gray < 120 = dark)
#   timestep : the app renders at 60 fps -> dt = 1/60 s
#
# The follower below is a line-for-line port of DemoUserCode.cpp; tune here,
# then copy the numbers/logic back to the .cpp.
import math, sys
from PIL import Image, ImageDraw

TRACK = sys.argv[1] if len(sys.argv) > 1 else "demo-track.png"
img = Image.open(TRACK).convert("RGB")
W, H = img.size
px = img.load()

THRESHOLD  = 120
OFFSET     = 30.0
SPACING    = 10.0
COUNT      = 5
WHEEL_BASE = 44.0
DT         = 1.0 / 60.0

def read_gray(x, y):
    xi = min(max(int(x), 0), W - 1)
    yi = min(max(int(y), 0), H - 1)
    r, g, b = px[xi, yi]
    return (r + g + b) // 3

def read_sensors(pos, ang_deg):
    fwd = math.radians(ang_deg)
    perp = fwd + math.pi / 2
    out = []
    coords = []
    for i in range(COUNT):
        lat = (i - (COUNT - 1) * 0.5) * SPACING
        sx = pos[0] + math.cos(fwd) * OFFSET + math.cos(perp) * lat
        sy = pos[1] + math.sin(fwd) * OFFSET + math.sin(perp) * lat
        out.append(1 if read_gray(sx, sy) < THRESHOLD else 0)
        coords.append((sx, sy))
    return out, coords

# ----------------------------------------------------------------------------
# Follower — port of DemoUserCode.cpp
# ----------------------------------------------------------------------------
BASE     = 36.0
KP       = 26.0
KD       = 9.0
PIVOT    = 70.0
PIVOT_IN = -20.0

st = dict(prevErr=0.0, searchDir=1, lostFrames=0,
          allDarkCount=0, leftStart=False, finished=False,
          corner=0, cornerDir=0, barAge=999)

CORNER_MAX = 60      # max latched-turn frames (~1 s, < 180 degrees)
BAR_WINDOW = 25      # loss within this many frames of a wide bar = corner

def follower(s):
    n = len(s)
    dark = sum(1 for v in s if v)
    maxErr = (n - 1) * 0.5

    if st["finished"]:
        return 0.0, 0.0

    if dark == n:
        st["allDarkCount"] += 1
        st["corner"] = 0
        if st["leftStart"] and st["allDarkCount"] > 25:
            st["finished"] = True
            return 0.0, 0.0
        return BASE, BASE
    st["allDarkCount"] = 0
    st["leftStart"] = True

    # Pattern classification:
    #  - normal ground: dark minority = the line
    #  - inverted lane: dark majority WITH both outer sensors dark
    #    (the white gap is interior: [1,1,0,1,1])
    #  - dark majority with the gap at an edge ([1,1,1,0,0]) is the edge of
    #    a filled square or a skewed wide crossing -> just drive straight,
    #    never steer along it
    dark_majority = dark > n // 2
    inverted = dark_majority and s[0] and s[n-1]

    # Wide dark bar on normal ground = the row is crossing a perpendicular
    # line: a 90-degree corner ahead, a circle crossing, or a square edge.
    # Must be recorded BEFORE the drive-straight rule below, because the
    # corner's own bar is usually an edge-gap pattern like [0,0,1,1,1].
    if not inverted and dark >= 3:
        st["barAge"] = 0
        # The bar's dark mass sits on the side the corner turns toward —
        # capture it now, because these frames return early below and the
        # normal searchDir update never sees them.
        rawErr = sum(i - maxErr for i in range(n) if s[i]) / dark
        if abs(rawErr) > 0.2:
            st["searchDir"] = 1 if rawErr > 0 else -1
    else:
        st["barAge"] += 1

    if dark_majority and not inverted:
        st["lostFrames"] = 0
        return BASE, BASE

    ssum = 0.0
    cnt = 0
    for i in range(n):
        onLine = (s[i] == 0) if inverted else (s[i] != 0)
        if onLine:
            ssum += i - maxErr
            cnt += 1

    err = ssum / cnt if cnt else 0.0

    # ── Latched corner turn (in progress) ──
    if st["corner"] > 0:
        st["corner"] += 1
        if (cnt > 0 and abs(err) <= 1.0) or st["corner"] > CORNER_MAX:
            st["corner"] = 0        # line reacquired near center: resume PD
        else:
            if st["cornerDir"] > 0:
                return PIVOT, PIVOT_IN
            return PIVOT_IN, PIVOT

    if cnt == 0:
        st["lostFrames"] += 1
        # Loss right after a wide bar is a corner (every 90-degree corner
        # sweeps the perpendicular branch across the row first): latch a
        # turn toward the side the line went. A loss WITHOUT a recent bar
        # is a dotted gap -> just coast.
        if st["lostFrames"] >= 3 and st["barAge"] < BAR_WINDOW:
            st["corner"] = 1
            st["cornerDir"] = st["searchDir"]
            print(f"  [latch] step={st.get('step','?')} pos={st.get('pos','?')} dir={st['cornerDir']} barAge={st['barAge']}")
            if st["cornerDir"] > 0:
                return PIVOT, PIVOT_IN
            return PIVOT_IN, PIVOT
        if st["lostFrames"] <= 35:
            return BASE, BASE          # dotted gap: coast straight
        turn = 12.0 * st["searchDir"]  # long loss: gentle forward arc
        return 30.0 + turn, 30.0 - turn
    st["lostFrames"] = 0

    if abs(err) > 0.2:
        st["searchDir"] = 1 if err > 0 else -1

    # ── PD steering (curves are handled purely by PD) ──
    dErr = err - st["prevErr"]
    st["prevErr"] = err
    corr = KP * err + KD * dErr
    left  = BASE + corr
    right = BASE - corr
    left  = max(-60.0, min(140.0, left))
    right = max(-60.0, min(140.0, right))
    return left, right

# ----------------------------------------------------------------------------
# Run
# ----------------------------------------------------------------------------
pos = [95.0, 590.0]
ang = 0.0
path = [(pos[0], pos[1])]
STEPS = 9000            # 150 s of sim time
left_start_region = False

for step in range(STEPS):
    s, _ = read_sensors(pos, ang)
    st["step"] = step
    st["pos"] = (round(pos[0]), round(pos[1]))
    vL, vR = follower(s)
    if 1330 <= step <= 1700 and step % 8 == 0:
        print(f"  step={step} pos=({pos[0]:.0f},{pos[1]:.0f}) ang={ang%360:.0f} s={s} "
              f"corner={st['corner']} lost={st['lostFrames']} bar={st['barAge']} "
              f"sd={st['searchDir']} v=({vL:.0f},{vR:.0f})")
    v = 0.5 * (vL + vR)
    omega = (vL - vR) / WHEEL_BASE
    dA = omega * DT
    mid = math.radians(ang) + dA * 0.5
    pos[0] += v * math.cos(mid) * DT
    pos[1] += v * math.sin(mid) * DT
    ang += math.degrees(dA)
    pos[0] = min(max(pos[0], 0.0), float(W))
    pos[1] = min(max(pos[1], 0.0), float(H))
    path.append((pos[0], pos[1]))

    if st["finished"]:
        print(f"FINISHED at step {step} ({step*DT:.1f}s), pos=({pos[0]:.0f},{pos[1]:.0f})")
        break
else:
    print(f"DID NOT FINISH in {STEPS} steps. final pos=({pos[0]:.0f},{pos[1]:.0f}) ang={ang:.0f}")

# Draw the path over the track for visual inspection
out = img.copy()
d = ImageDraw.Draw(out)
for i in range(1, len(path)):
    d.line([path[i-1], path[i]], fill=(220, 30, 30), width=2)
# mark start and final
d.ellipse([path[0][0]-5, path[0][1]-5, path[0][0]+5, path[0][1]+5], outline=(0,120,255), width=2)
fx, fy = path[-1]
d.ellipse([fx-6, fy-6, fx+6, fy+6], outline=(0,180,0), width=3)
out.save("sim-path.png")
print("wrote sim-path.png ; path points:", len(path))
