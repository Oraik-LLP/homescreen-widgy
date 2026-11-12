"""Compose exported app surfaces on a neutral backdrop for visual review."""
from PIL import Image, ImageDraw
from pathlib import Path
import struct
import sys

root = Path(sys.argv[1])
prefix = sys.argv[2] if len(sys.argv) > 2 else 'preview'
bg = (24, 36, 30)
images = []
for i in range(3):
    path = root / f'{prefix}-{i}.bmp'
    if not path.exists():
        continue
    data = path.read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    source = data[offset:]
    pixels = bytearray(width * abs(height) * 3)
    for n in range(width * abs(height)):
        b, g, r, a = source[n*4:n*4+4]
        for k, v in enumerate((r,g,b)):
            pixels[n*3+k] = min(255, v + bg[k] * (255-a) // 255)
    im = Image.frombytes('RGB', (width, abs(height)), bytes(pixels))
    if height > 0:
        im = im.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    im.save(path.with_suffix('.png'))
    images.append((i, im))
canvas = Image.new('RGB', (1300, 900), bg)
draw = ImageDraw.Draw(canvas)
draw.text((48, 34), 'PANES  /  NATIVE WINDOWS APP', fill='#91B89D')
positions = {0: (55,100), 1: (0,385), 2: (790,275)}
for i, im in images:
    if i == 1 and im.width > 740:
        im.thumbnail((740,180))
    if i == 2:
        im.thumbnail((450,540))
    canvas.paste(im, positions[i])
draw.text((48,840), 'Clock  /  Date  /  Calendar    -    Thin and Compact presets', fill='#91B89D')
canvas.save(root / f'{prefix}-overview.png')
