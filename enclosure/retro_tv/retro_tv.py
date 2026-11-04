"""Mini retro TV enclosure for the ESP32 panel -- Fusion 360 script.

Builds, in the active design (use a NEW, empty design):
  * "Cuerpo TV"      -- shell with screen window in a sunken CRT bezel, touch
                        knob (TTP223 behind a 1.2 mm wall), speaker slots,
                        side grille for the DHT11, antenna socket, leg holes,
                        screw posts, screen posts and ESP32 rails.
  * "Tapa trasera"   -- back lid with locating lip, screw holes, vents and a
                        micro-USB opening.
  * "Antena"         -- rabbit-ear antenna, printed flat, plugs into the top.
  * "Pata 1..4"      -- little legs that plug into the bottom.

All sizes are in millimetres in the PARAMETERS block below: measure your
parts, edit, and run again in a new design.

Axes: X = width (right +), Y = depth (front face at Y = 0, back at Y = D),
Z = height (bottom at Z = 0). Every feature is built with symmetric extents
from planes placed at known coordinates, so it does not depend on which way
Fusion's sketch normals point.

Print (no supports): body front-face DOWN; lid outer face down; antenna flat;
legs upright. PLA/PETG, 0.2 mm layers, 3 walls.
"""
import math
import traceback

import adsk.core
import adsk.fusion

# ============================ PARAMETERS (mm) ================================
W, H, D = 112.0, 78.0, 72.0      # outer body: width, height, depth
R = 10.0                          # corner radius seen from the front
WALL = 2.4                        # wall thickness

# Screen: 2.4" ST7789 module, landscape. Measure yours!
PCB_W, PCB_H = 71.0, 43.0         # PCB outline
PCB_HOLE_INSET = 2.5              # mounting-hole centre from PCB edges
GLASS_T = 3.5                     # front of glass -> front of PCB
WIN_W, WIN_H = 49.5, 37.2         # visible area (active 48.96 x 36.72)
WIN_OFF_X, WIN_OFF_Z = 0.0, 0.0   # visible area offset from PCB centre
SCR_CX, SCR_CZ = -12.0, 39.0      # PCB centre on the front face
BEZEL_W, BEZEL_H, BEZEL_R = 62.0, 49.0, 7.0   # sunken CRT frame
BEZEL_DEPTH = 1.2

# Right-hand control panel
KNOB_X = 40.0
KNOB_TOUCH_Z = 50.0               # touch knob (TTP223 behind it)
KNOB_DECO_Z = 30.0                # decorative knob
KNOB_D, KNOB_RING = 16.0, 2.2     # knob diameter, engraved ring width
TOUCH_WALL = 1.2                  # wall left in front of the TTP223 pad
TOUCH_POCKET_D = 15.0
GRILLE_SLOTS, GRILLE_LEN, GRILLE_W = 4, 18.0, 1.6
GRILLE_TOP_Z = 18.0

# Left side grille (DHT11 sits behind it, glued)
SIDE_SLOTS, SIDE_LEN, SIDE_W = 6, 26.0, 1.8
SIDE_Y0, SIDE_Z0 = 34.0, 26.0     # first slot: front end (Y), bottom (Z)

# ESP32 DevKit 38-pin, micro-USB at the back
ESP_W, ESP_L = 28.0, 55.0
ESP_CX = 30.0                     # board centre X (clear of the corner post)
RAIL_H, RAIL_T = 12.0, 2.0        # rails the board edges rest on (pins hang between)
USB_W, USB_H = 12.0, 8.0          # lid opening
USB_Z = WALL + RAIL_H + 1.6 + 1.5 # micro-USB centre height

# Lid and screws
LID_T, LIP_H, LIP_T, LIP_CLEAR = 2.4, 3.0, 1.6, 0.25
POST_R, PILOT_D, SCREW_D = 3.5, 2.5, 3.2      # M3 self-tapping, or M2.5
SCREEN_POST_R, SCREEN_PILOT_D = 2.5, 1.8      # M2 into PCB holes

# Antenna + legs
ANT_SOCKET_W, ANT_SOCKET_T = 4.4, 3.6         # hole in the top
ANT_PEG_W, ANT_T, ANT_LEN, ANT_ANGLE = 4.0, 3.2, 46.0, 25.0
ANT_SOCKET_Y = 0.6 * D
LEG_D, LEG_H, LEG_PEG_D, LEG_PEG_H, LEG_HOLE_D = 10.0, 8.0, 6.0, 4.0, 6.4
LEG_INSET_X, LEG_INSET_Y = 20.0, 14.0
FRONT_FILLET = 2.0
# =============================================================================

MM = 0.1  # Fusion's internal unit is cm

app = adsk.core.Application.get()
ui = app.userInterface


def v(x):
    return adsk.core.ValueInput.createByReal(x * MM)


class Builder:
    def __init__(self, comp):
        self.c = comp
        self.planes = {}

    # ---- planes at known coordinates ------------------------------------
    def plane(self, axis, at):
        """Plane perpendicular to `axis` ('x','y','z') through coordinate `at`."""
        key = (axis, round(at, 4))
        if key in self.planes:
            return self.planes[key]
        base = {"x": self.c.yZConstructionPlane, "y": self.c.xZConstructionPlane,
                "z": self.c.xYConstructionPlane}[axis]
        if abs(at) < 1e-9:
            self.planes[key] = base
            return base
        cps = self.c.constructionPlanes
        for sign in (1, -1):
            inp = cps.createInput()
            inp.setByOffset(base, adsk.core.ValueInput.createByReal(sign * at * MM))
            pl = cps.add(inp)
            o = pl.geometry.origin
            got = {"x": o.x, "y": o.y, "z": o.z}[axis] / MM
            if abs(got - at) < 1e-3:
                pl.isLightBulbOn = False
                self.planes[key] = pl
                return pl
            pl.deleteMe()
        raise RuntimeError("could not place plane %s=%s" % (axis, at))

    # ---- sketch helpers (model coordinates in mm) ------------------------
    def sketch(self, axis, at):
        sk = self.c.sketches.add(self.plane(axis, at))
        sk.isComputeDeferred = True
        sk.isVisible = False
        return sk

    @staticmethod
    def sp(sk, x, y, z):
        p = sk.modelToSketchSpace(adsk.core.Point3D.create(x * MM, y * MM, z * MM))
        return adsk.core.Point3D.create(p.x, p.y, 0)

    def rrect(self, sk, to3d, cu, cv, w, h, r):
        """Rounded rectangle in the plane; to3d(u, v) -> (x, y, z) in mm."""
        r = max(0.01, min(r, w / 2 - 0.01, h / 2 - 0.01))
        P = lambda u, vv: self.sp(sk, *to3d(u, vv))
        u0, u1, v0, v1 = cu - w / 2, cu + w / 2, cv - h / 2, cv + h / 2
        L = sk.sketchCurves.sketchLines
        A = sk.sketchCurves.sketchArcs
        L.addByTwoPoints(P(u0 + r, v0), P(u1 - r, v0))
        L.addByTwoPoints(P(u1, v0 + r), P(u1, v1 - r))
        L.addByTwoPoints(P(u1 - r, v1), P(u0 + r, v1))
        L.addByTwoPoints(P(u0, v1 - r), P(u0, v0 + r))
        k = r * (1 - math.sqrt(0.5))
        A.addByThreePoints(P(u1 - r, v0), P(u1 - k, v0 + k), P(u1, v0 + r))
        A.addByThreePoints(P(u1, v1 - r), P(u1 - k, v1 - k), P(u1 - r, v1))
        A.addByThreePoints(P(u0 + r, v1), P(u0 + k, v1 - k), P(u0, v1 - r))
        A.addByThreePoints(P(u0, v0 + r), P(u0 + k, v0 + k), P(u0 + r, v0))

    def circle(self, sk, x, y, z, r):
        sk.sketchCurves.sketchCircles.addByCenterRadius(self.sp(sk, x, y, z), r * MM)

    def poly(self, sk, pts3d):
        L = sk.sketchCurves.sketchLines
        n = len(pts3d)
        for i in range(n):
            L.addByTwoPoints(self.sp(sk, *pts3d[i]), self.sp(sk, *pts3d[(i + 1) % n]))

    # ---- features -----------------------------------------------------------
    @staticmethod
    def all_profiles(sk):
        sk.isComputeDeferred = False
        oc = adsk.core.ObjectCollection.create()
        for p in sk.profiles:
            oc.add(p)
        return oc

    @staticmethod
    def profile_near(sk, area_mm2):
        """Profile whose area is closest to area_mm2 (for rings etc.)."""
        sk.isComputeDeferred = False
        best, err = None, None
        for p in sk.profiles:
            a = p.areaProperties().area / (MM * MM)
            e = abs(a - area_mm2)
            if best is None or e < err:
                best, err = p, e
        return best

    def sym(self, profiles, half, op, bodies=None, name=None):
        ext = self.c.features.extrudeFeatures
        inp = ext.createInput(profiles, op)
        inp.setSymmetricExtent(v(half), False)
        if bodies:
            inp.participantBodies = bodies
        f = ext.add(inp)
        if name and op == adsk.fusion.FeatureOperations.NewBodyFeatureOperation:
            f.bodies.item(0).name = name
        return f


def build():
    design = adsk.fusion.Design.cast(app.activeProduct)
    if not design:
        raise RuntimeError("Abre un diseño de Fusion 360 primero.")
    design.designType = adsk.fusion.DesignTypes.ParametricDesignType
    comp = design.rootComponent
    b = Builder(comp)
    NEW = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
    CUT = adsk.fusion.FeatureOperations.CutFeatureOperation
    JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation

    # In-plane mappings: front planes (u=X, v=Z), side planes (u=Y, v=Z),
    # top/bottom planes (u=X, v=Y).
    def front(y):
        return lambda u, vv: (u, y, vv)

    def side(x):
        return lambda u, vv: (x, u, vv)

    def flat(z):
        return lambda u, vv: (u, vv, z)

    # ---- 1. body block + shell --------------------------------------------
    sk = b.sketch("y", D / 2)
    b.rrect(sk, front(D / 2), 0, H / 2, W, H, R)
    body = b.sym(b.all_profiles(sk), D / 2, NEW, name="Cuerpo TV").bodies.item(0)

    back = None
    for f in body.faces:
        if abs(f.pointOnFace.y / MM - D) < 1e-3 and f.geometry.surfaceType == adsk.core.SurfaceTypes.PlaneSurfaceType:
            back = f
    sh = comp.features.shellFeatures
    oc = adsk.core.ObjectCollection.create()
    oc.add(back)
    si = sh.createInput(oc, False)
    si.insideThickness = v(WALL)
    sh.add(si)

    # Soft front edge.
    try:
        frontface = None
        for f in body.faces:
            if abs(f.pointOnFace.y) < 1e-6 and f.geometry.surfaceType == adsk.core.SurfaceTypes.PlaneSurfaceType:
                frontface = f
        edges = adsk.core.ObjectCollection.create()
        for e in frontface.loops.item(0).edges:
            edges.add(e)
        fi = comp.features.filletFeatures.createInput()
        fi.addConstantRadiusEdgeSet(edges, v(FRONT_FILLET), True)
        comp.features.filletFeatures.add(fi)
    except Exception:
        pass  # cosmetic only

    BODY = [body]

    # ---- 2. front: bezel recess, screen window, knobs, speaker slots --------
    sk = b.sketch("y", 0)
    b.rrect(sk, front(0), SCR_CX + WIN_OFF_X, SCR_CZ + WIN_OFF_Z, BEZEL_W, BEZEL_H, BEZEL_R)
    b.sym(b.all_profiles(sk), BEZEL_DEPTH, CUT, BODY)

    sk = b.sketch("y", 0)
    b.rrect(sk, front(0), SCR_CX + WIN_OFF_X, SCR_CZ + WIN_OFF_Z, WIN_W, WIN_H, 2.0)
    b.sym(b.all_profiles(sk), WALL + 1, CUT, BODY)

    for kz in (KNOB_TOUCH_Z, KNOB_DECO_Z):                 # engraved knob rings
        sk = b.sketch("y", 0)
        ro, ri = KNOB_D / 2, KNOB_D / 2 - KNOB_RING
        b.circle(sk, KNOB_X, 0, kz, ro)
        b.circle(sk, KNOB_X, 0, kz, ri)
        b.sym(b.profile_near(sk, math.pi * (ro * ro - ri * ri)), 0.8, CUT, BODY)
        sk = b.sketch("y", 0)                              # pointer notch
        b.rrect(sk, front(0), KNOB_X, kz + ri - 2.5, 1.6, 5.0, 0.7)
        b.sym(b.all_profiles(sk), 0.8, CUT, BODY)

    sk = b.sketch("y", WALL)                               # thin wall for the TTP223
    b.circle(sk, KNOB_X, WALL, KNOB_TOUCH_Z, TOUCH_POCKET_D / 2)
    b.sym(b.all_profiles(sk), WALL - TOUCH_WALL, CUT, BODY)

    sk = b.sketch("y", 0)                                  # speaker slots
    for i in range(GRILLE_SLOTS):
        b.rrect(sk, front(0), KNOB_X, GRILLE_TOP_Z - i * (GRILLE_W * 2.2), GRILLE_LEN, GRILLE_W, GRILLE_W / 2 - 0.05)
    b.sym(b.all_profiles(sk), WALL + 1, CUT, BODY)

    # ---- 3. left side grille (DHT11) -----------------------------------------
    sk = b.sketch("x", -W / 2)
    for i in range(SIDE_SLOTS):
        z = SIDE_Z0 + i * SIDE_W * 2.4
        b.rrect(sk, side(-W / 2), SIDE_Y0 + SIDE_LEN / 2, z, SIDE_LEN, SIDE_W, SIDE_W / 2 - 0.05)
    b.sym(b.all_profiles(sk), WALL + 1, CUT, BODY)

    # ---- 4. top antenna socket, bottom leg holes -------------------------------
    sk = b.sketch("z", H)
    b.rrect(sk, flat(H), 0, ANT_SOCKET_Y, ANT_SOCKET_W, ANT_SOCKET_T, 0.3)
    b.sym(b.all_profiles(sk), WALL + 1, CUT, BODY)

    sk = b.sketch("z", 0)
    legs_xy = [(sx * (W / 2 - LEG_INSET_X), y) for sx in (-1, 1) for y in (LEG_INSET_Y, D - LEG_INSET_Y)]
    for (x, y) in legs_xy:
        b.circle(sk, x, y, 0, LEG_HOLE_D / 2)
    b.sym(b.all_profiles(sk), WALL + 1, CUT, BODY)

    # ---- 5. inside: screw posts, screen posts, ESP32 rails --------------------
    corners = [(sx * (W / 2 - R), z) for sx in (-1, 1) for z in (R, H - R)]
    sk = b.sketch("y", (WALL + D) / 2)
    for (x, z) in corners:
        b.circle(sk, x, (WALL + D) / 2, z, POST_R)
    b.sym(b.all_profiles(sk), (D - WALL) / 2, JOIN, BODY)
    sk = b.sketch("y", D)
    for (x, z) in corners:
        b.circle(sk, x, D, z, PILOT_D / 2)
    b.sym(b.all_profiles(sk), 12.0, CUT, BODY)

    hx, hz = PCB_W / 2 - PCB_HOLE_INSET, PCB_H / 2 - PCB_HOLE_INSET
    spts = [(SCR_CX + sx * hx, SCR_CZ + sz * hz) for sx in (-1, 1) for sz in (-1, 1)]
    ymid = WALL + GLASS_T / 2
    sk = b.sketch("y", ymid)
    for (x, z) in spts:
        b.circle(sk, x, ymid, z, SCREEN_POST_R)
    b.sym(b.all_profiles(sk), GLASS_T / 2, JOIN, BODY)
    sk = b.sketch("y", WALL + GLASS_T)
    for (x, z) in spts:
        b.circle(sk, x, WALL + GLASS_T, z, SCREEN_PILOT_D / 2)
    b.sym(b.all_profiles(sk), GLASS_T - 0.6, CUT, BODY)

    zr = WALL + RAIL_H / 2
    y0r, y1r = WALL, D - LIP_H - 1.0                     # rails start at the front wall (printable)
    sk = b.sketch("z", zr)
    for sx in (-1, 1):
        cx = ESP_CX + sx * (ESP_W / 2 - RAIL_T / 2)
        b.rrect(sk, flat(zr), cx, (y0r + y1r) / 2, RAIL_T, y1r - y0r, 0.2)
    b.sym(b.all_profiles(sk), RAIL_H / 2 + 0.2, JOIN, BODY)

    # ---- 6. back lid (printed separately, placed behind the body) -------------
    LID_Y = D + 15.0                                       # inner face of the plate
    sk = b.sketch("y", LID_Y + LID_T / 2)
    b.rrect(sk, front(LID_Y + LID_T / 2), 0, H / 2, W, H, R)
    lid = b.sym(b.all_profiles(sk), LID_T / 2, NEW, name="Tapa trasera").bodies.item(0)
    LID = [lid]

    ym = LID_Y - LIP_H / 2
    sk = b.sketch("y", ym)
    ow, oh = W - 2 * WALL - 2 * LIP_CLEAR, H - 2 * WALL - 2 * LIP_CLEAR
    orr = R - WALL - LIP_CLEAR
    b.rrect(sk, front(ym), 0, H / 2, ow, oh, orr)
    b.rrect(sk, front(ym), 0, H / 2, ow - 2 * LIP_T, oh - 2 * LIP_T, max(0.5, orr - LIP_T))
    ring_area = 2 * (ow + oh) * LIP_T                      # approx.
    b.sym(b.profile_near(sk, ring_area), LIP_H / 2 + 0.05, JOIN, LID)

    yc = LID_Y + LID_T / 2
    sk = b.sketch("y", yc)
    for (x, z) in corners:
        b.circle(sk, x, yc, z, SCREW_D / 2)
    b.rrect(sk, front(yc), ESP_CX, USB_Z, USB_W, USB_H, 1.5)
    for i in range(5):                                     # vents, upper half
        b.rrect(sk, front(yc), -18.0, H - 18.0 - i * 5.0, 44.0, 2.0, 0.95)
    b.sym(b.all_profiles(sk), LID_T + LIP_H + 1, CUT, LID)

    # ---- 7. antenna (flat, in front of the body) -------------------------------
    AY = -40.0
    sk = b.sketch("y", AY)
    pw = ANT_PEG_W / 2
    b.poly(sk, [(-pw, AY, 0), (pw, AY, 0), (pw, AY, 8), (-pw, AY, 8)])                  # peg
    b.rrect(sk, front(AY), 0, 10.5, 16.0, 6.0, 2.5)                                     # hub
    a = math.radians(ANT_ANGLE)
    for sx in (-1, 1):
        ux, uz = sx * math.sin(a), math.cos(a)                                          # along the ear
        nx, nz = uz, -ux                                                                # across
        x0, z0 = sx * 3.0, 11.0
        x1, z1 = x0 + ux * ANT_LEN, z0 + uz * ANT_LEN
        t = 1.5
        b.poly(sk, [(x0 + nx * t, AY, z0 + nz * t), (x1 + nx * t, AY, z1 + nz * t),
                    (x1 - nx * t, AY, z1 - nz * t), (x0 - nx * t, AY, z0 - nz * t)])
        b.circle(sk, x1, AY, z1, 4.5)                                                   # ball
    b.sym(b.all_profiles(sk), ANT_T / 2, NEW, name="Antena")

    # ---- 8. legs (upright, beside the body) -------------------------------------
    for i in range(4):
        x, y = W / 2 + 20.0 + i * (LEG_D + 6), 10.0
        sk = b.sketch("z", LEG_H / 2)
        b.circle(sk, x, y, LEG_H / 2, LEG_D / 2)
        leg = b.sym(b.all_profiles(sk), LEG_H / 2, NEW, name="Pata %d" % (i + 1)).bodies.item(0)
        sk = b.sketch("z", LEG_H + LEG_PEG_H / 2)
        b.circle(sk, x, y, LEG_H + LEG_PEG_H / 2, LEG_PEG_D / 2)
        b.sym(b.all_profiles(sk), LEG_PEG_H / 2, JOIN, [leg])

    app.activeViewport.fit()


def run(context):
    try:
        build()
        ui.messageBox("Mini TV creada: Cuerpo TV, Tapa trasera, Antena y 4 patas.\n"
                      "Comprueba las medidas de tu pantalla en el script si algo no encaja.")
    except Exception:
        ui.messageBox("Error:\n{}".format(traceback.format_exc()))
