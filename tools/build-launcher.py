#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Render launcher artwork from the original Sopwith sprites, terrain and font."""
import argparse
import ast
import math
from pathlib import Path
import re
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
FRAMES = 48


class Bitmap:
    def __init__(self, width, height):
        self.width, self.height = width, height
        self.pixels = bytearray([1]) * (width * height)

    def pixel(self, x, y, color=0):
        if 0 <= x < self.width and 0 <= y < self.height:
            self.pixels[y * self.width + x] = color

    def rect(self, x, y, width, height, color=0):
        for row in range(y, y + height):
            for col in range(x, x + width):
                self.pixel(col, row, color)

    def rounded(self, x, y, width, height, radius, color=0):
        for row in range(y, y + height):
            for col in range(x, x + width):
                dx = max(x + radius - col, 0, col - (x + width - 1 - radius))
                dy = max(y + radius - row, 0, row - (y + height - 1 - radius))
                if dx * dx + dy * dy <= radius * radius:
                    self.pixel(col, row, color)

    def paste(self, other, x, y):
        for row in range(other.height):
            for col in range(other.width):
                self.pixel(x + col, y + row, other.pixels[row * other.width + col])

    def png(self):
        stride = (self.width + 7) // 8
        rows = bytearray()
        for y in range(self.height):
            packed = bytearray(stride)
            for x in range(self.width):
                if self.pixels[y * self.width + x]:
                    packed[x // 8] |= 0x80 >> (x % 8)
            rows.append(0)  # PNG filter: none.
            rows.extend(packed)

        def chunk(kind, data):
            return (struct.pack('>I', len(data)) + kind + data
                    + struct.pack('>I', zlib.crc32(kind + data)))

        header = struct.pack('>IIBBBBB', self.width, self.height, 1, 0, 0, 0, 0)
        return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header)
                + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))


def without_comments(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)


def load_art():
    symbols = (ROOT / 'Sources/core/swsymbol.c').read_text()

    def sprite_array(name):
        block = symbols.split(f'static const char *{name}[] = {{', 1)[1].split('};', 1)[0]
        result = []
        for entry in without_comments(block).split(','):
            strings = re.findall(r'"(?:[^"\\]|\\.)*"', entry)
            if strings:
                text = ''.join(ast.literal_eval(value) for value in strings)
                result.append([[" *-#".index(c) for c in row[::2]]
                               for row in text.splitlines()])
            elif entry.strip():
                result.append(None)  # Named custom symbols are not used here.
        return result

    font = without_comments((ROOT / 'Sources/core/font.h').read_text(encoding='latin1'))
    font = font.split('font_data[GFX_FONTDATAMAX] = {', 1)[1].split('};', 1)[0]
    font = bytes(int(value, 16) for value in re.findall(r'0x[0-9a-fA-F]+', font))
    if len(font) != 2048:
        raise ValueError('Expected the upstream 256-character 8x8 font')
    ground = without_comments((ROOT / 'Sources/core/swgames.c').read_text())
    ground = ground.split('GRNDTYPE original_ground[] =', 1)[1].split('};', 1)[0]
    ground = [int(value) for value in re.findall(r'\d+', ground)]
    planes, buildings = sprite_array('swplnsym'), sprite_array('swtrgsym')
    if len(planes) != 4 or len(ground) < 700:
        raise ValueError('Unexpected upstream sprite/terrain layout')
    return planes, buildings, font, ground


def sprite(canvas, data, x, y, scale=1):
    for row, values in enumerate(data):
        for col, value in enumerate(values):
            if not value:
                continue
            # Preserve the silhouette, with fixed local hatching for index 2.
            for dy in range(scale):
                for dx in range(scale):
                    white = value == 2 and (col * scale + dx + row * scale + dy) % 2
                    canvas.pixel(x + col * scale + dx, y + row * scale + dy, int(bool(white)))


def plane_at(planes, angle):
    # Same four base poses and quarter-turn transform as swsymbol.c / swmove.c.
    data = planes[angle % 4]
    for _ in range(angle // 4):
        data = [list(row) for row in zip(*data)][::-1]
    return data


def label(canvas, font, text, x, y, scale=1, color=0):
    for index, char in enumerate(text):
        for row, bits in enumerate(font[ord(char) * 8:ord(char) * 8 + 8]):
            for col in range(8):
                if bits & (0x80 >> col):
                    canvas.rect(x + (index * 8 + col) * scale, y + row * scale,
                                scale, scale, color)


def render_assets():
    planes, buildings, font, ground = load_art()
    base = Bitmap(350, 155)
    base.rounded(0, 0, 350, 155, 10)
    base.rounded(3, 3, 344, 149, 8, 1)
    base.rect(164, 3, 180, 149)
    label(base, font, 'SOPWITH', 170, 56, scale=3, color=1)
    label(base, font, 'BIPLANE COMBAT', 202, 94, color=1)

    # Scale a section of the original landscape into the left panel.
    heights = [137 - (ground[330 + x * 2] - 26) // 4 for x in range(156)]
    for x, top in enumerate(heights, 4):
        base.rect(x, top, 1, 151 - top)
        for y in range(top + 4, 148):
            if (x + 2 * y) % 8 == 0:
                base.pixel(x, y, 1)
    for x, target in ((13, 0), (64, 0), (117, 7)):
        # A level foundation lets buildings sit on sloping original terrain.
        top = min(heights[x - 4:x + 28])
        base.rect(x, top, 32, 3)
        sprite(base, buildings[target], x, top - 32, scale=2)

    cards, icons = [], []
    for frame in range(FRAMES):
        phase = 2 * math.pi * frame / FRAMES
        angle = (frame * 16 + FRAMES // 2) // FRAMES % 16
        plane = plane_at(planes, angle)
        card = Bitmap(350, 155)
        card.paste(base, 0, 0)
        x, y = round(81 + 34 * math.sin(phase)), round(54 + 34 * math.cos(phase))
        sprite(card, plane, x - 16, y - 16, scale=2)
        cards.append(card)

        icon = Bitmap(32, 32)
        icon.rounded(0, 0, 32, 32, 5)
        icon.rounded(2, 2, 28, 28, 4, 1)
        x, y = round(16 + 5 * math.sin(phase)), round(16 + 5 * math.cos(phase))
        sprite(icon, plane, x - 8, y - 8)
        icons.append(icon)

    assets = {'card.png': cards[0].png(), 'icon.png': icons[0].png()}
    for directory, frames in (('card-highlighted', cards), ('icon-highlighted', icons)):
        for number, bitmap in enumerate(frames, 1):
            assets[f'{directory}/{number}.png'] = bitmap.png()
        # No loopCount: the selected artwork loops indefinitely.
        sequence = ', '.join(f'{number}x2' for number in range(1, FRAMES + 1))
        assets[f'{directory}/animation.txt'] = f'frames = {sequence}\n'.encode()
    loading = Bitmap(400, 240)
    loading.paste(cards[0], 25, 43)
    assets['launchImage.png'] = loading.png()
    return assets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Check checked-in assets without writing')
    parser.add_argument('--output', type=Path, default=ROOT / 'Source/launcher')
    args = parser.parse_args()
    assets = render_assets()
    if args.check:
        actual = {p.relative_to(args.output).as_posix(): p.read_bytes()
                  for p in args.output.rglob('*') if p.is_file()}
        different = sorted(name for name in actual.keys() | assets.keys()
                           if actual.get(name) != assets.get(name))
        if different:
            parser.error('Launcher assets need regeneration: ' + ', '.join(different))
    else:
        # Prune only old numbered frames; keep unrelated files untouched.
        for directory in ('card-highlighted', 'icon-highlighted'):
            for path in (args.output / directory).glob('*.png'):
                if path.stem.isdigit() and path.relative_to(args.output).as_posix() not in assets:
                    path.unlink()
        for name, data in assets.items():
            path = args.output / name
            path.parent.mkdir(parents=True, exist_ok=True)
            if not path.exists() or path.read_bytes() != data:
                path.write_bytes(data)
    print(f'{"Verified" if args.check else "Rendered"} {len(assets)} launcher files in {args.output}')


if __name__ == '__main__':
    main()
