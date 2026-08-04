# Master map renderer: 4 island panels, campaign targets + routes annotated.
import json, math, datetime
from PIL import Image, ImageDraw, ImageFont

D = json.load(open('mastermap.json', encoding='utf-8'))
FONT = r'C:\Windows\Fonts\consola.ttf'
f10 = ImageFont.truetype(FONT, 15)
f12 = ImageFont.truetype(FONT, 18)
f14 = ImageFont.truetype(FONT, 21)
f16 = ImageFont.truetype(FONT, 24)
f22 = ImageFont.truetype(FONT, 34)

MAP_PX = 1080          # island panel raster
IDX_W  = 470           # per-panel index column
PANEL_W = MAP_PX + IDX_W + 30
PANEL_H = MAP_PX + 70
EXT = 5000.0

INK      = (28, 32, 24)
INK2     = (90, 96, 84)
RED      = (196, 32, 24)     # enemy / target
BLUE     = (26, 82, 168)     # friendly / no-fire
ORANGE   = (216, 122, 12)    # route (moving target)
PURPLE   = (122, 40, 160)    # phase lines / planning
PAPER    = (246, 244, 236)

def elev_color(h):
    if h < -8:  return (105, 146, 168)
    if h < 0:   return (146, 182, 198)
    if h < 3:   return (222, 214, 178)          # beach
    if h < 25:  return (168, 190, 140)
    if h < 60:  return (146, 172, 122)
    if h < 100: return (162, 158, 122)
    if h < 140: return (158, 144, 116)
    return (170, 162, 150)

def render_island(isl):
    H = isl['h']; N = len(H)
    img = Image.new('RGB', (N, N))
    px = img.load()
    # hillshade: light from NW
    for j in range(N):
        for i in range(N):
            h = H[j][i]
            c = elev_color(h)
            if h >= 0:
                hx = H[j][min(i + 1, N - 1)] - H[j][max(i - 1, 0)]
                hz = H[min(j + 1, N - 1)][i] - H[max(j - 1, 0)][i]
                shade = max(-1.0, min(1.0, (hx + hz) / 24.0))
                k = 1.0 - 0.22 * shade
                c = tuple(max(0, min(255, int(v * k))) for v in c)
            px[i, j] = c
    return img.resize((MAP_PX, MAP_PX), Image.LANCZOS)

def w2p(x, z):
    return ((x + EXT) / (2 * EXT) * MAP_PX, (z + EXT) / (2 * EXT) * MAP_PX)

def grid_of(isl, x, z):
    e = isl['map']['originE'] + x + EXT
    n = isl['map']['originN'] + EXT - z
    return f"{int(e // 100):03d} {int(n // 100):03d}"

def diamond(d, p, r, fill):
    x, y = p
    d.polygon([(x, y - r), (x + r, y), (x, y + r), (x - r, y)], fill=fill, outline=PAPER)

def square(d, p, r, fill):
    x, y = p
    d.rectangle([x - r, y - r, x + r, y + r], fill=fill, outline=PAPER)

def arrow(d, p0, p1, color, w=4):
    d.line([p0, p1], fill=color, width=w)
    ang = math.atan2(p1[1] - p0[1], p1[0] - p0[0])
    for s in (-1, 1):
        d.line([p1, (p1[0] - 14 * math.cos(ang + s * 0.45),
                     p1[1] - 14 * math.sin(ang + s * 0.45))], fill=color, width=w)

placed = []
def label(d, p, text, color=INK):
    # greedy offset placement dodging previously placed labels
    tw = d.textlength(text, font=f12); th = 20
    cands = [(12, -24), (12, 10), (-tw - 12, -24), (-tw - 12, 10),
             (12, -46), (-tw - 12, -46), (14, 30), (-tw - 14, 30)]
    for dx, dy in cands:
        box = (p[0] + dx - 3, p[1] + dy - 2, p[0] + dx + tw + 3, p[1] + dy + th)
        if box[0] < 4 or box[1] < 4 or box[2] > MAP_PX - 4 or box[3] > MAP_PX - 4:
            continue
        if any(not (box[2] < b[0] or box[0] > b[2] or box[3] < b[1] or box[1] > b[3]) for b in placed):
            continue
        placed.append(box)
        d.rectangle(box, fill=(255, 255, 252))
        d.rectangle(box, outline=color)
        d.text((p[0] + dx, p[1] + dy), text, font=f12, fill=color)
        return
    d.text((p[0] + 10, p[1] - 20), text, font=f12, fill=color)   # last resort

ISLAND_TITLES = {
    '1337': 'ISLAND 1337 — FOREWORD · VOLUME I · EPILOGUE',
    '9021': 'ISLAND 9021 — VOLUME II  "THE RIDGE LINE"',
    '5150': 'ISLAND 5150 — VOLUME III "THUNDER RUN"',
    '66600-black': 'ISLAND 66600 — VOLUME IV "BLACK SAND" (black-sand palette in game)',
}
ORDER = ['1337', '9021', '5150', '66600-black']

sheet = Image.new('RGB', (2 * PANEL_W + 60, 2 * PANEL_H + 210), PAPER)
sd = ImageDraw.Draw(sheet)
sd.text((30, 18), 'SHITFIRE — MASTER ANSWER KEY: ALL CAMPAIGN TARGETS & ROUTES', font=f22, fill=INK)
sd.text((30, 62), 'Fixed-seed chapters only — SKIRMISH targets are randomized per mission and are not plottable. '
                  'Generated from the live simulator, ' + datetime.date.today().isoformat() + '.', font=f12, fill=INK2)

for idx, key in enumerate(ORDER):
    isl = D['islands'][key]
    chapters = [c for c in D['chapters'] if c['island'] == key]
    panel = Image.new('RGB', (PANEL_W, PANEL_H), PAPER)
    base = render_island(isl)
    panel.paste(base, (0, 50))
    d = ImageDraw.Draw(panel)
    placed.clear()
    d.text((6, 10), ISLAND_TITLES.get(key, key), font=f16, fill=INK)

    md = ImageDraw.Draw(panel)
    off = 50   # map top offset inside panel

    def MP(x, z):
        p = w2p(x, z); return (p[0], p[1] + off)

    # 1-km grid + labels
    for k in range(-5, 6):
        x = k * 1000.0
        p0 = MP(x, -EXT); p1 = MP(x, EXT)
        md.line([p0, p1], fill=(255, 255, 255, 60), width=1)
        p0 = MP(-EXT, x); p1 = MP(EXT, x)
        md.line([p0, p1], fill=(255, 255, 255, 60), width=1)
    for k in range(-4, 5, 2):
        ekm = int((isl['map']['originE'] + k * 1000 + EXT) // 1000)
        nkm = int((isl['map']['originN'] + EXT - k * 1000) // 1000)
        md.text((MP(k * 1000, -EXT)[0] - 8, off + 4), f'{ekm:02d}', font=f10, fill=INK2)
        md.text((6, MP(-EXT, k * 1000)[1] - 8), f'{nkm:02d}', font=f10, fill=INK2)

    # roads / villages / facilities
    for r in isl['roads']:
        md.line([MP(x, z) for x, z in r], fill=(124, 106, 82), width=3)
    for v in isl['villages']:
        p = MP(v['x'], v['z'])
        rr = max(10, v['r'] / (2 * EXT) * MAP_PX)
        md.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], outline=BLUE, width=3)
        md.text((p[0] - md.textlength(v['name'], font=f10) / 2, p[1] + rr + 2), v['name'], font=f10, fill=BLUE)
    for f in isl['facilities']:
        p = MP(f['x'], f['z'])
        md.rectangle([p[0] - 4, p[1] - 4, p[0] + 4, p[1] + 4], outline=INK2, width=2)
    # OP + battery
    p = MP(isl['op']['x'], isl['op']['z'])
    md.ellipse([p[0] - 9, p[1] - 9, p[0] + 9, p[1] + 9], outline=BLUE, width=4)
    md.text((p[0] + 12, p[1] - 10), 'OP', font=f12, fill=BLUE)
    if isl['battery']:
        p = MP(isl['battery']['x'], isl['battery']['z'])
        md.text((p[0] - 8, p[1] - 12), '\u2295', font=f16, fill=BLUE)

    # chapter annotations
    index_rows = []
    for c in chapters:
        g = c['geo']; cid = c['id']; typ = g['type'].upper()
        todTag = '' if c['tod'] == 'day' else f" ({c['tod']})"
        if 'route' in g:
            pts = [MP(x, z) for x, z in g['route']]
            for a, b in zip(pts, pts[1:]):
                md.line([a, b], fill=ORANGE, width=5)
            arrow(md, pts[-2] if len(pts) > 1 else pts[0], pts[-1], ORANGE, 5)
            md.ellipse([pts[0][0] - 6, pts[0][1] - 6, pts[0][0] + 6, pts[0][1] + 6], fill=ORANGE, outline=PAPER)
            if 'stop' in g:
                sp = MP(*g['stop'])
                md.ellipse([sp[0] - 6, sp[1] - 6, sp[0] + 6, sp[1] + 6], fill=PAPER, outline=ORANGE, width=3)
            mid = pts[len(pts) // 2]
            label(md, mid, f'{cid} {typ} ROUTE', ORANGE)
            index_rows.append((cid, f'{typ} — route, start GRID {grid_of(isl, *g["route"][0])}'
                                    + (', pit stop marked' if 'stop' in g else '')))
        elif 'avenues' in g and g.get('fireBase'):
            fb = MP(*g['fireBase'])
            names = ['RED', 'WHITE', 'BLUE']
            for i2, av in enumerate(g['avenues']):
                p2 = MP(*av)
                md.line([(p2[0] - 12, p2[1]), (p2[0] + 12, p2[1])], fill=PURPLE, width=5)
                md.text((p2[0] + 14, p2[1] - 10), names[i2] if len(g['avenues']) == 3 else f'AV{i2+1}', font=f10, fill=PURPLE)
            if g.get('enemy'):
                en = MP(*g['enemy'])
                arrow(md, en, fb, PURPLE, 4)
            square(md, fb, 9, BLUE)
            label(md, fb, f'{cid} {typ} PHASE LINES{todTag}', PURPLE)
            index_rows.append((cid, f'QFP defense — position GRID {grid_of(isl, *g["fireBase"])}, lines RED/WHITE/BLUE in depth'))
        else:
            en = g.get('enemy')
            if g.get('compound'):
                square(md, MP(*g['compound']), 9, BLUE)
            if g.get('bbq'):
                square(md, MP(*g['bbq']), 9, BLUE)
            if g.get('fStart'):
                fs = MP(*g['fStart'])
                square(md, fs, 9, BLUE)
                if en: arrow(md, fs, MP(*en), BLUE, 3)
            if en:
                p2 = MP(*en)
                diamond(md, p2, 11, RED)
                label(md, p2, f'{cid} {typ}{todTag}', RED)
                extra = ''
                if g.get('compound'): extra = ' (friendly compound adjacent)'
                if g.get('village'):  extra = ' (no-strike village adjacent)'
                if g.get('bbq'):      extra = ' (no-fire cooks adjacent)'
                index_rows.append((cid, f'{typ}{todTag} — target GRID {grid_of(isl, *en)}{extra}'))

    # index column
    ix = MAP_PX + 16
    md.text((ix, 56), 'ANSWER KEY', font=f14, fill=INK)
    y = 92
    for cid, txt in sorted(index_rows, key=lambda r: r[0]):
        md.text((ix, y), cid, font=f12, fill=INK)
        # wrap
        words = txt.split(' ')
        line = ''
        yy = y
        for w in words:
            t = (line + ' ' + w).strip()
            if md.textlength(t, font=f10) > IDX_W - 76:
                md.text((ix + 62, yy), line, font=f10, fill=INK2); yy += 20; line = w
            else:
                line = t
        md.text((ix + 62, yy), line, font=f10, fill=INK2)
        y = yy + 30

    cx = 30 + (idx % 2) * (PANEL_W + 30)
    cy = 100 + (idx // 2) * (PANEL_H + 30)
    sheet.paste(panel, (cx, cy))

# legend strip
ly = 2 * PANEL_H + 140
sd.text((30, ly), 'LEGEND:', font=f14, fill=INK)
x0 = 160
diamond(sd, (x0, ly + 12), 11, RED);            sd.text((x0 + 20, ly), 'target', font=f12, fill=INK)
square(sd, (x0 + 130, ly + 12), 9, BLUE);       sd.text((x0 + 148, ly), 'friendly / no-fire', font=f12, fill=INK)
sd.line([(x0 + 360, ly + 12), (x0 + 420, ly + 12)], fill=ORANGE, width=5)
sd.text((x0 + 430, ly), 'route (moving target; hollow dot = pit stop)', font=f12, fill=INK)
sd.line([(x0 + 940, ly + 12), (x0 + 980, ly + 12)], fill=PURPLE, width=5)
sd.text((x0 + 990, ly), 'phase lines / planning', font=f12, fill=INK)
sd.ellipse([x0 + 1280, ly + 4, x0 + 1298, ly + 22], outline=BLUE, width=4)
sd.text((x0 + 1306, ly), 'OP tower', font=f12, fill=INK)
sd.ellipse([x0 + 1450, ly + 4, x0 + 1466, ly + 20], outline=BLUE, width=3)
sd.text((x0 + 1474, ly), 'village (no-strike)', font=f12, fill=INK)
sd.text((x0 + 1720, ly), 'grid: 1 km lines, labels in km (map-sheet convention); assault arrows show friendly advance', font=f10, fill=INK2)

sheet.save('SHITFIRE_MASTER_MAP.png')
print('wrote SHITFIRE_MASTER_MAP.png', sheet.size)
