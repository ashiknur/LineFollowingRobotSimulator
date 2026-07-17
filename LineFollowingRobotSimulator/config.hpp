#pragma once

constexpr unsigned WIDTH = 700;
constexpr unsigned HEIGHT = 700;
constexpr int THRESHOLD = 120;

constexpr float LINE_WIDTH = 8.f;

// Distance between the two wheels (px). The robot sprite is 220x220 scaled
// by 0.25 (= 55 px); the wheels sit near the sprite's edges.
constexpr float WHEEL_BASE = 44.f;
