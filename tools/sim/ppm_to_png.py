#!/usr/bin/env python3
import argparse
from PIL import Image


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert RGB565 simulator PPM to crisp PNG.")
    parser.add_argument("input_ppm")
    parser.add_argument("output_png")
    parser.add_argument("--scale", type=int, default=1, help="Nearest-neighbor integer scale factor (1 = native 240x135)")
    args = parser.parse_args()

    img = Image.open(args.input_ppm)
    if args.scale > 1:
        img = img.resize((img.width * args.scale, img.height * args.scale), Image.NEAREST)
    img.save(args.output_png)
    print(f"Saved {args.output_png} ({img.width}x{img.height})")


if __name__ == "__main__":
    main()
