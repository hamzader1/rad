# Builds data/rad_icons.ttf: raddbg's icon font (data/icons.ttf, unchanged)
# plus the SVG icons in this folder, as glyphs on free ASCII codepoints. rad
# embeds the result (src/rad/rad.mdesk, @embed_file rad_icon_font_bytes) and
# names the glyphs in RAD_IconTable.
#
# Run from the repository root after changing an SVG (needs fonttools:
# `python -m pip install fonttools`):
#
#   python icons/make_rad_icons.py
#
# SVGs: 16x16 viewBox, filled paths (`fill="currentColor"`); the color comes
# from the theme at draw time. An entry may name one path of its SVG (by id). TrueType fills by the nonzero rule, so paths
# with fill-rule="evenodd" get their inner contours turned around.

import os
import re
import sys
import xml.etree.ElementTree as ET

from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.reverseContourPen import ReverseContourPen
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.svgLib.path import parse_path

# file -> (codepoint, glyph name); codepoints are ones icons.ttf leaves free
ICONS = [
    ('play.svg',  '2', 'rad_play'),
    ('stop.svg',  '%', 'rad_stop'),
    ('build.svg', '$', 'rad_build'),
    ('check.svg', '~', 'rad_check'),
    ('test.svg',  't', 'rad_test'),
    # completion item kinds (the user's, 2026-11-04)
    ('symbol-method.svg',      ',',  'rad_symbol_method'),
    ('symbol-field.svg',       '/',  'rad_symbol_field'),
    ('symbol-variable.svg',    ':',  'rad_symbol_variable'),
    ('symbol-class.svg',       ';',  'rad_symbol_class'),
    ('symbol-enum.svg',        '=',  'rad_symbol_enum'),
    ('symbol-enum-member.svg', '_',  'rad_symbol_enum_member'),
    ('symbol-constant.svg',    '|',  'rad_symbol_constant'),
    ('symbol-keyword.svg',     '\\', 'rad_symbol_keyword'),
    ('symbol-parameter.svg',   '`',  'rad_symbol_parameter'),
    # past ASCII: private-use codepoints (UTF-8 in RAD_IconTable)
    ('screwdriver.svg',        '\ue000', 'rad_screwdriver'),
    ('symbol-interface.svg',   '\ue001', 'rad_symbol_interface'),
    # the explorer's folder (the user's, 2026-11-06): two glyphs drawn on top
    # of each other, the fill at half opacity under the outline (one path
    # each, by id; the SVG's invisible canvas square is left out)
    ('Folder_16x.svg',         '\ue002', 'rad_folder_fill', 'path2'),
    ('Folder_16x.svg',         '\ue003', 'rad_folder',      'path3'),
]

# 16 SVG units -> 1060 font units (the font has 1000/em), the box centered
# on y = 350 like raddbg's icons (they span about -86..787)
SCALE = 1060.0/16.0
TOP = 350 + 530

def contours_from_recording(rec):
    contours, cur = [], []
    for op, args in rec.value:
        cur.append((op, args))
        if op in ('closePath', 'endPath'):
            contours.append(cur)
            cur = []
    if cur:
        contours.append(cur)
    return contours

def points_of(contour):
    pts = []
    for op, args in contour:
        pts.extend(args)
    return pts

def signed_area(pts):
    a = 0.0
    for i in range(len(pts)):
        x0, y0 = pts[i]
        x1, y1 = pts[(i+1) % len(pts)]
        a += x0*y1 - x1*y0
    return a/2

def point_in_polygon(p, poly):
    x, y = p
    inside = False
    for i in range(len(poly)):
        x0, y0 = poly[i]
        x1, y1 = poly[(i+1) % len(poly)]
        if (y0 > y) != (y1 > y):
            if x < x0 + (y - y0)*(x1 - x0)/(y1 - y0):
                inside = not inside
    return inside

def glyph_from_svg(path, path_id=None):
    root = ET.parse(path).getroot()
    out = RecordingPen()
    for el in root.iter():
        if not el.tag.endswith('path'):
            continue
        if path_id is not None and el.get('id') != path_id:
            continue
        rec = RecordingPen()
        parse_path(el.get('d'), TransformPen(rec, (SCALE, 0, 0, -SCALE, 0, TOP)))
        contours = contours_from_recording(rec)
        evenodd = (el.get('fill-rule') == 'evenodd')
        polys = [points_of(c) for c in contours]
        for idx, contour in enumerate(contours):
            reverse = False
            if evenodd:
                # rad: depth = how many other contours of this path hold it;
                # odd depth = a hole: it must turn the other way
                depth = sum(1 for j, other in enumerate(polys) if j != idx and point_in_polygon(polys[idx][0], other))
                want_clockwise = (depth % 2 == 0)
                is_clockwise = signed_area(polys[idx]) < 0
                reverse = (want_clockwise != is_clockwise)
            sub = RecordingPen()
            sub.value = contour
            sub.replay(ReverseContourPen(out) if reverse else out)
    tt = TTGlyphPen(None)
    out.replay(Cu2QuPen(tt, max_err=1.0, reverse_direction=False))
    return tt.glyph()

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)
    font = TTFont(os.path.join(repo, 'data', 'icons.ttf'))
    cmap = font.getBestCmap()
    order = font.getGlyphOrder()
    for file_name, char, glyph_name, *path_id in ICONS:
        assert ord(char) not in cmap, 'codepoint already used: ' + char
        glyph = glyph_from_svg(os.path.join(here, file_name), *path_id)
        glyph.recalcBounds(font['glyf'])
        order.append(glyph_name)
        font['glyf'].glyphs[glyph_name] = glyph
        font['hmtx'].metrics[glyph_name] = (int(round(16*SCALE)), getattr(glyph, 'xMin', 0))
        for table in font['cmap'].tables:
            if table.isUnicode():
                table.cmap[ord(char)] = glyph_name
    font.setGlyphOrder(order)
    font['glyf'].glyphOrder = order
    font['maxp'].numGlyphs = len(order)
    out_path = os.path.join(repo, 'data', 'rad_icons.ttf')
    font.save(out_path)
    print('wrote', out_path, '(%d glyphs)' % len(order))

if __name__ == '__main__':
    main()
