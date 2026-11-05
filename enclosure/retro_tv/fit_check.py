"""Fit check for the Mini TV enclosure: rebuilds the body / lid features of
retro_tv.py as solids, places the real components inside (with their
connectors and dupont cables) and reports every collision.

    pip install manifold3d numpy
    python enclosure/retro_tv/fit_check.py [--solder] [--header left|right]

It imports the PARAMETERS block from retro_tv.py, so it always checks the
current numbers. Component sizes are typical datasheet values: measure yours
and edit COMPONENTS below if they differ.
"""
import argparse
import ast
import math
import os
import sys

from manifold3d import CrossSection, JoinType, Manifold

HERE = os.path.dirname(os.path.abspath(__file__))


def load_params():
    """Evaluates the assignments of retro_tv.py's PARAMETERS block."""
    src = open(os.path.join(HERE, "retro_tv.py"), encoding="utf-8").read()
    a = src.index("# ============================ PARAMETERS")
    b = src.index("# =====", a + 10)
    ns = {"math": math}
    for node in ast.parse(src[a:b]).body:
        exec(compile(ast.Module([node], []), "params", "exec"), ns)
    return ns


P = load_params()
g = lambda k: P[k]
W, H, D, R, WALL = g("W"), g("H"), g("D"), g("R"), g("WALL")


PAD = 0.0   # grown around component boxes (--margin)


def box(x0, x1, y0, y1, z0, z1):
    x0, y0, z0, x1, y1, z1 = x0 - PAD, y0 - PAD, z0 - PAD, x1 + PAD, y1 + PAD, z1 + PAD
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def cyl_y(x, z, r, y0, y1):          # cylinder along Y
    return Manifold.cylinder(y1 - y0, r, circular_segments=32).rotate([-90, 0, 0]).translate([x, y0, z])


def cyl_z(x, y, r, z0, z1):
    return Manifold.cylinder(z1 - z0, r, circular_segments=32).translate([x, y, z0])


def rrect_y(cx, cz, w, h, r, y0, y1):  # rounded rect in XZ, extruded along Y
    cs = CrossSection.square([max(w - 2 * r, 0.01), max(h - 2 * r, 0.01)], center=True)
    if r > 0:
        cs = cs.offset(r, JoinType.Round, circular_segments=32)
    return cs.extrude(y1 - y0).rotate([-90, 0, 0]).translate([cx, y0, cz])


# ---- enclosure (assembled: lid on the back) ------------------------------------

def enclosure():
    parts = {}
    outer = rrect_y(0, H / 2, W, H, R, 0, D)
    inner = rrect_y(0, H / 2, W - 2 * WALL, H - 2 * WALL, R - WALL, WALL, D + 1)
    pocket = rrect_y(g("KNOB_X"), g("KNOB_TOUCH_Z"), g("TTP_W") + 2 * g("TTP_CLEAR"),
                     g("TTP_H") + 2 * g("TTP_CLEAR"), 0.6, g("TOUCH_WALL"), WALL + 0.01)
    parts["carcasa"] = outer - inner - pocket

    corners = [(sx * (W / 2 - R), z) for sx in (-1, 1) for z in (R, H - R)]
    parts["postes tapa"] = sum((cyl_y(x, z, g("POST_R"), WALL, D) for x, z in corners), Manifold())

    hx, hz = g("PCB_W") / 2 - g("PCB_HOLE_INSET"), g("PCB_H") / 2 - g("PCB_HOLE_INSET")
    scx, scz = g("SCR_CX"), g("SCR_CZ")
    parts["postes pantalla"] = sum((cyl_y(scx + sx * hx, scz + sz * hz, g("SCREEN_POST_R"), WALL, WALL + g("GLASS_T"))
                                    for sx in (-1, 1) for sz in (-1, 1)), Manifold())

    ESP_CX, ESP_W, RAIL_T, RAIL_H = g("ESP_CX"), g("ESP_W"), g("RAIL_T"), g("RAIL_H")
    y1r = D - g("LIP_H") - 1.0
    scr_back = WALL + g("GLASS_T") + 1.6 + g("SCREEN_BACK")
    esp_y0 = D - g("USB_GAP") - g("ESP_L")
    rails = Manifold()
    for sx in (-1, 1):
        cx = ESP_CX + sx * g("RAIL_OFF")
        under = abs(cx - g("SCR_CX")) < g("PCB_W") / 2 + RAIL_T
        ys = scr_back if under else WALL
        ramp = RAIL_H if under else 0.0
        x0, x1 = cx - RAIL_T / 2, cx + RAIL_T / 2
        rails += box(x0, x1, ys + ramp, y1r, WALL, WALL + RAIL_H)
        for k in range(int(ramp)):                       # 45-degree ramp, 1 mm steps
            rails += box(x0, x1, ys + k, ys + k + 1, WALL, WALL + k + 1)
        if ys + ramp <= esp_y0 - 6.0:
            rails += box(x0, x1, esp_y0 - 6, esp_y0, WALL + RAIL_H, WALL + RAIL_H + g("ESP_STOP_H"))
    parts["raíles ESP32"] = rails


    # Lid: plate behind the body + locating lip inside it.
    LIP_H, LIP_T, CL = g("LIP_H"), g("LIP_T"), g("LIP_CLEAR")
    ow, oh = W - 2 * WALL - 2 * CL, H - 2 * WALL - 2 * CL
    orr = R - WALL - CL
    lip = rrect_y(0, H / 2, ow, oh, orr, D - LIP_H, D) - rrect_y(0, H / 2, ow - 2 * LIP_T, oh - 2 * LIP_T, max(0.5, orr - LIP_T), D - LIP_H - 1, D + 1)
    plate = rrect_y(0, H / 2, W, H, R, D, D + g("LID_T"))
    hole = rrect_y(ESP_CX, g("USB_Z"), g("USB_W"), g("USB_H"), 1.5, D - 1, D + g("LID_T") + 1)
    parts["tapa"] = (plate + lip) - hole

    # DHT11 module cradle on the lid's inner face (Y = D).
    dep = g("DHTM_STACK") + g("DHTM_CLEAR")
    gz = (g("DHTM_W") + 2 * g("DHTM_CLEAR")) / 2
    rt, cz = g("DHTM_RAIL_T"), g("DHTM_CZ")
    xs = g("DHTM_CX") - g("DHTM_L") / 2 - g("DHTM_CLEAR")
    xe = g("DHTM_CX") + g("DHTM_L") / 2 - g("DHTM_PIN_END")
    cr = Manifold()
    for sz in (-1, 1):
        zlo, zhi = sorted((cz + sz * gz, cz + sz * (gz + rt)))
        cr += box(xs - rt, xe, D - dep - rt, D, zlo, zhi)
        llo, lhi = sorted((cz + sz * (gz - g("DHTM_LIP")), cz + sz * gz))
        cr += box(xs - rt, xe, D - dep - rt, D - dep, llo, lhi)
    cr += box(xs - rt, xs, D - dep, D, cz - gz - rt, cz + gz + rt)
    parts["guía DHT11 (tapa)"] = cr
    return parts


# ---- components ----------------------------------------------------------------------

DUPONT = 14.0 + 3.0   # female dupont housing over the pin + the wire's bend


PINS = "straight"   # DHT11 module pins: "straight" (into the box) or "angle" (along the PCB)
ROWS = 25.4   # ESP32 header row spacing (some narrow boards: 22.86)


def components(solder, header_side):
    c = {}
    scx, scz = g("SCR_CX"), g("SCR_CZ")
    pcb_y0 = WALL + g("GLASS_T")                              # PCB front face
    # Screen: LCD + backlight in front of the PCB, PCB, 8-pin header on a short edge.
    c["pantalla (LCD)"] = box(scx - 30.2, scx + 30.2, WALL + 0.05, pcb_y0, scz - 21.4, scz + 21.4)
    c["pantalla (PCB)"] = box(scx - g("PCB_W") / 2, scx + g("PCB_W") / 2, pcb_y0, pcb_y0 + 1.6,
                              scz - g("PCB_H") / 2, scz + g("PCB_H") / 2)
    hx = scx + (1 if header_side == "right" else -1) * (g("PCB_W") / 2 - 2.5)
    back = pcb_y0 + 1.6
    reach = 2.5 + (6.0 if solder else DUPONT)                 # header (+ dupont + bend)
    c["pantalla (pines%s)" % ("" if solder else " + dupont")] = box(hx - 1.5, hx + 1.5, back, back + reach,
                                                                   scz - 10.5, scz + 10.5)

    # ESP32 DevKit 38: board on the rails, module at the front, USB at the back,
    # male headers pointing down (rows 25.4 apart).
    ex, ew, el = g("ESP_CX"), 28.0, 55.0
    ey1 = D - g("USB_GAP")                                    # USB face
    ey0 = ey1 - el
    zb = WALL + g("RAIL_H")                                   # board bottom
    c["ESP32 (placa)"] = box(ex - ew / 2, ex + ew / 2, ey0, ey1, zb, zb + 1.6)
    c["ESP32 (módulo WROOM)"] = box(ex - 9, ex + 9, ey0, ey0 + 25.5, zb + 1.6, zb + 1.6 + 3.2)
    c["ESP32 (conector USB)"] = box(ex - 4, ex + 4, ey1 - 5.5, ey1 + 0.5, zb + 1.6, zb + 1.6 + 3.0)
    below = 2.5 + (2.0 if solder else DUPONT)                 # header plastic (+ dupont + bend)
    rows = Manifold()
    for sx in (-1, 1):
        x = ex + sx * ROWS / 2
        rows += box(x - 1.3, x + 1.3, ey0 + 2, ey1 - 2, zb - below, zb)
    c["ESP32 (pines%s)" % ("" if solder else " + dupont")] = rows
    # Micro-USB plug: metal inside the socket, overmould from just behind it.
    uz = zb + 1.6 + 1.5
    c["ESP32 (clavija USB)"] = box(ex - 3.5, ex + 3.5, ey1 - 5.0, ey1 + 1.5, uz - 1.25, uz + 1.25)
    c["cable USB (funda)"] = box(ex - 5.5, ex + 5.5, ey1 + 1.5, ey1 + 25, uz - 3.75, uz + 3.75)

    # DHT11 module in the lid cradle: sensor face on the lid, PCB behind it,
    # 3 pins at the end: right-angle (along +X) or straight (into the box).
    L, Wd, cz = g("DHTM_L"), g("DHTM_W"), g("DHTM_CZ")
    x0 = g("DHTM_CX") - L / 2
    c["DHT11 (sensor)"] = box(x0, x0 + 15.5, D - 5.5, D - 0.05, cz - 6, cz + 6)
    c["DHT11 (placa)"] = box(x0, x0 + L, D - 7.1, D - 5.5, cz - Wd / 2, cz + Wd / 2)
    reach = 2.5 + (4.0 if solder else DUPONT)
    if PINS == "angle":
        c["DHT11 (pines%s)" % ("" if solder else " + dupont")] = box(x0 + L - g("DHTM_PIN_END"), x0 + L + reach, D - 7.1 - 2.5, D - 5.5,
                                                                    cz - 4, cz + 4)
    else:
        c["DHT11 (pines%s)" % ("" if solder else " + dupont")] = box(x0 + L - 3.5, x0 + L - 1, D - 7.1 - reach, D - 7.1,
                                                                    cz - 4, cz + 4)

    # TTP223 in its pocket, components and wires behind.
    tx, tz = g("KNOB_X"), g("KNOB_TOUCH_Z")
    c["TTP223"] = box(tx - 7.5, tx + 7.5, g("TOUCH_WALL") + 0.05, g("TOUCH_WALL") + 1.05, tz - 5.5, tz + 5.5)
    c["TTP223 (componentes + cables)"] = box(tx - 6, tx + 6, g("TOUCH_WALL") + 1.05, g("TOUCH_WALL") + 1.05 + (4 if solder else 2.5 + DUPONT),
                                             tz - 4, tz + 4)
    return c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--solder", action="store_true", help="wires soldered (no dupont connectors)")
    ap.add_argument("--margin", type=float, default=0.0, help="clearance (mm) required around each component")
    ap.add_argument("--dht-pins", choices=["straight", "angle"], default="straight",
                    help="DHT11 module pins: straight (into the box) or right-angle (along the PCB)")
    ap.add_argument("--narrow", action="store_true", help="narrow ESP32 board (rows 22.86 mm apart)")
    ap.add_argument("--header", choices=["left", "right"], default="right", help="side of the screen's pin header")
    a = ap.parse_args()
    enc = enclosure()
    global PAD, ROWS, PINS
    PINS = a.dht_pins
    if a.narrow:
        ROWS = 22.86
    PAD = a.margin
    comp = components(a.solder, a.header)
    PAD = 0.0
    print("Cableado: %s · pines de la pantalla a la %s" % ("soldado" if a.solder else "dupont",
                                                          "derecha" if a.header == "right" else "izquierda"))
    bad = 0
    names = list(comp)
    for n in names:
        for en, e in enc.items():
            v = (comp[n] ^ e).volume()
            if v > 0.5:
                print("  CHOCA  %-32s con %-16s %7.1f mm3" % (n, en, v)); bad += 1
    for i, a1 in enumerate(names):
        for a2 in names[i + 1:]:
            if a1.split(" (")[0] == a2.split(" (")[0] or a.margin:
                continue
            v = (comp[a1] ^ comp[a2]).volume()
            if v > 0.5:
                print("  CHOCA  %-32s con %-16s %7.1f mm3" % (a1, a2, v)); bad += 1
    # USB plug must pass through the lid hole.
    print("  %s" % ("OK" if bad == 0 else "%d choques" % bad))
    return bad


if __name__ == "__main__":
    sys.exit(1 if main() else 0)
