#!/usr/bin/env python3
"""Regenerates test_env.hdr: a tiny (64x32) synthetic equirectangular panorama, old-style flat
Radiance HDR (no RLE), for the ibl_control.json scenario's LoadEnvironmentHDR round-trip.
Deterministic -- running this reproduces test_env.hdr byte-for-byte. Identical to
CGApp/Universe/scenarios/models/gen_test_env.py (same fixture, kept as a separate per-app copy
per this repo's convention of not sharing scenario test assets across modules). Hue sweeps around
the horizon and brightness fades from a bright zenith to a dim nadir, so LoadEnvironmentHDR's
direction-to-UV mapping is visibly exercised (a flat/solid environment couldn't tell a working
equirect-to-cube conversion apart from a bug that just samples the same texel everywhere)."""
import math
import os

WIDTH, HEIGHT = 64, 32


def float_to_rgbe(r, g, b):
    v = max(r, g, b)
    if v < 1e-32:
        return (0, 0, 0, 0)
    m, e = math.frexp(v)
    scale = m * 256.0 / v
    return (
        min(255, max(0, int(r * scale))),
        min(255, max(0, int(g * scale))),
        min(255, max(0, int(b * scale))),
        e + 128,
    )


def main():
    rows = []
    for y in range(HEIGHT):
        v = y / (HEIGHT - 1)  # 0 at top (zenith) -> 1 at bottom (nadir)
        row = bytearray()
        for x in range(WIDTH):
            u = x / WIDTH  # 0..1 around the horizon
            hue = u * 2.0 * math.pi
            r = 0.5 + 0.5 * math.cos(hue)
            g = 0.5 + 0.5 * math.cos(hue - 2.09439)
            b = 0.5 + 0.5 * math.cos(hue + 2.09439)
            brightness = 3.0 * (1.0 - v) + 0.05
            row += bytes(float_to_rgbe(r * brightness, g * brightness, b * brightness))
        rows.append(row)

    out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "test_env.hdr")
    with open(out_path, "wb") as f:
        f.write(b"#?RADIANCE\n")
        f.write(b"FORMAT=32-bit_rle_rgbe\n\n")
        f.write(f"-Y {HEIGHT} +X {WIDTH}\n".encode("ascii"))
        for row in rows:
            f.write(row)
    print(f"wrote {out_path} ({WIDTH}x{HEIGHT})")


if __name__ == "__main__":
    main()
