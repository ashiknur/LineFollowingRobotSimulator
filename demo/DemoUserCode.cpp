#include "UserCode.hpp"
#include "UserAPI.hpp"
#include <cmath>

// ============================================================
//  DEMO TRACK FOLLOWER  (for demo-track.png)
//
//  Set up before pressing Run:
//    1. File > Open Track...  ->  demo-track.png
//    2. Sensors: 5 sensors, gap 10 px
//    3. Robot Start Position: X=95  Y=590  Angle=0
//       then click "Reset Robot Pos"
//
//  Completes the full lap (~75 s): long straight, 180-degree
//  curve, dotted line, 90-degree corners, 45-degree bend, the
//  inverted section (white line on black), the triangle, both
//  circle crossings — and stops on the black square.
//
//  How it works:
//   * The line is the MINORITY color under the sensors: dark on
//     white ground, white inside the inverted section (only when
//     the white gap is interior, e.g. [1,1,0,1,1]).
//   * A "wide bar" (3+ dark sensors) means the row is crossing a
//     perpendicular line: a corner, a circle, or a square edge.
//     Drive straight through it, but remember which side its
//     mass was on.
//   * Losing the line right after a bar = a 90-degree corner:
//     latch a turn toward the bar side until the line is back
//     near center. Losing it with no bar = a dotted gap: coast.
//   * Sustained all-dark after leaving the start = the end
//     square: stop.
// ============================================================

static constexpr float BASE = 36.f;   // cruise speed (px/s)
static constexpr float KP = 26.f;   // steering gain
static constexpr float KD = 9.f;    // damping
static constexpr float PIVOT = 70.f;   // outer wheel in a latched turn
static constexpr float PIVOT_IN = -20.f;  // inner wheel in a latched turn
static constexpr int   LOOP_MS = 16;     // ~60 Hz control loop

static constexpr int CORNER_MAX = 60;  // latched-turn cap (~1 s, < 180 deg)
static constexpr int BAR_WINDOW = 25;  // loss this soon after a bar = corner

static float prevErr = 0.f;
static int   searchDir = 1;      // +1 = line/bar was last to the right
static int   lostFrames = 0;
static int   allDarkCount = 0;
static int   barAge = 999;
static int   corner = 0;         // latched-turn frame counter (0 = off)
static int   cornerDir = 0;
static bool  leftStart = false;
static bool  finished = false;

void setup()
{
    prevErr = 0.f;
    searchDir = 1;
    lostFrames = allDarkCount = 0;
    barAge = 999;
    corner = 0;
    cornerDir = 0;
    leftStart = false;
    finished = false;
}

void loop()
{
    if (finished) { setMotorSpeed(0.f, 0.f); delayMs(100); return; }

    auto s = readSensor();
    int n = (int)s.size();
    if (n < 2) { delayMs(LOOP_MS); return; }

    int dark = 0;
    for (int v : s) dark += (v ? 1 : 0);
    float maxErr = (n - 1) * 0.5f;

    // ── All dark: the filled start/end square ──
    if (dark == n)
    {
        ++allDarkCount;
        corner = 0;
        if (leftStart && allDarkCount > 25)   // sustained black: lap done
        {
            finished = true;
            setMotorSpeed(0.f, 0.f);
            return;
        }
        setMotorSpeed(BASE, BASE);
        delayMs(LOOP_MS);
        return;
    }
    allDarkCount = 0;
    leftStart = true;

    // ── Classify the pattern ──
    bool darkMajority = dark > n / 2;
    bool inverted = darkMajority && s[0] && s[n - 1];   // interior white gap

    // Wide dark bar on normal ground: crossing a perpendicular line.
    // Recorded before the drive-straight rule below, because a corner's
    // bar is usually an edge pattern like [0,0,1,1,1].
    if (!inverted && dark >= 3)
    {
        barAge = 0;
        float rawErr = 0.f;
        for (int i = 0; i < n; ++i)
            if (s[i]) rawErr += i - maxErr;
        rawErr /= dark;
        if (std::fabs(rawErr) > 0.2f) searchDir = (rawErr > 0.f) ? 1 : -1;
    }
    else
        ++barAge;

    // Dark majority with the gap at an edge = square edge / skewed
    // crossing: drive straight, never steer along it.
    if (darkMajority && !inverted)
    {
        lostFrames = 0;
        setMotorSpeed(BASE, BASE);
        delayMs(LOOP_MS);
        return;
    }

    // ── Locate the line as the minority color ──
    float sum = 0.f;
    int   cnt = 0;
    for (int i = 0; i < n; ++i)
    {
        bool onLine = inverted ? (s[i] == 0) : (s[i] != 0);
        if (onLine) { sum += i - maxErr; ++cnt; }
    }
    float err = (cnt > 0) ? sum / cnt : 0.f;

    // ── Latched corner turn in progress ──
    if (corner > 0)
    {
        ++corner;
        if ((cnt > 0 && std::fabs(err) <= 1.0f) || corner > CORNER_MAX)
            corner = 0;                       // reacquired: resume PD
        else
        {
            if (cornerDir > 0) setMotorSpeed(PIVOT, PIVOT_IN);
            else               setMotorSpeed(PIVOT_IN, PIVOT);
            delayMs(LOOP_MS);
            return;
        }
    }

    // ── Line lost ──
    if (cnt == 0)
    {
        ++lostFrames;
        if (lostFrames >= 3 && barAge < BAR_WINDOW)
        {
            // Lost right after crossing a bar: 90-degree corner.
            corner = 1;
            cornerDir = searchDir;
            if (cornerDir > 0) setMotorSpeed(PIVOT, PIVOT_IN);
            else               setMotorSpeed(PIVOT_IN, PIVOT);
        }
        else if (lostFrames <= 35)
            setMotorSpeed(BASE, BASE);        // dotted gap: coast straight
        else
        {
            // Long loss: gentle forward arc toward the last-seen side
            float turn = 12.f * (float)searchDir;
            setMotorSpeed(30.f + turn, 30.f - turn);
        }
        delayMs(LOOP_MS);
        return;
    }
    lostFrames = 0;

    if (std::fabs(err) > 0.2f) searchDir = (err > 0.f) ? 1 : -1;

    // ── PD steering (curves and bends are pure PD) ──
    float dErr = err - prevErr;
    prevErr = err;

    float corr = KP * err + KD * dErr;
    float left = BASE + corr;
    float right = BASE - corr;

    if (left > 140.f) left = 140.f;   if (left < -60.f) left = -60.f;
    if (right > 140.f) right = 140.f;   if (right < -60.f) right = -60.f;

    setMotorSpeed(left, right);
    delayMs(LOOP_MS);
}
