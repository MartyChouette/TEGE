# Golden-image comparer. Pure stdlib (no PIL/numpy) — reads P6 PPM files
# written by the editor's --golden flag.
# Usage: python _golden_compare.py reference.ppm candidate.ppm [maxPctBad] [channelTol]
#   channelTol: per-channel abs difference below which a pixel is "same" (default 8)
#   maxPctBad:  max percentage of differing pixels allowed to PASS (default 1.0)
# Exit code 0 = PASS, 1 = FAIL, 2 = unreadable/mismatched inputs.
import sys


def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    # Tokenize the header: magic, width, height, maxval (comments start with #)
    tokens = []
    i = 0
    while len(tokens) < 4 and i < len(data):
        c = data[i:i + 1]
        if c.isspace():
            i += 1
        elif c == b"#":
            while i < len(data) and data[i:i + 1] != b"\n":
                i += 1
        else:
            j = i
            while j < len(data) and not data[j:j + 1].isspace():
                j += 1
            tokens.append(data[i:j])
            i = j
    i += 1  # single whitespace after maxval
    if len(tokens) < 4 or tokens[0] != b"P6":
        raise ValueError(f"{path}: not a P6 PPM")
    w, h, maxval = int(tokens[1]), int(tokens[2]), int(tokens[3])
    if maxval != 255:
        raise ValueError(f"{path}: unsupported maxval {maxval}")
    pixels = data[i:i + w * h * 3]
    if len(pixels) < w * h * 3:
        raise ValueError(f"{path}: truncated pixel data")
    return w, h, pixels


def main():
    if len(sys.argv) < 3:
        print("usage: _golden_compare.py reference.ppm candidate.ppm [maxPctBad] [channelTol]")
        return 2
    ref_path, cand_path = sys.argv[1], sys.argv[2]
    max_pct_bad = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    tol = int(sys.argv[4]) if len(sys.argv) > 4 else 8

    try:
        rw, rh, ref = read_ppm(ref_path)
        cw, ch, cand = read_ppm(cand_path)
    except (OSError, ValueError) as e:
        print(f"ERROR: {e}")
        return 2

    if (rw, rh) != (cw, ch):
        print(f"FAIL: dimension mismatch ref {rw}x{rh} vs candidate {cw}x{ch}")
        print("      (goldens are resolution-dependent: keep the editor window/layout")
        print("       the same as when the reference was recorded, or re-record)")
        return 1

    n = rw * rh
    bad = 0
    max_diff = 0
    diff_sum = 0
    for p in range(0, n * 3, 3):
        d = max(abs(ref[p] - cand[p]), abs(ref[p + 1] - cand[p + 1]), abs(ref[p + 2] - cand[p + 2]))
        if d > max_diff:
            max_diff = d
        diff_sum += d
        if d > tol:
            bad += 1

    pct_bad = 100.0 * bad / n
    mean_diff = diff_sum / n
    verdict = "PASS" if pct_bad <= max_pct_bad else "FAIL"
    print(f"{verdict}: {pct_bad:.3f}% pixels differ beyond tol {tol} "
          f"(allowed {max_pct_bad}%), maxDiff {max_diff}, meanDiff {mean_diff:.2f}, {rw}x{rh}")
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
