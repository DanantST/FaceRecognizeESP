#!/usr/bin/env python3
"""Sanity test for generated PCA header."""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys
import tempfile


def parse_feature_dim(header: str) -> int:
    m = re.search(r"#define\s+FEATURE_DIM\s+(\d+)", header)
    if not m:
        raise ValueError("FEATURE_DIM macro not found")
    return int(m.group(1))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dim", type=int, default=16)
    args = parser.parse_args()

    repo = pathlib.Path(__file__).resolve().parents[1]
    script = repo / "tools" / "compute_pca.py"

    with tempfile.TemporaryDirectory() as tmp:
        out = pathlib.Path(tmp) / "pca_params.h"
        cmd = [
            sys.executable,
            str(script),
            "--input",
            str(pathlib.Path(tmp) / "dataset"),
            "--dim",
            str(args.dim),
            "--out",
            str(out),
        ]
        subprocess.check_call(cmd)
        text = out.read_text(encoding="utf-8")

    feature_dim = parse_feature_dim(text)
    if feature_dim != args.dim:
        raise SystemExit(f"Expected FEATURE_DIM={args.dim}, got {feature_dim}")

    if "LBP_MEAN[256]" not in text or "PCA_MATRIX[FEATURE_DIM][256]" not in text:
        raise SystemExit("Required arrays not found")

    print("PCA header sanity test passed")


if __name__ == "__main__":
    main()