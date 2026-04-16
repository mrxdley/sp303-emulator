#include "raylib.h"
#include "sp303.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <algorithm>
#include <unordered_map>

using json = nlohmann::json;

// ─── Screen ───────────────────────────────────────────────────────────────────

static const int SW = 1280;
static const int SH =  780;

// ─── Colors ───────────────────────────────────────────────────────────────────

static const Color C_BG            = {  18,  18,  18, 255 };
static const Color C_UNLIT         = {  75,  75,  75, 255 };
static const Color C_LIT           = { 180,  35,  35, 255 };
static const Color C_PRESSED       = {  45,  45,  45, 255 };
static const Color C_LIT_PRESSED   = { 110,  18,  18, 255 }; // darker red
static const Color C_BORDER        = {  30,  30,  30, 255 };
static const Color C_TEXT          = { 215, 215, 215, 255 };
static const Color C_ALT           = { 120, 120, 120, 255 };
static const Color C_SEG_ON        = { 200,  40,  40, 255 };
static const Color C_SEG_OFF       = {  40,  12,  12, 255 };
static const Color C_PEAK_ON       = { 220,  40,  40, 255 };
static const Color C_PEAK_OFF      = {  55,  18,  18, 255 };
static const Color C_DRAG          = {  60,  90, 180, 255 };
static const Color C_DISP_BG       = {  10,   5,   5, 255 };
static const Color C_KNOB_TRACK    = {  40,  40,  40, 255 };
static const Color C_KNOB_FILL     = { 160,  30,  30, 255 };
static const Color C_KNOB_THUMB    = { 210, 210, 210, 255 };

static const char* LAYOUT_FILE  = "layout.json";
static const char* KEYMAP_FILE  = "keymap.json";

// ─── Layout structs ───────────────────────────────────────────────────────────

struct BtnPos  { int x, y, w, h; };
struct IndPos  { int x, y, r;    };
struct DispPos { int x, y, dw, dh, gap; };
struct KnobPos { int x, y, len;  };  // horizontal slider, value maps 0..1 over len px

struct Layout {
    BtnPos  buttons[SP303_BTN_COUNT]; // pads 9–32 share positions with 1–8 (see renderer)
    IndPos  peak;
    DispPos disp;
    KnobPos knobs[SP303_KNOB_COUNT];
};

// ─── Keymap ───────────────────────────────────────────────────────────────────
//
// Format: { "a": "PAD_1", "space": "REC", ... }
// Key names : single lowercase letter, digit, or named key (see key_from_name).
// Button names: enum without "SP303_BTN_" prefix (e.g. "PAD_1", "BANK_A", "REC").

// Resolve a key-name string to a raylib KeyboardKey (0 = unknown).
static int key_from_name(const std::string& s) {
    if (s.size() == 1) {
        char c = s[0];
        if (c >= 'a' && c <= 'z') return KEY_A + (c - 'a');
        if (c >= '0' && c <= '9') return KEY_ZERO + (c - '0');
    }
    if (s == "space")      return KEY_SPACE;
    if (s == "enter")      return KEY_ENTER;
    if (s == "backspace")  return KEY_BACKSPACE;
    if (s == "tab")        return KEY_TAB;
    if (s == "escape")     return KEY_ESCAPE;
    if (s == "delete")     return KEY_DELETE;
    if (s == "up")         return KEY_UP;
    if (s == "down")       return KEY_DOWN;
    if (s == "left")       return KEY_LEFT;
    if (s == "right")      return KEY_RIGHT;
    if (s == "minus")      return KEY_MINUS;
    if (s == "equal")      return KEY_EQUAL;
    if (s == "semicolon")  return KEY_SEMICOLON;
    if (s == "comma")      return KEY_COMMA;
    if (s == "period")     return KEY_PERIOD;
    if (s == "slash")      return KEY_SLASH;
    for (int i = 1; i <= 12; ++i)
        if (s == "f" + std::to_string(i)) return KEY_F1 + (i - 1);
    return 0;
}

// Resolve a button-name string to SP303ButtonID (-1 = unknown).
static SP303ButtonID btn_from_name(const std::string& s) {
    static std::unordered_map<std::string, SP303ButtonID> tbl;
    static bool ready = false;
    if (!ready) {
        ready = true;
        tbl = {
            {"FILTER_DRIVE",    SP303_BTN_FILTER_DRIVE},
            {"PITCH",           SP303_BTN_PITCH},
            {"DELAY",           SP303_BTN_DELAY},
            {"MFX",             SP303_BTN_MFX},
            {"VINYL_SIM",       SP303_BTN_VINYL_SIM},
            {"ISOLATOR",        SP303_BTN_ISOLATOR},
            {"START_END_LEVEL", SP303_BTN_START_END_LEVEL},
            {"TIME_BPM",        SP303_BTN_TIME_BPM},
            {"PATTERN_SELECT",  SP303_BTN_PATTERN_SELECT},
            {"LENGTH",          SP303_BTN_LENGTH},
            {"QUANTIZE",        SP303_BTN_QUANTIZE},
            {"TAP_TEMPO",       SP303_BTN_TAP_TEMPO},
            {"CANCEL",          SP303_BTN_CANCEL},
            {"REMAIN",          SP303_BTN_REMAIN},
            {"LONG_LOFI",       SP303_BTN_LONG_LOFI},
            {"STEREO",          SP303_BTN_STEREO},
            {"GATE",            SP303_BTN_GATE},
            {"LOOP",            SP303_BTN_LOOP},
            {"REVERSE",         SP303_BTN_REVERSE},
            {"DEL",             SP303_BTN_DEL},
            {"REC",             SP303_BTN_REC},
            {"RESAMPLE",        SP303_BTN_RESAMPLE},
            {"MARK",            SP303_BTN_MARK},
            {"BANK_A",          SP303_BTN_BANK_A},
            {"BANK_B",          SP303_BTN_BANK_B},
            {"BANK_C",          SP303_BTN_BANK_C},
            {"BANK_D",          SP303_BTN_BANK_D},
            {"HOLD",            SP303_BTN_HOLD},
            {"EXT_SOURCE",      SP303_BTN_EXT_SOURCE},
        };
        for (int i = 1; i <= 32; ++i)
            tbl["PAD_" + std::to_string(i)] = (SP303ButtonID)(SP303_BTN_PAD_1 + i - 1);
    }
    auto it = tbl.find(s);
    return it != tbl.end() ? it->second : (SP303ButtonID)-1;
}

// Write the default keymap file and return the parsed mapping.
static std::unordered_map<int, SP303ButtonID> write_default_keymap() {
    json j = {
        {"a", "PAD_1"}, {"s", "PAD_2"}, {"d", "PAD_3"}, {"f", "PAD_4"},
        {"z", "PAD_5"}, {"x", "PAD_6"}, {"c", "PAD_7"}, {"v", "PAD_8"},
        {"1", "BANK_A"}, {"2", "BANK_B"}, {"3", "BANK_C"}, {"4", "BANK_D"},
        {"space",     "REC"},
        {"enter",     "PATTERN_SELECT"},
        {"backspace", "DEL"},
        {"escape",    "CANCEL"},
    };
    std::ofstream f(KEYMAP_FILE);
    f << j.dump(2);

    std::unordered_map<int, SP303ButtonID> out;
    for (auto& [k, v] : j.items()) {
        int          key = key_from_name(k);
        SP303ButtonID btn = btn_from_name(v.get<std::string>());
        if (key && btn != (SP303ButtonID)-1) out[key] = btn;
    }
    return out;
}

// Load keymap.json; writes a default if the file is missing or invalid.
static std::unordered_map<int, SP303ButtonID> load_keymap() {
    std::ifstream f(KEYMAP_FILE);
    if (!f.is_open()) return write_default_keymap();
    try {
        json j = json::parse(f);
        std::unordered_map<int, SP303ButtonID> out;
        for (auto& [k, v] : j.items()) {
            int           key = key_from_name(k);
            SP303ButtonID btn = btn_from_name(v.get<std::string>());
            if (key && btn != (SP303ButtonID)-1) out[key] = btn;
        }
        return out;
    } catch (...) {
        return write_default_keymap();
    }
}

// ─── Default layout ───────────────────────────────────────────────────────────

static Layout build_default_layout() {
    Layout L{};

    const int BW = 92, BH = 38, BG = 6;
    const int SX = BW + BG;
    const int SY = BH + BG;
    const int OX = 20, OY = 200;  // leave room for knobs at top

    auto place = [&](SP303ButtonID id, int col, int row) {
        L.buttons[id] = { OX + col*SX, OY + row*SY, BW, BH };
    };

    // Row 0: DSP effects
    place(SP303_BTN_FILTER_DRIVE,    0, 0);
    place(SP303_BTN_PITCH,           1, 0);
    place(SP303_BTN_DELAY,           2, 0);
    place(SP303_BTN_MFX,             3, 0);
    place(SP303_BTN_VINYL_SIM,       4, 0);
    place(SP303_BTN_ISOLATOR,        5, 0);

    // Row 1: Sample edit | gap | Pattern | gap | Transport
    place(SP303_BTN_START_END_LEVEL, 0, 1);
    place(SP303_BTN_TIME_BPM,        1, 1);
    place(SP303_BTN_PATTERN_SELECT,  3, 1);
    place(SP303_BTN_LENGTH,          4, 1);
    place(SP303_BTN_QUANTIZE,        5, 1);
    place(SP303_BTN_TAP_TEMPO,       7, 1);
    place(SP303_BTN_CANCEL,          8, 1);
    place(SP303_BTN_REMAIN,          9, 1);

    // Row 2: Sample mode | Recording
    place(SP303_BTN_LONG_LOFI,  0, 2);
    place(SP303_BTN_STEREO,     1, 2);
    place(SP303_BTN_GATE,       2, 2);
    place(SP303_BTN_LOOP,       3, 2);
    place(SP303_BTN_REVERSE,    4, 2);
    place(SP303_BTN_DEL,        5, 2);
    place(SP303_BTN_REC,        6, 2);
    place(SP303_BTN_RESAMPLE,   8, 2);
    place(SP303_BTN_MARK,       9, 2);

    // Row 3: Bank | gap | Special
    place(SP303_BTN_BANK_A,     0, 3);
    place(SP303_BTN_BANK_B,     1, 3);
    place(SP303_BTN_BANK_C,     2, 3);
    place(SP303_BTN_BANK_D,     3, 3);
    place(SP303_BTN_HOLD,       5, 3);
    place(SP303_BTN_EXT_SOURCE, 6, 3);

    // Rows 4–5: 8 physical pad slots (shared by all banks)
    const int PW = 80, PH = 70, PG = 8;
    const int POY = OY + 4*SY + 4;
    for (int i = 0; i < 8; ++i) {
        BtnPos p = { OX + i*(PW+PG), POY, PW, PH };
        // All 32 pad IDs share the first 8 positions
        L.buttons[SP303_BTN_PAD_1 + i]      = p;
        L.buttons[SP303_BTN_PAD_1 + 8  + i] = p;
        L.buttons[SP303_BTN_PAD_1 + 16 + i] = p;
        L.buttons[SP303_BTN_PAD_1 + 24 + i] = p;
    }

    // 7-seg display — top right
    const int DW = 28, DH = 52, DG = 6;
    L.disp = { OX + 7*SX, 30, DW, DH, DG };

    // PEAK indicator — right of display
    L.peak = { L.disp.x + 3*DW + 2*DG + 26, 30 + DH/2, 10 };

    // Knobs — horizontal sliders, top area
    const int KY = 90, KLEN = 160, KSX = 220;
    for (int i = 0; i < SP303_KNOB_COUNT; ++i)
        L.knobs[i] = { OX + i*KSX, KY, KLEN };

    return L;
}

// ─── JSON persistence ─────────────────────────────────────────────────────────

static void save_layout(const Layout& L) {
    json j;
    // Only save the first 8 pad positions; the rest are mirrors
    for (int i = 0; i < SP303_BTN_COUNT; ++i) {
        if (i > SP303_BTN_PAD_8 && i <= SP303_BTN_PAD_32) continue;
        const auto& b = L.buttons[i];
        j["buttons"][std::to_string(i)] = { {"x",b.x},{"y",b.y},{"w",b.w},{"h",b.h} };
    }
    j["peak"]    = { {"x",L.peak.x}, {"y",L.peak.y}, {"r",L.peak.r} };
    j["display"] = { {"x",L.disp.x}, {"y",L.disp.y},
                     {"dw",L.disp.dw}, {"dh",L.disp.dh}, {"gap",L.disp.gap} };
    for (int i = 0; i < SP303_KNOB_COUNT; ++i) {
        const auto& k = L.knobs[i];
        j["knobs"][std::to_string(i)] = { {"x",k.x},{"y",k.y},{"len",k.len} };
    }
    std::ofstream f(LAYOUT_FILE);
    f << j.dump(2);
}

static Layout load_layout() {
    std::ifstream f(LAYOUT_FILE);
    if (!f.is_open()) return build_default_layout();
    try {
        json   j = json::parse(f);
        Layout L = build_default_layout();
        if (j.contains("buttons")) {
            for (int i = 0; i < SP303_BTN_COUNT; ++i) {
                if (i > SP303_BTN_PAD_8 && i <= SP303_BTN_PAD_32) continue;
                auto key = std::to_string(i);
                if (j["buttons"].contains(key)) {
                    auto& r = j["buttons"][key];
                    BtnPos p = { r["x"], r["y"], r["w"], r["h"] };
                    L.buttons[i] = p;
                    // Mirror to the same slot in all banks
                    if (i >= SP303_BTN_PAD_1 && i <= SP303_BTN_PAD_8) {
                        int slot = i - SP303_BTN_PAD_1;
                        L.buttons[SP303_BTN_PAD_1 + 8  + slot] = p;
                        L.buttons[SP303_BTN_PAD_1 + 16 + slot] = p;
                        L.buttons[SP303_BTN_PAD_1 + 24 + slot] = p;
                    }
                }
            }
        }
        if (j.contains("peak"))
            L.peak = { j["peak"]["x"], j["peak"]["y"], j["peak"]["r"] };
        if (j.contains("display")) {
            auto& d = j["display"];
            L.disp = { d["x"], d["y"], d["dw"], d["dh"], d["gap"] };
        }
        if (j.contains("knobs")) {
            for (int i = 0; i < SP303_KNOB_COUNT; ++i) {
                auto key = std::to_string(i);
                if (j["knobs"].contains(key)) {
                    auto& k = j["knobs"][key];
                    L.knobs[i] = { k["x"], k["y"], k["len"] };
                }
            }
        }
        return L;
    } catch (...) {
        return build_default_layout();
    }
}

// ─── 7-Segment drawing ────────────────────────────────────────────────────────

static void draw_7seg(int x, int y, int dw, int dh, uint8_t mask) {
    const int t  = 4;
    const int p  = 2;
    const int hh = dh / 2;
    auto col = [&](uint8_t bit) { return (mask & bit) ? C_SEG_ON : C_SEG_OFF; };
    DrawRectangle(x+t+p,  y,         dw-2*(t+p), t,    col(SP303_SEG_A));
    DrawRectangle(x+dw-t, y+t,       t,          hh-t, col(SP303_SEG_B));
    DrawRectangle(x+dw-t, y+hh,      t,          hh-t, col(SP303_SEG_C));
    DrawRectangle(x+t+p,  y+dh-t,    dw-2*(t+p), t,    col(SP303_SEG_D));
    DrawRectangle(x,      y+hh,      t,          hh-t, col(SP303_SEG_E));
    DrawRectangle(x,      y+t,       t,          hh-t, col(SP303_SEG_F));
    DrawRectangle(x+t+p,  y+hh-t/2,  dw-2*(t+p), t,    col(SP303_SEG_G));
    if (mask & SP303_SEG_DP)
        DrawRectangle(x+dw+2, y+dh-t, t, t, C_SEG_ON);
}

static void draw_display(const DispPos& dp, const SP303Display& disp) {
    int tw = 3*dp.dw + 2*dp.gap;
    DrawRectangle(dp.x-6, dp.y-6, tw+12, dp.dh+12, C_DISP_BG);
    DrawRectangleLines(dp.x-6, dp.y-6, tw+12, dp.dh+12, C_BORDER);
    for (int i = 0; i < 3; ++i)
        draw_7seg(dp.x + i*(dp.dw+dp.gap), dp.y, dp.dw, dp.dh, disp.digit[i]);
}

// ─── Button drawing ───────────────────────────────────────────────────────────

static void draw_button(const BtnPos& r, const SP303ButtonDef& def,
                        const SP303ButtonState& s, bool dragging) {
    Color fill;
    if      ( s.lit &&  s.pressed) fill = C_LIT_PRESSED;
    else if (!s.lit &&  s.pressed) fill = C_PRESSED;
    else if ( s.lit && !s.pressed) fill = C_LIT;
    else                           fill = C_UNLIT;

    DrawRectangle(r.x, r.y, r.w, r.h, fill);
    DrawRectangleLines(r.x, r.y, r.w, r.h, dragging ? C_DRAG : C_BORDER);

    int fs = 9;
    int tw = MeasureText(def.primary, fs);
    while (tw > r.w - 4 && fs > 6) { --fs; tw = MeasureText(def.primary, fs); }

    bool has_alt = (def.alt1 != nullptr);
    int  ty = r.y + (r.h - fs) / 2 - (has_alt ? 5 : 0);
    DrawText(def.primary, r.x + (r.w - tw)/2, ty, fs, C_TEXT);

    if (has_alt) {
        std::string alt = def.alt1;
        if (def.alt2) { alt += "/"; alt += def.alt2; }
        int afs = 7;
        int atw = MeasureText(alt.c_str(), afs);
        while (atw > r.w - 4 && afs > 5) { --afs; atw = MeasureText(alt.c_str(), afs); }
        DrawText(alt.c_str(), r.x + (r.w - atw)/2, r.y + r.h - afs - 3, afs, C_ALT);
    }
}

// ─── Knob (slider) drawing ────────────────────────────────────────────────────

static void draw_knob(const KnobPos& k, const SP303KnobDef& def, float value, bool dragging) {
    const int TH = 10;   // track height
    const int TR = 7;    // thumb radius
    int fill_w = (int)(value * k.len);
    int cx     = k.x + fill_w;
    int cy     = k.y + TH / 2;

    // Label
    const int lfs = 9;
    DrawText(def.primary, k.x, k.y - lfs - 4, lfs, dragging ? C_DRAG : C_TEXT);
    if (def.alt1) {
        std::string alt = def.alt1;
        if (def.alt2) { alt += "/"; alt += def.alt2; }
        int atw = MeasureText(alt.c_str(), 7);
        DrawText(alt.c_str(), k.x + k.len - atw, k.y - 7 - 4, 7, C_ALT);
    }

    // Track
    DrawRectangle(k.x, k.y, k.len, TH, C_KNOB_TRACK);
    DrawRectangle(k.x, k.y, fill_w, TH, dragging ? C_DRAG : C_KNOB_FILL);
    DrawRectangleLines(k.x, k.y, k.len, TH, C_BORDER);

    // Thumb
    DrawCircle(cx, cy, (float)TR, dragging ? C_DRAG : C_KNOB_THUMB);
    DrawCircleLines(cx, cy, (float)TR, C_BORDER);
}

// ─── PEAK indicator ───────────────────────────────────────────────────────────

static void draw_peak(const IndPos& p, bool lit) {
    DrawCircle(p.x, p.y, (float)(p.r + 1), C_BORDER);
    DrawCircle(p.x, p.y, (float)p.r,       lit ? C_PEAK_ON : C_PEAK_OFF);
    const int fs = 8;
    DrawText("PEAK", p.x - MeasureText("PEAK", fs)/2, p.y + p.r + 3, fs, C_ALT);
}

// ─── Hit testing ──────────────────────────────────────────────────────────────

static bool hit_btn(int mx, int my, const BtnPos& r) {
    return mx >= r.x && mx < r.x+r.w && my >= r.y && my < r.y+r.h;
}
static bool hit_circle(int mx, int my, const IndPos& p) {
    int dx = mx-p.x, dy = my-p.y;
    return dx*dx + dy*dy <= p.r*p.r;
}
static bool hit_disp(int mx, int my, const DispPos& dp) {
    int w = 3*dp.dw + 2*dp.gap + 12;
    return mx >= dp.x-6 && mx < dp.x-6+w && my >= dp.y-6 && my < dp.y-6+dp.dh+12;
}
static bool hit_knob(int mx, int my, const KnobPos& k) {
    return mx >= k.x-6 && mx <= k.x+k.len+6 && my >= k.y-10 && my <= k.y+20;
}

// ─── Drag targets ─────────────────────────────────────────────────────────────

static const int DRAG_NONE   = -1;
static const int DRAG_PEAK   = SP303_BTN_COUNT;
static const int DRAG_DISP   = SP303_BTN_COUNT + 1;
static const int DRAG_KNOB_0 = SP303_BTN_COUNT + 2; // +3, +4, +5 for knobs 1–3

struct Drag {
    int target = DRAG_NONE;
    int offx   = 0;
    int offy   = 0;
};

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(void) {
    SP303Device* dev    = sp303_create();
    Layout       layout = load_layout();
    Drag         drag;
    int          pressed_btn  = -1;  // button currently held
    int          active_knob  = -1;  // knob currently being value-dragged

    auto keymap = load_keymap();
    // Track which actual button each key fired (bank may change between press/release)
    std::unordered_map<int, SP303ButtonID> key_held;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SW, SH, "SP-303");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();
        int  mx    = (int)mouse.x;
        int  my    = (int)mouse.y;
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        // ── Press ─────────────────────────────────────────────────────────────
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (shift) {
                // Drag to reposition
                for (int i = 0; i < SP303_BTN_COUNT && drag.target == DRAG_NONE; ++i) {
                    // For pads 9–32 skip — they share positions with 1–8
                    if (i > SP303_BTN_PAD_8 && i <= SP303_BTN_PAD_32) continue;
                    if (hit_btn(mx, my, layout.buttons[i]))
                        drag = { i, mx - layout.buttons[i].x, my - layout.buttons[i].y };
                }
                if (drag.target == DRAG_NONE && hit_circle(mx, my, layout.peak))
                    drag = { DRAG_PEAK, mx - layout.peak.x, my - layout.peak.y };
                if (drag.target == DRAG_NONE && hit_disp(mx, my, layout.disp))
                    drag = { DRAG_DISP, mx - layout.disp.x, my - layout.disp.y };
                for (int i = 0; i < SP303_KNOB_COUNT && drag.target == DRAG_NONE; ++i) {
                    if (hit_knob(mx, my, layout.knobs[i]))
                        drag = { DRAG_KNOB_0 + i, mx - layout.knobs[i].x, my - layout.knobs[i].y };
                }
            } else {
                // Knob value drag
                for (int i = 0; i < SP303_KNOB_COUNT; ++i) {
                    if (hit_knob(mx, my, layout.knobs[i])) {
                        active_knob = i;
                        float v = std::clamp((float)(mx - layout.knobs[i].x) / layout.knobs[i].len, 0.0f, 1.0f);
                        sp303_knob_set(dev, (SP303KnobID)i, v);
                        break;
                    }
                }
                if (active_knob < 0) {
                    // Read current state to know active bank before firing pad events
                    SP303State cur = sp303_get_state(dev);
                    // Pads (8 physical positions, mapped to active bank)
                    for (int i = 0; i < 8; ++i) {
                        if (hit_btn(mx, my, layout.buttons[SP303_BTN_PAD_1 + i])) {
                            int pad_id = SP303_BTN_PAD_1 + cur.active_bank * 8 + i;
                            sp303_button_down(dev, (SP303ButtonID)pad_id);
                            pressed_btn = pad_id;
                            break;
                        }
                    }
                    // All other buttons
                    if (pressed_btn < 0) {
                        for (int i = 0; i < SP303_BTN_COUNT; ++i) {
                            if (i >= SP303_BTN_PAD_1 && i <= SP303_BTN_PAD_32) continue;
                            if (hit_btn(mx, my, layout.buttons[i])) {
                                sp303_button_down(dev, (SP303ButtonID)i);
                                pressed_btn = i;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // ── Release ───────────────────────────────────────────────────────────
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (drag.target != DRAG_NONE) {
                drag.target = DRAG_NONE;
                save_layout(layout);
            }
            if (pressed_btn >= 0) {
                sp303_button_up(dev, (SP303ButtonID)pressed_btn);
                pressed_btn = -1;
            }
            active_knob = -1;
        }

        // ── Continuous drag ───────────────────────────────────────────────────
        if (drag.target != DRAG_NONE) {
            int nx = mx - drag.offx;
            int ny = my - drag.offy;
            int t  = drag.target;
            if (t < SP303_BTN_COUNT) {
                layout.buttons[t].x = nx;
                layout.buttons[t].y = ny;
                // Mirror pad repositions to all banks
                if (t >= SP303_BTN_PAD_1 && t <= SP303_BTN_PAD_8) {
                    int slot = t - SP303_BTN_PAD_1;
                    BtnPos p = layout.buttons[t];
                    layout.buttons[SP303_BTN_PAD_1 + 8  + slot] = p;
                    layout.buttons[SP303_BTN_PAD_1 + 16 + slot] = p;
                    layout.buttons[SP303_BTN_PAD_1 + 24 + slot] = p;
                }
            } else if (t == DRAG_PEAK) {
                layout.peak.x = nx; layout.peak.y = ny;
            } else if (t == DRAG_DISP) {
                layout.disp.x = nx; layout.disp.y = ny;
            } else if (t >= DRAG_KNOB_0 && t < DRAG_KNOB_0 + SP303_KNOB_COUNT) {
                layout.knobs[t - DRAG_KNOB_0].x = nx;
                layout.knobs[t - DRAG_KNOB_0].y = ny;
            }
        }

        // Knob value drag (continuous, separate from repositioning)
        if (active_knob >= 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float v = std::clamp((float)(mx - layout.knobs[active_knob].x) / layout.knobs[active_knob].len, 0.0f, 1.0f);
            sp303_knob_set(dev, (SP303KnobID)active_knob, v);
        }

        // ── Keyboard ──────────────────────────────────────────────────────────
        // Process before tick so the state update sees the press this frame.
        // PAD_1..8 in the keymap are treated as physical slots — they remap
        // through active_bank so bank switches take effect immediately.
        if (!shift) {
            SP303State cur = sp303_get_state(dev);
            for (auto& [key, btn] : keymap) {
                if (IsKeyPressed(key)) {
                    SP303ButtonID actual = btn;
                    if (btn >= SP303_BTN_PAD_1 && btn <= SP303_BTN_PAD_8)
                        actual = (SP303ButtonID)(SP303_BTN_PAD_1 + cur.active_bank * 8 + (btn - SP303_BTN_PAD_1));
                    sp303_button_down(dev, actual);
                    key_held[key] = actual;
                }
                if (IsKeyReleased(key)) {
                    auto it = key_held.find(key);
                    if (it != key_held.end()) {
                        sp303_button_up(dev, it->second);
                        key_held.erase(it);
                    }
                }
            }
        }

        // ── Tick + state ──────────────────────────────────────────────────────
        sp303_tick(dev, 735);
        SP303State state = sp303_get_state(dev);

        // ── Draw ──────────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(C_BG);

        // Non-pad buttons
        for (int i = 0; i < SP303_BTN_COUNT; ++i) {
            if (i >= SP303_BTN_PAD_1 && i <= SP303_BTN_PAD_32) continue;
            draw_button(layout.buttons[i], SP303_BUTTON_DEFS[i],
                        state.buttons[i], drag.target == i);
        }

        // 8 physical pad positions → active bank's pads
        int bank_off = state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            int pad_id    = SP303_BTN_PAD_1 + bank_off + i;
            int layout_id = SP303_BTN_PAD_1 + i;
            draw_button(layout.buttons[layout_id], SP303_BUTTON_DEFS[pad_id],
                        state.buttons[pad_id], drag.target == layout_id);
        }

        // Knobs
        for (int i = 0; i < SP303_KNOB_COUNT; ++i)
            draw_knob(layout.knobs[i], SP303_KNOB_DEFS[i],
                      state.knobs[i].value, active_knob == i || drag.target == DRAG_KNOB_0 + i);

        // Display + PEAK
        draw_display(layout.disp, state.display);
        draw_peak(layout.peak, state.indicators[SP303_IND_PEAK].lit);

        if (shift)
            DrawText("SHIFT + drag: reposition  |  release: save", 10, SH - 18, 9, C_ALT);

        EndDrawing();
    }

    save_layout(layout);
    sp303_destroy(dev);
    CloseWindow();
    return 0;
}
