# Generates demo-track2.png — circuit-diagram-style track.
#
# Route (start on the bottom-left square, heading up / angle -90):
#   start square -> sine squiggle up -> loop circle (crossed) ->
#   top connector -> zigzag column down -> bottom connector ->
#   diamond (solid left side, dotted right side) -> two overlapping
#   circles (crossed) -> top corner -> down through the dashed
#   rectangle -> end square.
param([string]$Out = "$PSScriptRoot\demo-track2.png")

Add-Type -AssemblyName System.Drawing

$W = 700; $H = 700
$bmp = New-Object System.Drawing.Bitmap($W, $H)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
$g.Clear([System.Drawing.Color]::White)

$black = [System.Drawing.Color]::Black
$brush = New-Object System.Drawing.SolidBrush($black)

function NewPen($width) {
    $p = New-Object System.Drawing.Pen([System.Drawing.Color]::Black, $width)
    $p.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $p.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $p.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    return $p
}
$pen = NewPen 11

# ── Start square (90 px, robot starts at its center heading up) ─────────────
$g.FillRectangle($brush, 55, 585, 90, 90)

# ── Sine squiggle up the left column ────────────────────────────────────────
$pts = New-Object System.Collections.Generic.List[System.Drawing.PointF]
for ($y = 585; $y -ge 190; $y -= 4) {
    $x = 100 + 26 * [math]::Sin(2 * [math]::PI * (585 - $y) / 85.0)
    $pts.Add([System.Drawing.PointF]::new($x, $y))
}
$g.DrawLines($pen, $pts.ToArray())

# ── Loop at the top: circle crossed by the continuing line ──────────────────
$g.DrawLine($pen, 100, 190, 100, 60)
$g.DrawEllipse($pen, 52, 92, 96, 96)        # center (100,140) r48

# ── Top connector to the zigzag ─────────────────────────────────────────────
$g.DrawLine($pen, 100, 60, 300, 60)

# ── Zigzag column ───────────────────────────────────────────────────────────
$zig = @(
    [System.Drawing.PointF]::new(300,  60),
    [System.Drawing.PointF]::new(340, 140),
    [System.Drawing.PointF]::new(260, 220),
    [System.Drawing.PointF]::new(340, 300),
    [System.Drawing.PointF]::new(260, 380),
    [System.Drawing.PointF]::new(340, 460),
    [System.Drawing.PointF]::new(260, 540),
    [System.Drawing.PointF]::new(300, 620)
)
$g.DrawLines($pen, $zig)

# ── Bottom connector, then up into the diamond ──────────────────────────────
$g.DrawLine($pen, 300, 620, 450, 620)
$g.DrawLine($pen, 450, 620, 450, 535)

# ── Diamond: path follows the LEFT side; right side is sparse dots ──────────
$g.DrawLine($pen, 450, 535, 395, 480)      # bottom -> left vertex
$g.DrawLine($pen, 395, 480, 450, 425)      # left vertex -> top
$dpen = NewPen 6
$dpen.DashPattern = @(0.5, 3.5)
$g.DrawLine($dpen, 450, 535, 505, 480)     # dotted: bottom -> right
$g.DrawLine($dpen, 505, 480, 450, 425)     # dotted: right -> top

# ── Two overlapping circles (transformer), crossed vertically ───────────────
$g.DrawLine($pen, 450, 425, 450, 180)
$g.DrawEllipse($pen, 405, 310, 90, 90)     # center (450,355) r45
$g.DrawEllipse($pen, 405, 240, 90, 90)     # center (450,285) r45

# ── Top corner, then down through the dashed rectangle ──────────────────────
$g.DrawLine($pen, 450, 180, 580, 180)
$g.DrawLine($pen, 580, 180, 580, 430)
$rpen = NewPen 8
$rpen.DashPattern = @(1.6, 1.2)
$g.DrawRectangle($rpen, 540, 210, 80, 190)  # x 540..620, y 210..400

# ── End square ──────────────────────────────────────────────────────────────
$g.FillRectangle($brush, 535, 430, 90, 90)

$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "saved: $Out"
