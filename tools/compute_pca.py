#!/usr/bin/env python3
"""Compute demo PCA parameters for FaceRecognizeESP."""

from __future__ import annotations

import argparse
import pathlib
from typing import Iterable, List

import numpy as np
from sklearn.decomposition import PCA


try:
    import cv2  # type: ignore
except Exception:  # pragma: no cover - cv2 is optional
    cv2 = None


IMG_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".pgm"}


def _iter_images(folder: pathlib.Path) -> Iterable[pathlib.Path]:
    if not folder.exists():
        return []
    return sorted(
        p for p in folder.rglob("*") if p.is_file() and p.suffix.lower() in IMG_EXTENSIONS
    )


def _load_image_gray(path: pathlib.Path, face_size: int) -> np.ndarray:
    if cv2 is not None:
        # Workaround for Windows paths with non-ASCII characters
        img_array = np.fromfile(str(path), dtype=np.uint8)
        img = cv2.imdecode(img_array, cv2.IMREAD_GRAYSCALE)
        if img is None:
            raise ValueError(f"Failed to read image: {path}")
        img = cv2.resize(img, (face_size, face_size), interpolation=cv2.INTER_LINEAR)
        return img.astype(np.uint8)

    # Fallback: npy files containing grayscale arrays.
    if path.suffix.lower() == ".npy":
        arr = np.load(path)
        if arr.ndim != 2:
            raise ValueError(f"Unsupported npy shape for {path}: {arr.shape}")
        return arr.astype(np.uint8)

    raise RuntimeError(
        "opencv-python is not available. Install it or provide .npy grayscale arrays."
    )


def lbp_histogram(gray: np.ndarray) -> np.ndarray:
    h, w = gray.shape
    hist = np.zeros(256, dtype=np.float32)
    if h < 3 or w < 3:
        return hist

    for y in range(1, h - 1):
        row_up = gray[y - 1]
        row = gray[y]
        row_dn = gray[y + 1]
        for x in range(1, w - 1):
            c = row[x]
            code = 0
            code |= (1 if row_up[x - 1] >= c else 0) << 7
            code |= (1 if row_up[x] >= c else 0) << 6
            code |= (1 if row_up[x + 1] >= c else 0) << 5
            code |= (1 if row[x + 1] >= c else 0) << 4
            code |= (1 if row_dn[x + 1] >= c else 0) << 3
            code |= (1 if row_dn[x] >= c else 0) << 2
            code |= (1 if row_dn[x - 1] >= c else 0) << 1
            code |= (1 if row[x - 1] >= c else 0) << 0
            hist[code] += 1.0

    s = float(hist.sum())
    if s > 0:
        hist /= s
    return hist


def _format_array(name: str, arr: np.ndarray, indent: str = "  ") -> str:
    lines = [f"static const float {name}[{arr.shape[0]}] = {{"]
    for i in range(arr.shape[0]):
        tail = "," if i + 1 < arr.shape[0] else ""
        lines.append(f"{indent}{arr[i]:.8f}f{tail}")
    lines.append("};")
    return "\n".join(lines)


def _format_matrix(name: str, mat: np.ndarray, rows: int, cols: int) -> str:
    lines = [f"static const float {name}[FEATURE_DIM][256] = {{"]
    for r in range(rows):
        lines.append("  {")
        for c in range(cols):
            tail = "," if c + 1 < cols else ""
            lines.append(f"    {mat[r, c]:.8f}f{tail}")
        row_tail = "," if r + 1 < rows else ""
        lines.append(f"  }}{row_tail}")
    lines.append("};")
    return "\n".join(lines)


def write_header(out_path: pathlib.Path, mean: np.ndarray, matrix: np.ndarray, dim: int) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    content = [
        "#pragma once",
        "",
        f"#define FEATURE_DIM {dim}",
        "",
        _format_array("LBP_MEAN", mean.astype(np.float32)),
        "",
        _format_matrix("PCA_MATRIX", matrix.astype(np.float32), dim, 256),
        "",
    ]
    out_path.write_text("\n".join(content), encoding="utf-8")


def build_histograms(dataset: pathlib.Path, face_size: int) -> np.ndarray:
    images = list(_iter_images(dataset))
    histograms: List[np.ndarray] = []

    for path in images:
        gray = _load_image_gray(path, face_size)
        histograms.append(lbp_histogram(gray))

    if not histograms:
        rng = np.random.default_rng(7)
        for _ in range(64):
            synthetic = rng.integers(0, 256, size=(face_size, face_size), dtype=np.uint8)
            histograms.append(lbp_histogram(synthetic))

    return np.stack(histograms, axis=0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, help="Input dataset folder")
    parser.add_argument("--dim", type=int, default=100, help="PCA output dimensionality")
    parser.add_argument("--out", required=True, help="Output header path")
    parser.add_argument("--face-size", type=int, default=96)
    args = parser.parse_args()

    dataset = pathlib.Path(args.input)
    dim = int(args.dim)
    out = pathlib.Path(args.out)

    hists = build_histograms(dataset, args.face_size)
    n_samples = hists.shape[0]
    dim = max(1, min(dim, 256, n_samples))

    pca = PCA(n_components=dim, whiten=False, random_state=7)
    pca.fit(hists)

    mean = pca.mean_
    matrix = pca.components_

    if dim < args.dim:
        padding = np.zeros((args.dim - dim, 256), dtype=np.float32)
        matrix = np.vstack([matrix, padding])
        dim = args.dim

    write_header(out, mean, matrix, dim)
    print(f"Wrote {out} with FEATURE_DIM={dim}")


if __name__ == "__main__":
    main()