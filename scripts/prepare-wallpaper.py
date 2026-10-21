#!/usr/bin/env python3
"""Convert a local photo to Daybook's 600x800 grayscale wallpaper format."""
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('photo', type=Path)
    parser.add_argument('output', type=Path, help='Output wallpaper.pgm, preferably in a local work folder')
    args = parser.parse_args()
    if args.photo.resolve() == args.output.resolve():
        parser.error('Choose a separate output file to preserve the original photo.')
    if args.output.exists():
        parser.error('Output already exists; choose another filename or remove it deliberately.')
    try:
        from PIL import Image, ImageOps
    except ImportError:
        parser.error('Pillow is required. Install it with: python3 -m pip install Pillow')
    with Image.open(args.photo) as image:
        image = ImageOps.fit(ImageOps.exif_transpose(image).convert('L'), (600, 800), method=Image.Resampling.LANCZOS)
        with args.output.open('xb') as target:
            target.write(b'P5\n600 800\n255\n')
            target.write(image.tobytes())
    print(f'Wallpaper ready: {args.output}. Copy it to daybook/data/wallpaper.pgm while Daybook is closed.')


if __name__ == '__main__':
    main()
