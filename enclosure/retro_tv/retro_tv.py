"""Mini retro TV enclosure for the ESP32 panel -- Fusion 360 script.

Builds, in the active design (use a NEW, empty design):
  * "Cuerpo TV"      -- shell with screen window in a sunken CRT bezel, touch
                        knob (TTP223 pocket behind a 1.2 mm wall), speaker
                        slots, side grille,
                        antenna socket, leg holes, screw posts, screen corner guides
                        and ESP32 rails with a stop that puts the micro-USB
                        right behind the lid's cable opening.
  * "Tapa trasera"   -- back lid with locating lip, screw holes, vents, a
                        slide-in cradle for the DHT11 module and a
                        micro-USB opening.
  * "Antena"         -- rabbit-ear antenna, printed flat, plugs into the top.
  * "Pata delantera/trasera" -- legs; the front ones are longer so the TV
                        leans back TILT_DEG degrees towards you.

All sizes are in millimetres in the PARAMETERS block below: measure your
parts, edit, and run again in a new design.

Axes: X = width (right +), Y = depth (front face at Y = 0, back at Y = D),
Z = height (bottom at Z = 0). Every feature is built with symmetric extents
from planes placed at known coordinates, so it does not depend on which way
Fusion's sketch normals point.

Print (no supports): body front-face DOWN; lid outer face down; antenna flat;
legs upside down (peg on the bed). PLA/PETG, 0.2 mm layers, 3 walls.
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
PCB_W, PCB_H = 63.0, 38.0         # module outline (glass covers almost all of it)
SCREEN_T = 5.0                    # front of glass -> back of PCB
GLASS_T = SCREEN_T - 1.6          # front of glass -> front of PCB
# No room for screws: the module drops into corner guides, face on the wall,
# and a dot of hot glue (or tape) at each corner keeps it there.
SCR_FRAME_T, SCR_FRAME_LEG = 1.6, 8.0   # guide wall thickness, leg length
SCR_CLEAR = 0.3                   # clearance per side
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
TTP_W, TTP_H = 15.0, 11.0         # TTP223 module PCB (pad side faces the wall)
TTP_CLEAR = 0.3                   # pocket clearance per side
GRILLE_SLOTS, GRILLE_LEN, GRILLE_W = 4, 18.0, 1.6
GRILLE_TOP_Z = 18.0

# Left side grille (air in for the sensor; looks like a speaker)
SIDE_SLOTS, SIDE_LEN, SIDE_W = 6, 26.0, 1.8
SIDE_Y0, SIDE_Z0 = 34.0, 26.0     # first slot: front end (Y), bottom (Z)

# DHT11 on its little PCB (3-pin module), held on the inside of the back lid
# behind the vents, top left: far from the ESP32's heat. Measure yours!
DHTM_L, DHTM_W = 30.0, 17.0       # module: length WITH the pins (at one end), width
DHTM_STACK = 10.0                 # sensor face to PCB back (measured)
DHTM_CX, DHTM_CZ = -24.0, 60.0    # module centre on the lid (X, Z)
DHTM_CLEAR = 0.3                  # slide clearance
DHTM_RAIL_T, DHTM_LIP = 1.6, 1.2  # cradle rail thickness, lip over the PCB edge
DHTM_PIN_END = 7.0                # pin end left free (header + pins, no lip there)

# ESP32 DevKit 38-pin, micro-USB at the back
ESP_W, ESP_L = 28.0, 55.0
ESP_CX = 28.0                     # board centre X (pin rows clear of the corner posts)
RAIL_H, RAIL_T = 21.0, 2.0        # rails under the board, room for dupont plugs below
RAIL_OFF = 8.0                    # rail centre from board centre: between the pin rows (+-12.7, or +-11.4 on narrow boards)
SCREEN_BACK = 3.0                 # parts on the back of the screen PCB (SD slot, regulator)
ESP_STOP_H = 3.0                  # front stop on the rails: USB ends up at the lid
USB_GAP = 1.5                     # USB connector face -> lid inner face
USB_W, USB_H = 13.0, 9.0          # lid opening for the power cable (plug moulding ~11 x 7.5)
USB_Z = WALL + RAIL_H + 1.6 + 1.5 # micro-USB centre height

# USB-C power-in module (small breakout PCB, receptacle on one short edge),
# held flat in an open-top pocket on the lid: receptacle through the lid,
# PCB front edge against the lid, stops behind it. Measure yours!
USBC_W, USBC_L, USBC_PCB_T = 14.0, 12.0, 1.6  # PCB width (across), length, thickness
USBC_OUT = 1.0                    # receptacle mouth ahead of the PCB front edge
USBC_SHELL_W, USBC_SHELL_H = 8.94, 3.26       # receptacle shell (USB-C standard)
USBC_CX, USBC_Z0 = -20.0, 12.0    # module centre X, PCB underside Z
USBC_CLEAR = 0.3                  # pocket clearance
USBC_PLUG_W, USBC_PLUG_H = 13.5, 8.0          # outside recess for the plug moulding

# Lid and screws
LID_T, LIP_H, LIP_T, LIP_CLEAR = 2.4, 3.0, 1.6, 0.25
POST_R, PILOT_D, SCREW_D = 3.5, 2.5, 3.2      # M3 self-tapping, or M2.5

# Antenna + legs
ANT_SOCKET_W, ANT_SOCKET_T = 4.4, 3.6         # hole in the top
ANT_PEG_W, ANT_T, ANT_LEN, ANT_ANGLE = 4.0, 3.2, 46.0, 25.0
ANT_SOCKET_Y = 0.6 * D
LEG_D, LEG_H, LEG_PEG_D, LEG_PEG_H, LEG_HOLE_D = 10.0, 8.0, 6.0, 4.0, 6.4
TILT_DEG = 12.0                   # leans back so the screen faces you; 0 = upright
LEG_TIP_R = 4.0                   # rounded foot tip (sits well at any tilt)
LEG_INSET_X, LEG_INSET_Y = 20.0, 14.0
FRONT_FILLET = 2.0

# Layout: True = every piece lying on the print bed (Z = 0) in its print
# orientation, ready to export; False = pieces next to the assembled TV.
# The move is the last step in the timeline: suppress it to see the TV whole.
PRINT_LAYOUT = True
PRINT_GAP = 8.0                   # space between pieces on the bed
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

    sk = b.sketch("y", WALL)                               # TTP223 pocket, thin wall
    b.rrect(sk, front(WALL), KNOB_X, KNOB_TOUCH_Z, TTP_W + 2 * TTP_CLEAR, TTP_H + 2 * TTP_CLEAR, 0.6)
    b.sym(b.all_profiles(sk), WALL - TOUCH_WALL, CUT, BODY)

    sk = b.sketch("y", 0)                                  # speaker slots
    for i in range(GRILLE_SLOTS):
        b.rrect(sk, front(0), KNOB_X, GRILLE_TOP_Z - i * (GRILLE_W * 2.2), GRILLE_LEN, GRILLE_W, GRILLE_W / 2 - 0.05)
    b.sym(b.all_profiles(sk), WALL + 1, CUT, BODY)

    # ---- 3. left side grille --------------------------------------------------
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

    # ---- 5. inside: screw posts, screen corner guides, ESP32 rails --------------------
    corners = [(sx * (W / 2 - R), z) for sx in (-1, 1) for z in (R, H - R)]
    sk = b.sketch("y", (WALL + D) / 2)
    for (x, z) in corners:
        b.circle(sk, x, (WALL + D) / 2, z, POST_R)
    b.sym(b.all_profiles(sk), (D - WALL) / 2, JOIN, BODY)
    sk = b.sketch("y", D)
    for (x, z) in corners:
        b.circle(sk, x, D, z, PILOT_D / 2)
    b.sym(b.all_profiles(sk), 12.0, CUT, BODY)

    # Screen corner guides: L-shaped walls just outside the module outline,
    # as deep as the module, standing on the front wall (prints upright).
    ymid = WALL + SCREEN_T / 2
    gx, gz = PCB_W / 2 + SCR_CLEAR, PCB_H / 2 + SCR_CLEAR
    t, leg = SCR_FRAME_T, SCR_FRAME_LEG
    for sx in (-1, 1):
        for sz in (-1, 1):
            sk = b.sketch("y", ymid)
            cx, cz = SCR_CX + sx * (gx + t / 2), SCR_CZ + sz * (gz + t / 2)
            b.rrect(sk, front(ymid), SCR_CX + sx * (gx + t - (leg + t) / 2), cz, leg + t, t, 0.2)
            b.rrect(sk, front(ymid), cx, SCR_CZ + sz * (gz + t - (leg + t) / 2), t, leg + t, 0.2)
            b.sym(b.all_profiles(sk), SCREEN_T / 2, JOIN, BODY)

    # Rails along Y, between the ESP32's pin rows (dupont plugs hang down
    # beside them). A rail under the screen starts behind the screen PCB with
    # a 45-degree ramp; one clear of the screen starts at the front wall. Both
    # print without supports (front face on the bed).
    y1r = D - LIP_H - 1.0
    scr_back = WALL + GLASS_T + 1.6 + SCREEN_BACK
    esp_y0 = D - USB_GAP - ESP_L
    for sx in (-1, 1):
        cx = ESP_CX + sx * RAIL_OFF
        under_screen = abs(cx - SCR_CX) < PCB_W / 2 + RAIL_T
        ys = scr_back if under_screen else WALL
        ramp = RAIL_H if under_screen else 0.0
        sk = b.sketch("x", cx)
        b.poly(sk, [(cx, ys, WALL - 0.2), (cx, ys + ramp, WALL + RAIL_H), (cx, y1r, WALL + RAIL_H),
                    (cx, y1r, WALL - 0.2)])
        b.sym(b.all_profiles(sk), RAIL_T / 2, JOIN, BODY)
        # Stop on the rail(s) already full height there: push the board
        # against it and the micro-USB lands USB_GAP mm behind the lid.
        if ys + ramp <= esp_y0 - 6.0:
            zs = WALL + RAIL_H + ESP_STOP_H / 2
            sk = b.sketch("z", zs)
            b.rrect(sk, flat(zs), cx, esp_y0 - 3.0, RAIL_T, 6.0, 0.2)
            b.sym(b.all_profiles(sk), ESP_STOP_H / 2 + 0.1, JOIN, BODY)

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

    # DHT11 module cradle on the lid's inner face: two L-rails along X hold
    # the PCB edges with the sensor face against the lid (behind the vents);
    # it slides in from the pin end and stops against an end wall. Printed
    # with the lid's outer face down, the rails stand up and the small lips
    # are short overhangs.
    dep = DHTM_STACK + DHTM_CLEAR                          # sensor face -> under the lip
    gz = (DHTM_W + 2 * DHTM_CLEAR) / 2                     # half gap between the rails
    xs = DHTM_CX - DHTM_L / 2 - DHTM_CLEAR                 # sensor end of the slot
    xe = DHTM_CX + DHTM_L / 2 - DHTM_PIN_END               # rails stop before the pins
    xm = (xs - DHTM_RAIL_T + xe) / 2
    sk = b.sketch("x", xm)
    for sz in (-1, 1):
        z_in, z_out = DHTM_CZ + sz * gz, DHTM_CZ + sz * (gz + DHTM_RAIL_T)
        z_lip = DHTM_CZ + sz * (gz - DHTM_LIP)
        y_in, y_lip = LID_Y - dep, LID_Y - dep - DHTM_RAIL_T
        b.poly(sk, [(xm, LID_Y + 0.2, z_in), (xm, LID_Y + 0.2, z_out), (xm, y_lip, z_out),
                    (xm, y_lip, z_lip), (xm, y_in, z_lip), (xm, y_in, z_in)])
    b.sym(b.all_profiles(sk), (xe - xs + DHTM_RAIL_T) / 2, JOIN, LID)
    sk = b.sketch("y", LID_Y - dep / 2)                    # end wall at the sensor end
    b.rrect(sk, front(LID_Y - dep / 2), xs - DHTM_RAIL_T / 2, DHTM_CZ, DHTM_RAIL_T,
            2 * (gz + DHTM_RAIL_T), 0.2)
    b.sym(b.all_profiles(sk), dep / 2 + 0.1, JOIN, LID)

    # USB-C module pocket: per side a ledge under the PCB edge and a wall
    # beside it, from the lid inwards; a stop behind each rear corner. The
    # module drops in from above (the TV stands on its legs, so it rests on
    # the ledges); its front edge sits on the lid. All of it stands straight
    # up from the lid when printed, no overhangs.
    hw = USBC_W / 2 + USBC_CLEAR
    z0, z1 = USBC_Z0 - 3.0, USBC_Z0 + USBC_PCB_T + 2.0
    ya, yb = LID_Y + 0.2, LID_Y - USBC_L - USBC_CLEAR
    ym = (ya + yb) / 2
    sk = b.sketch("y", ym)
    for sx in (-1, 1):
        xe, xl, xw = USBC_CX + sx * hw, USBC_CX + sx * (hw - 1.5), USBC_CX + sx * (hw + 2.0)
        b.poly(sk, [(xl, ym, z0), (xw, ym, z0), (xw, ym, z1), (xe, ym, z1),
                    (xe, ym, USBC_Z0), (xl, ym, USBC_Z0)])
    b.sym(b.all_profiles(sk), (ya - yb) / 2, JOIN, LID)
    sk = b.sketch("y", yb - 0.75)
    for sx in (-1, 1):
        b.rrect(sk, front(yb - 0.75), USBC_CX + sx * (hw + 0.25), (z0 + USBC_Z0 + USBC_PCB_T + 1.0) / 2,
                3.5, USBC_Z0 + USBC_PCB_T + 1.0 - z0, 0.2)
    b.sym(b.all_profiles(sk), 0.8, JOIN, LID)
    # Opening: the receptacle fits through a thin ring; the plug moulding
    # sits in a recess from outside so it reaches all the way in.
    uz = USBC_Z0 + USBC_PCB_T + USBC_SHELL_H / 2
    ring = min(LID_T, max(0.6, USBC_OUT))
    sk = b.sketch("y", LID_Y + LID_T / 2)
    b.rrect(sk, front(LID_Y + LID_T / 2), USBC_CX, uz, USBC_SHELL_W + 0.6, USBC_SHELL_H + 0.6, 1.5)
    b.sym(b.all_profiles(sk), LID_T / 2 + 0.5, CUT, LID)
    if ring < LID_T:
        sk = b.sketch("y", LID_Y + LID_T)
        b.rrect(sk, front(LID_Y + LID_T), USBC_CX, uz, USBC_PLUG_W, USBC_PLUG_H, 2.5)
        b.sym(b.all_profiles(sk), LID_T - ring, CUT, LID)

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

    # ---- 8. legs: front ones longer so the TV leans back by TILT_DEG -------------
    # Printed upside down (peg on the bed); rounded tips touch the table.
    span = D - 2 * LEG_INSET_Y
    front_h = LEG_H + span * math.tan(math.radians(TILT_DEG))
    specs = [("Pata delantera 1", front_h), ("Pata delantera 2", front_h),
             ("Pata trasera 1", LEG_H), ("Pata trasera 2", LEG_H)]
    for i, (name, lh) in enumerate(specs):
        x, y = W / 2 + 20.0 + i * (LEG_D + 6), 10.0
        sk = b.sketch("z", lh / 2)
        b.circle(sk, x, y, lh / 2, LEG_D / 2)
        leg = b.sym(b.all_profiles(sk), lh / 2, NEW, name=name).bodies.item(0)
        sk = b.sketch("z", lh + LEG_PEG_H / 2)
        b.circle(sk, x, y, lh + LEG_PEG_H / 2, LEG_PEG_D / 2)
        b.sym(b.all_profiles(sk), LEG_PEG_H / 2, JOIN, [leg])
        try:                                               # round the foot tip
            tip = None
            for e in leg.edges:
                bb = e.boundingBox
                if abs(bb.maxPoint.z) < 1e-6 and abs(bb.minPoint.z) < 1e-6:
                    tip = e
            if tip:
                ec = adsk.core.ObjectCollection.create()
                ec.add(tip)
                fi = comp.features.filletFeatures.createInput()
                fi.addConstantRadiusEdgeSet(ec, v(LEG_TIP_R), True)
                comp.features.filletFeatures.add(fi)
        except Exception:
            pass

    if PRINT_LAYOUT:
        print_layout(comp, [n for n, _ in specs])

    app.activeViewport.fit()


def _move(comp, body, matrix):
    mf = comp.features.moveFeatures
    oc = adsk.core.ObjectCollection.create()
    oc.add(body)
    try:
        inp = mf.createInput2(oc)
        inp.defineAsFreeMove(matrix)
    except AttributeError:                                 # older Fusion API
        inp = mf.createInput(oc, matrix)
    mf.add(inp)


def _place(comp, name, angle_deg, x, y):
    """Rotate body `name` about the X axis, then drop it on the bed with its
    bounding-box corner at (x, y). Returns its footprint (w, d) in mm."""
    body = comp.bRepBodies.itemByName(name)
    if angle_deg:
        m = adsk.core.Matrix3D.create()
        m.setToRotation(math.radians(angle_deg), adsk.core.Vector3D.create(1, 0, 0),
                        adsk.core.Point3D.create(0, 0, 0))
        _move(comp, body, m)
        body = comp.bRepBodies.itemByName(name)
    bb = body.boundingBox
    t = adsk.core.Matrix3D.create()
    t.translation = adsk.core.Vector3D.create(x * MM - bb.minPoint.x, y * MM - bb.minPoint.y,
                                              -bb.minPoint.z)
    _move(comp, body, t)
    return ((bb.maxPoint.x - bb.minPoint.x) / MM, (bb.maxPoint.y - bb.minPoint.y) / MM)


def print_layout(comp, leg_names):
    """Every piece in its support-free print orientation, lying on Z = 0:
       body  -> front face down (rotate +90 about X: model Y becomes height)
       lid   -> outer face down, lip up (rotate -90 about X)
       antenna -> flat (+90 about X)
       legs  -> upside down, peg on the bed (180 about X)."""
    g = PRINT_GAP
    bw, bd = _place(comp, "Cuerpo TV", 90, 0, 0)
    _place(comp, "Tapa trasera", -90, 0, bd + g)
    aw, ad = _place(comp, "Antena", 90, bw + g, 0)
    for i, name in enumerate(leg_names):
        _place(comp, name, 180, bw + g + i * (LEG_D + g), ad + g)


def run(context):
    try:
        build()
        extra = ("\nPiezas colocadas en posición de impresión (Z = 0).\n"
                 "Para ver la tele montada, suprime los últimos 'Mover' de la línea de tiempo."
                 if PRINT_LAYOUT else "")
        ui.messageBox("Mini TV creada: Cuerpo TV, Tapa trasera, Antena y 4 patas.\n"
                      "Comprueba las medidas de tu pantalla en el script si algo no encaja." + extra)
    except Exception:
        ui.messageBox("Error:\n{}".format(traceback.format_exc()))
