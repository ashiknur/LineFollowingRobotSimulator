# Generates demo-track.png — the full-feature demo track for the simulator.
#
# Route (robot starts on the black square at bottom-left, heading east):
#   start square -> long straight -> 180-degree arc (long curve) ->
#   straight -> dotted segment -> 90-degree corner -> 45-degree angle ->
#   inverted section (white line on black) -> triangle (two sides) ->
#   vertical return crossing two circles perpendicularly ->
#   two more 90-degree corners -> back into the same square (stop).
param([string]$Out = "$PSScriptRoot\demo-track.png")

Add-Type -AssemblyName System.Drawing

$W = 700; $H = 700
$bmp = New-Object System.Drawing.Bitmap($W, $H)
$g = [System.Drawing.Graphics]::FromImage($bmp)
# No antialiasing: crisp black/white edges read reliably by the sensors
# (gray fringe pixels would blur the threshold at 120)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
$g.Clear([System.Drawing.Color]::White)

$black = [System.Drawing.Color]::Black
$white = [System.Drawing.Color]::White
$brush = New-Object System.Drawing.SolidBrush($black)

function NewPen($color, $width) {
    $p = New-Object System.Drawing.Pen($color, $width)
    $p.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $p.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    return $p
}

$pen  = NewPen $black 11      # main line
# Wider than the sensor gap (10 px) so the white line can never fall
# entirely between two sensors inside the black patch
$wpen = NewPen $white 16      # inverted-section line

# ── Start/end square (90x90, bigger than the 55px robot) ─────────────────────
$g.FillRectangle($brush, 50, 545, 90, 90)

# ── Long straight (east along y=590) ─────────────────────────────────────────
$g.DrawLine($pen, 95, 590, 430, 590)

# ── Long curve: 180-degree arc, radius 120, center (430,470) ────────────────
# Enters heading east at (430,590), exits heading west at (430,350)
$g.DrawArc($pen, 310, 350, 240, 240, 90, -180)

# ── Straight west, then dotted segment, then solid up to the corner ─────────
$g.DrawLine($pen, 430, 350, 300, 350)
$dpen = NewPen $black 11
$dpen.DashPattern = @(1.4, 1.0)          # ~15px dash, ~11px gap
$g.DrawLine($dpen, 300, 350, 210, 350)
$g.DrawLine($pen, 210, 350, 170, 350)

# ── 90-degree corner: north ─────────────────────────────────────────────────
$g.DrawLine($pen, 170, 350, 170, 240)

# ── 45-degree angle bend: northeast ─────────────────────────────────────────
$g.DrawLine($pen, 170, 240, 250, 160)

# ── Realign straight, then the inverted section ─────────────────────────────
$g.DrawLine($pen, 250, 160, 315, 160)
$g.FillRectangle($brush, 310, 90, 190, 140)     # black background patch
$g.DrawLine($wpen, 315, 160, 495, 160)          # white line through it

# ── Resume black line to the triangle ───────────────────────────────────────
$g.DrawLine($pen, 500, 160, 540, 160)

# ── Right triangle: A(540,160) B(640,160) C(640,250) ────────────────────────
# Path follows A->B (east) and B->C (south, a 90-degree vertex); the closing
# hypotenuse C->A is sparse dots so the follower doesn't treat it as a
# junction, but the triangle reads visually complete.
$g.DrawLine($pen, 540, 160, 640, 160)
$g.DrawLine($pen, 640, 160, 640, 250)
$tpen = NewPen $black 6
$tpen.DashPattern = @(0.5, 3.5)
$g.DrawLine($tpen, 640, 250, 540, 160)

# ── Vertical return through two circles (perpendicular crossings) ───────────
# Kept at x=640 so the circles stay clear of the big arc (no junctions).
$g.DrawLine($pen, 640, 250, 640, 670)
$cpen = NewPen $black 11
$g.DrawEllipse($cpen, 590, 350, 100, 100)   # center (640,400) r50
$g.DrawEllipse($cpen, 595, 495, 90, 90)     # center (640,540) r45

# ── Two 90-degree corners home, into the square ─────────────────────────────
# y=670 keeps the outermost sensor clear of the square while passing under it
$g.DrawLine($pen, 640, 670, 95, 670)
$g.DrawLine($pen, 95, 670, 95, 635)

$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "saved: $Out"
