// tools/snapshots/snap.cpp -- renders the Habitacion screen with the real
// firmware drawing code (room_scene, cat_draw, screen_room, chrome) into
// PPM files: one still per scenario plus frame sequences for animated GIFs.
// Built and run by snap.py; see README.md there.
#include <TFT_eSPI.h>
#include <cstdio>
#include <string>
#include "theme.h"
#include "fonts.h"
#include "chrome.h"
#include "room_scene.h"
#include "screen_room.h"
#include "cat_draw.h"
#include "logic/cat_state.h"
#include "logic/cat_behavior.h"
#include "logic/room.h"
#include "config.h"

TFT_eSPI tft;
void hostSetMillis(uint32_t ms);

struct Scn {
    int hour, minute;
    float temp, hum;
    int wxCode; float outTemp;   // wxCode < 0: no weather
    CatMood mood;
    CatAct act; float phase; int look; int facing;
    int x;
    bool toast;
    uint32_t moodSince;          // PETTED start
};

static void render(const Scn& s, uint32_t now) {
    hostSetMillis(now);
    RoomReading room;
    room.tempC = s.temp; room.humidity = s.hum; room.everOk = true; room.lastOkMs = now;
    const Comfort comfort = classifyTemp(s.temp);

    // Header (or the new-task toast over it).
    if (s.toast) {
        drawToast("Nueva tarea", "Regar las plantas", C_OK);
    } else {
        HeaderInfo hi = {};
        hi.title = "HABITACIÓN"; hi.online = true; hi.timeKnown = true;
        hi.hour = s.hour; hi.minute = s.minute; hi.room = &room; hi.roomFresh = true;
        hi.bg = C_BG; hi.showTime = false;
        drawHeader(hi);
    }

    SceneInfo sc = {};
    sc.timeKnown = true; sc.hour = s.hour; sc.minute = s.minute;
    sc.wday = 3; sc.mday = 4; sc.mon = 10;                    // mié 4 nov
    sc.weatherOk = s.wxCode >= 0; sc.weatherCode = s.wxCode; sc.outTemp = s.outTemp;
    sc.night = s.hour < 7 || s.hour >= 20;
    sc.comfort = comfort;
    sceneSetLook(sc.night, sc.comfort);
    drawRoomScreen();
    drawRoomStrip(room, true);
    drawRoomClock(sc);
    drawSceneAnim(now);

    CatPose p;
    p.mood = s.mood; p.blink = false;
    p.humid = classifyHumidity(s.hum);
    p.body = comfortColor(comfort);
    p.nowMs = now; p.moodSince = s.moodSince;
    p.act = s.act; p.actPhase = s.phase; p.facing = (int8_t)s.facing; p.look = (int8_t)s.look;
    p.comfort = comfort;
    p.envelope = s.toast && s.mood != CatMood::SLEEPING;
    catDraw(p, s.x, ROOM_CAT_Y);
    drawPageDots(0, 2, C_BG);
}

static void save(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::perror(path.c_str()); return; }
    std::fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    for (uint16_t c : tft.pixels()) {
        uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63),
                           (uint8_t)((c & 31) * 255 / 31) };
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
}

static Scn base() {
    Scn s = {};
    s.hour = 11; s.minute = 5; s.temp = 22; s.hum = 48; s.wxCode = 2; s.outTemp = 13;
    s.mood = CatMood::IDLE; s.act = CatAct::SIT; s.phase = 0; s.look = 0; s.facing = 1;
    s.x = ROOM_CAT_X; s.toast = false; s.moodSince = 0;
    return s;
}

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : ".";
    catDrawInit();
    sceneInit();

    // ---- stills ------------------------------------------------------------
    { Scn s = base(); s.act = CatAct::LOOK; s.phase = 0.5f; s.look = 1;
      render(s, 1200); save(out + "/dia.ppm"); }
    { Scn s = base(); s.hour = 2; s.minute = 14; s.wxCode = 0; s.outTemp = 6; s.mood = CatMood::SLEEPING;
      render(s, 2400); save(out + "/noche.ppm"); }
    { Scn s = base(); s.hour = 9; s.minute = 30; s.temp = 16; s.hum = 50; s.wxCode = 71; s.outTemp = -1;
      render(s, 1500); save(out + "/frio.ppm"); }
    { Scn s = base(); s.hour = 16; s.minute = 45; s.temp = 29; s.hum = 72; s.wxCode = 0; s.outTemp = 31;
      render(s, 900); save(out + "/calor.ppm"); }
    { Scn s = base(); s.mood = CatMood::PETTED; s.moodSince = 0;
      render(s, 700); save(out + "/carino.ppm"); }
    { Scn s = base(); s.toast = true;
      render(s, 600); save(out + "/tarea.ppm"); }

    // ---- day animation: every idle act, then a pet ---------------------------
    {
        struct Step { CatAct act; uint32_t ms; int toX; int look; };
        const Step steps[] = {
            { CatAct::SIT, 1200, 0, 0 },     { CatAct::WALK, 0, ROOM_BOT_MIN_X + 20, 0 },
            { CatAct::LOOK, 2000, 0, 1 },    { CatAct::GROOM, 2200, 0, 0 },
            { CatAct::WALK, 0, ROOM_CAT_X, 0 }, { CatAct::YAWN, 1600, 0, 0 },
            { CatAct::STRETCH, 1800, 0, 0 }, { CatAct::HOP, 700, 0, 0 },
            { CatAct::HOP, 700, 0, 0 },      { CatAct::SIT, 800, 0, 0 },
        };
        const uint32_t FRAME = 100;
        Scn s = base();
        uint32_t t = 0; int n = 0;
        char name[64];
        for (const Step& st : steps) {
            uint32_t dur = st.ms;
            int fromX = s.x;
            if (st.act == CatAct::WALK) {
                dur = (uint32_t)(abs(st.toX - fromX) * 1000 / CAT_WALK_PX_PER_S);
                s.facing = st.toX > fromX ? 1 : -1;
            }
            for (uint32_t e = 0; e < dur; e += FRAME, t += FRAME) {
                s.act = st.act; s.phase = (float)e / (float)dur; s.look = st.look;
                if (st.act == CatAct::WALK) s.x = fromX + s.facing * (int)(e * CAT_WALK_PX_PER_S / 1000);
                render(s, t);
                std::snprintf(name, sizeof(name), "/anim_dia_%03d.ppm", n++);
                save(out + name);
            }
            if (st.act == CatAct::WALK) s.x = st.toX;
        }
        s.act = CatAct::SIT; s.mood = CatMood::PETTED; s.moodSince = t;
        for (uint32_t e = 0; e < PET_MS; e += FRAME, t += FRAME) {
            render(s, t);
            std::snprintf(name, sizeof(name), "/anim_dia_%03d.ppm", n++);
            save(out + name);
        }
    }

    // ---- night animation: asleep -----------------------------------------------
    {
        Scn s = base(); s.hour = 2; s.minute = 14; s.wxCode = 0; s.outTemp = 6; s.mood = CatMood::SLEEPING;
        char name[64];
        int n = 0;
        for (uint32_t t = 0; t < 4000; t += 100) {
            render(s, t);
            std::snprintf(name, sizeof(name), "/anim_noche_%03d.ppm", n++);
            save(out + name);
        }
    }
    return 0;
}
