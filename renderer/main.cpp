#include "raylib.h"
#include "sp303.h"
#include "controller.h"
#include "project_io.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <cmath>

using json = nlohmann::json;

// ─── Screen ───────────────────────────────────────────────────────────────────

static const int SW = 1280;
static const int SH =  780;
static const int DRAG_GRID = 10;

// ─── Colors ───────────────────────────────────────────────────────────────────

static const Color C_BG            = {  18,  18,  18, 255 };
static const Color C_UNLIT         = {  75,  75,  75, 255 };
static const Color C_LIT           = { 180,  35,  35, 255 };
static const Color C_PRESSED       = {  45,  45,  45, 255 };
static const Color C_LIT_PRESSED   = { 110,  18,  18, 255 };
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
static const char* QUICKSAVE_DIR = "projects/quicksave";

// ─── Layout structs ───────────────────────────────────────────────────────────

struct BtnPos  { int x, y, w, h; };
struct IndPos  { int x, y, r;    };
struct DispPos { int x, y, dw, dh, gap; };
struct KnobPos { int x, y, len;  };

struct Layout {
    BtnPos  buttons[sp303::BTN_COUNT];
    IndPos  peak;
    DispPos disp;
    KnobPos knobs[sp303::KNOB_COUNT];
};

// ─── Keymap ───────────────────────────────────────────────────────────────────

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

static sp303::ButtonID btn_from_name(const std::string& s) {
    static std::unordered_map<std::string, sp303::ButtonID> tbl;
    static bool ready = false;
    if (!ready) {
        ready = true;
        tbl = {
            {"FILTER_DRIVE",    sp303::BTN_FILTER_DRIVE},
            {"PITCH",           sp303::BTN_PITCH},
            {"DELAY",           sp303::BTN_DELAY},
            {"MFX",             sp303::BTN_MFX},
            {"VINYL_SIM",       sp303::BTN_VINYL_SIM},
            {"ISOLATOR",        sp303::BTN_ISOLATOR},
            {"START_END_LEVEL", sp303::BTN_START_END_LEVEL},
            {"TIME_BPM",        sp303::BTN_TIME_BPM},
            {"PATTERN_SELECT",  sp303::BTN_PATTERN_SELECT},
            {"LENGTH",          sp303::BTN_LENGTH},
            {"QUANTIZE",        sp303::BTN_QUANTIZE},
            {"TAP_TEMPO",       sp303::BTN_TAP_TEMPO},
            {"CANCEL",          sp303::BTN_CANCEL},
            {"REMAIN",          sp303::BTN_REMAIN},
            {"LONG_LOFI",       sp303::BTN_LONG_LOFI},
            {"STEREO",          sp303::BTN_STEREO},
            {"GATE",            sp303::BTN_GATE},
            {"LOOP",            sp303::BTN_LOOP},
            {"REVERSE",         sp303::BTN_REVERSE},
            {"DEL",             sp303::BTN_DEL},
            {"REC",             sp303::BTN_REC},
            {"RESAMPLE",        sp303::BTN_RESAMPLE},
            {"MARK",            sp303::BTN_MARK},
            {"BANK_A",          sp303::BTN_BANK_A},
            {"BANK_B",          sp303::BTN_BANK_B},
            {"BANK_C",          sp303::BTN_BANK_C},
            {"BANK_D",          sp303::BTN_BANK_D},
            {"HOLD",            sp303::BTN_HOLD},
            {"EXT_SOURCE",      sp303::BTN_EXT_SOURCE},
        };
        for (int i = 1; i <= 32; ++i)
            tbl["PAD_" + std::to_string(i)] = (sp303::ButtonID)(sp303::BTN_PAD_1 + i - 1);
    }
    auto it = tbl.find(s);
    return it != tbl.end() ? it->second : (sp303::ButtonID)-1;
}

static std::unordered_map<int, sp303::ButtonID> write_default_keymap() {
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

    std::unordered_map<int, sp303::ButtonID> out;
    for (auto& [k, v] : j.items()) {
        int              key = key_from_name(k);
        sp303::ButtonID  btn = btn_from_name(v.get<std::string>());
        if (key && btn != (sp303::ButtonID)-1) out[key] = btn;
    }
    return out;
}

static std::unordered_map<int, sp303::ButtonID> load_keymap() {
    std::ifstream f(KEYMAP_FILE);
    if (!f.is_open()) return write_default_keymap();
    try {
        json j = json::parse(f);
        std::unordered_map<int, sp303::ButtonID> out;
        for (auto& [k, v] : j.items()) {
            int             key = key_from_name(k);
            sp303::ButtonID btn = btn_from_name(v.get<std::string>());
            if (key && btn != (sp303::ButtonID)-1) out[key] = btn;
        }
        return out;
    } catch (...) {
        return write_default_keymap();
    }
}

// ─── Config screen ────────────────────────────────────────────────────────────

static const uint32_t SAMPLE_RATES[] = {44100, 48000, 96000};
static const uint32_t BUFFER_SIZES[] = {128, 256, 512, 1024, 2048};
static const int      N_RATES        = 3;
static const int      N_BUFS         = 5;

static bool draw_config_screen(
    int& sel_out, int& sel_in, int& sel_rate, int& sel_buf, float& peak_threshold,
    const std::vector<sp303::AudioDeviceInfo>& out_devs,
    const std::vector<sp303::AudioDeviceInfo>& in_devs,
    bool playback_ok, int mx, int my, bool clicked, bool mouse_down,
    float input_peak)
{
    DrawRectangle(0, 0, SW, SH, {0, 0, 0, 170});

    const int PX = 160, PY = 140, PW = 960, PH = 440;
    DrawRectangle(PX, PY, PW, PH, {22, 22, 22, 255});
    DrawRectangleLines(PX, PY, PW, PH, C_BORDER);

    DrawText("AUDIO CONFIGURATION", PX + 24, PY + 18, 14, C_TEXT);
    DrawText("[TAB] close", PX + PW - 90, PY + 18, 9, C_ALT);

    auto selector = [&](int row, const char* label, const std::string& value) -> int {
        const int RY  = PY + 72 + row * 72;
        const int LX  = PX + 24;
        const int VX  = LX + 150;
        const int BW2 = 26, BH2 = 26;

        DrawText(label, LX, RY + 5, 10, C_ALT);

        DrawRectangle(VX, RY, BW2, BH2, C_UNLIT);
        DrawText("<", VX + 9, RY + 6, 10, C_TEXT);

        const int max_val_w = PW - 330;
        std::string disp = value;
        while (MeasureText(disp.c_str(), 10) > max_val_w && disp.size() > 1)
            disp = disp.substr(0, disp.size() - 1);
        DrawText(disp.c_str(), VX + BW2 + 8, RY + 6, 10, C_TEXT);

        const int NX = VX + BW2 + 8 + MeasureText(disp.c_str(), 10) + 8;
        DrawRectangle(NX, RY, BW2, BH2, C_UNLIT);
        DrawText(">", NX + 8, RY + 6, 10, C_TEXT);

        if (!clicked) return 0;
        if (mx >= VX && mx < VX + BW2 && my >= RY && my < RY + BH2) return -1;
        if (mx >= NX && mx < NX + BW2 && my >= RY && my < RY + BH2) return +1;
        return 0;
    };

    std::string out_name = out_devs.empty() ? "(no devices)" :
        (out_devs[sel_out].is_default
            ? std::string(out_devs[sel_out].name) + "  [default]"
            : out_devs[sel_out].name);
    int d = selector(0, "Output device:", out_name);
    if (d && !out_devs.empty())
        sel_out = (sel_out + d + (int)out_devs.size()) % (int)out_devs.size();

    std::string in_name = in_devs.empty() ? "(no devices)" :
        (in_devs[sel_in].is_default
            ? std::string(in_devs[sel_in].name) + "  [default]"
            : in_devs[sel_in].name);
    d = selector(1, "Input device:", in_name);
    if (d && !in_devs.empty())
        sel_in = (sel_in + d + (int)in_devs.size()) % (int)in_devs.size();

    d = selector(2, "Sample rate:", std::to_string(SAMPLE_RATES[sel_rate]) + " Hz");
    if (d) sel_rate = (sel_rate + d + N_RATES) % N_RATES;

    d = selector(3, "Buffer size:", std::to_string(BUFFER_SIZES[sel_buf]) + " frames");
    if (d) sel_buf = (sel_buf + d + N_BUFS) % N_BUFS;

    const int SRY = PY + 72 + 4 * 72;
    const int SLX = PX + 174;
    const int SLW = PW - 230;
    const int SLH = 12;
    DrawText("Peak threshold:", PX + 24, SRY - 1, 10, C_ALT);
    DrawRectangle(SLX, SRY + 7, SLW, SLH, C_KNOB_TRACK);
    DrawRectangleLines(SLX, SRY + 7, SLW, SLH, C_BORDER);
    float clamped_threshold = std::clamp(peak_threshold, 0.0f, 1.0f);
    int slider_fill_w = (int)(clamped_threshold * SLW);
    if (slider_fill_w > 0)
        DrawRectangle(SLX + 1, SRY + 8, std::max(slider_fill_w - 2, 0), SLH - 2, C_KNOB_FILL);
    int knob_x = SLX + (int)(clamped_threshold * SLW);
    DrawCircle(knob_x, SRY + 13, 8.0f, C_KNOB_THUMB);
    char peak_buf[64];
    std::snprintf(peak_buf, sizeof(peak_buf), "%.3f", clamped_threshold);
    DrawText(peak_buf, SLX + SLW + 12, SRY - 1, 10, C_TEXT);
    if (mouse_down && mx >= SLX && mx < SLX + SLW && my >= SRY && my < SRY + 26) {
        peak_threshold = std::clamp((float)(mx - SLX) / (float)SLW, 0.0f, 1.0f);
    }

    const int METER_Y = PY + 72 + 5 * 72 - 8;
    const int METER_X = PX + 24;
    const int METER_W = PW - 48;
    const int METER_H = 24;

    DrawText("Input level:", METER_X, METER_Y - 18, 10, C_ALT);
    DrawRectangle(METER_X, METER_Y, METER_W, METER_H, C_KNOB_TRACK);
    DrawRectangleLines(METER_X, METER_Y, METER_W, METER_H, C_BORDER);

    float level = std::clamp(input_peak * 2.0f, 0.0f, 1.0f);
    int fill_w = (int)(level * METER_W);

    Color meter_color;
    if (level < 0.5f) {
        meter_color = {50, 180, 50, 255};
    } else if (level < 0.75f) {
        meter_color = {180, 180, 50, 255};
    } else {
        meter_color = {180, 50, 50, 255};
    }

    if (fill_w > 0) {
        DrawRectangle(METER_X + 1, METER_Y + 1, fill_w - 2, METER_H - 2, meter_color);
    }

    if (level > 0.01f) {
        DrawLine(METER_X + fill_w, METER_Y, METER_X + fill_w, METER_Y + METER_H, {255, 255, 255, 200});
    }

    const char* status = playback_ok ? "output: OK" : "output: FAILED — check device";
    DrawText(status, PX + 24, PY + PH - 52, 9, playback_ok ? C_ALT : C_LIT);

    const int ABW = 160, ABH = 38;
    const int ABX = PX + PW/2 - ABW/2;
    const int ABY = PY + PH - 52;
    DrawRectangle(ABX, ABY, ABW, ABH, C_LIT);
    DrawText("APPLY", ABX + ABW/2 - MeasureText("APPLY", 13)/2, ABY + 12, 13, C_TEXT);

    if (clicked && mx >= ABX && mx < ABX + ABW && my >= ABY && my < ABY + ABH)
        return true;
    return false;
}

// ─── Default layout ───────────────────────────────────────────────────────────

static Layout build_default_layout() {
    Layout L{};

    const int BW = 92, BH = 38, BG = 6;
    const int SX = BW + BG;
    const int SY = BH + BG;
    const int OX = 20, OY = 200;

    auto place = [&](sp303::ButtonID id, int col, int row) {
        L.buttons[id] = { OX + col*SX, OY + row*SY, BW, BH };
    };

    place(sp303::BTN_FILTER_DRIVE,    0, 0);
    place(sp303::BTN_PITCH,           1, 0);
    place(sp303::BTN_DELAY,           2, 0);
    place(sp303::BTN_MFX,             3, 0);
    place(sp303::BTN_VINYL_SIM,       4, 0);
    place(sp303::BTN_ISOLATOR,        5, 0);

    place(sp303::BTN_START_END_LEVEL, 0, 1);
    place(sp303::BTN_TIME_BPM,        1, 1);
    place(sp303::BTN_PATTERN_SELECT,  3, 1);
    place(sp303::BTN_LENGTH,          4, 1);
    place(sp303::BTN_QUANTIZE,        5, 1);
    place(sp303::BTN_TAP_TEMPO,       7, 1);
    place(sp303::BTN_CANCEL,          8, 1);
    place(sp303::BTN_REMAIN,          9, 1);

    place(sp303::BTN_LONG_LOFI,  0, 2);
    place(sp303::BTN_STEREO,     1, 2);
    place(sp303::BTN_GATE,       2, 2);
    place(sp303::BTN_LOOP,       3, 2);
    place(sp303::BTN_REVERSE,    4, 2);
    place(sp303::BTN_DEL,        5, 2);
    place(sp303::BTN_REC,        6, 2);
    place(sp303::BTN_RESAMPLE,   8, 2);
    place(sp303::BTN_MARK,       9, 2);

    place(sp303::BTN_BANK_A,     0, 3);
    place(sp303::BTN_BANK_B,     1, 3);
    place(sp303::BTN_BANK_C,     2, 3);
    place(sp303::BTN_BANK_D,     3, 3);
    place(sp303::BTN_HOLD,       5, 3);
    place(sp303::BTN_EXT_SOURCE, 6, 3);

    const int PW = 96, PH = 90, PG = 10;
    const int POY = OY + 4*SY + 10;
    for (int i = 0; i < 4; ++i) {
        BtnPos top = { OX + i*(PW+PG), POY,        PW, PH };
        BtnPos bot = { OX + i*(PW+PG), POY+PH+PG,  PW, PH };
        for (int b = 0; b < 4; ++b) {
            L.buttons[sp303::BTN_PAD_1 + b*8 + i]     = top;
            L.buttons[sp303::BTN_PAD_1 + b*8 + 4 + i] = bot;
        }
    }

    const int DW = 28, DH = 52, DG = 6;
    L.disp = { OX + 7*SX, 30, DW, DH, DG };

    L.peak = { L.disp.x + 3*DW + 2*DG + 26, 30 + DH/2, 10 };

    const int KY = 90, KLEN = 160, KSX = 220;
    for (int i = 0; i < sp303::KNOB_COUNT; ++i)
        L.knobs[i] = { OX + i*KSX, KY, KLEN };

    return L;
}

// ─── JSON persistence ─────────────────────────────────────────────────────────

static void save_layout(const Layout& L) {
    json j;
    for (int i = 0; i < sp303::BTN_COUNT; ++i) {
        if (i > sp303::BTN_PAD_8 && i <= sp303::BTN_PAD_32) continue;
        const auto& b = L.buttons[i];
        j["buttons"][std::to_string(i)] = { {"x",b.x},{"y",b.y},{"w",b.w},{"h",b.h} };
    }
    j["peak"]    = { {"x",L.peak.x}, {"y",L.peak.y}, {"r",L.peak.r} };
    j["display"] = { {"x",L.disp.x}, {"y",L.disp.y},
                     {"dw",L.disp.dw}, {"dh",L.disp.dh}, {"gap",L.disp.gap} };
    for (int i = 0; i < sp303::KNOB_COUNT; ++i) {
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
            for (int i = 0; i < sp303::BTN_COUNT; ++i) {
                if (i > sp303::BTN_PAD_8 && i <= sp303::BTN_PAD_32) continue;
                auto key = std::to_string(i);
                if (j["buttons"].contains(key)) {
                    auto& r = j["buttons"][key];
                    BtnPos p = { r["x"], r["y"], r["w"], r["h"] };
                    L.buttons[i] = p;
                    if (i >= sp303::BTN_PAD_1 && i <= sp303::BTN_PAD_8) {
                        int slot = i - sp303::BTN_PAD_1;
                        for (int b = 1; b < 4; ++b)
                            L.buttons[sp303::BTN_PAD_1 + b*8 + slot] = p;
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
            for (int i = 0; i < sp303::KNOB_COUNT; ++i) {
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
    DrawRectangle(x+t+p,  y,         dw-2*(t+p), t,    col(sp303::SEG_A));
    DrawRectangle(x+dw-t, y+t,       t,          hh-t, col(sp303::SEG_B));
    DrawRectangle(x+dw-t, y+hh,      t,          hh-t, col(sp303::SEG_C));
    DrawRectangle(x+t+p,  y+dh-t,    dw-2*(t+p), t,    col(sp303::SEG_D));
    DrawRectangle(x,      y+hh,      t,          hh-t, col(sp303::SEG_E));
    DrawRectangle(x,      y+t,       t,          hh-t, col(sp303::SEG_F));
    DrawRectangle(x+t+p,  y+hh-t/2,  dw-2*(t+p), t,    col(sp303::SEG_G));
    if (mask & sp303::SEG_DP)
        DrawRectangle(x+dw+2, y+dh-t, t, t, C_SEG_ON);
}

static void draw_display(const DispPos& dp, const sp303::Display& disp) {
    int tw = 3*dp.dw + 2*dp.gap;
    DrawRectangle(dp.x-6, dp.y-6, tw+12, dp.dh+12, C_DISP_BG);
    DrawRectangleLines(dp.x-6, dp.y-6, tw+12, dp.dh+12, C_BORDER);
    for (int i = 0; i < 3; ++i)
        draw_7seg(dp.x + i*(dp.dw+dp.gap), dp.y, dp.dw, dp.dh, disp.digit[i]);
}

// ─── Button drawing ───────────────────────────────────────────────────────────

static void draw_button(const BtnPos& r, const sp303::ButtonDef& def,
                        const sp303::ButtonState& s, bool dragging) {
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

static void draw_knob(const KnobPos& k, const sp303::KnobDef& def, float value, bool dragging) {
    const int TH = 10;
    const int TR = 7;
    int fill_w = (int)(value * k.len);
    int cx     = k.x + fill_w;
    int cy     = k.y + TH / 2;

    const int lfs = 9;
    DrawText(def.primary, k.x, k.y - lfs - 4, lfs, dragging ? C_DRAG : C_TEXT);
    if (def.alt1) {
        std::string alt = def.alt1;
        if (def.alt2) { alt += "/"; alt += def.alt2; }
        int atw = MeasureText(alt.c_str(), 7);
        DrawText(alt.c_str(), k.x + k.len - atw, k.y - 7 - 4, 7, C_ALT);
    }

    DrawRectangle(k.x, k.y, k.len, TH, C_KNOB_TRACK);
    DrawRectangle(k.x, k.y, fill_w, TH, dragging ? C_DRAG : C_KNOB_FILL);
    DrawRectangleLines(k.x, k.y, k.len, TH, C_BORDER);

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

static void draw_stereo_activity(const IndPos& p, bool lit) {
    DrawCircle(p.x, p.y, (float)(p.r + 1), C_BORDER);
    DrawCircle(p.x, p.y, (float)p.r,       lit ? C_PEAK_ON : C_PEAK_OFF);
    const int fs = 8;
    DrawText("ST", p.x - MeasureText("ST", fs)/2, p.y + p.r + 3, fs, C_ALT);
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

static int snap_to_grid(int v) {
    if (DRAG_GRID <= 1) return v;
    float g = (float)DRAG_GRID;
    return (int)std::lround(v / g) * DRAG_GRID;
}

// ─── Drag targets ─────────────────────────────────────────────────────────────

static const int DRAG_NONE   = -1;
static const int DRAG_PEAK   = sp303::BTN_COUNT;
static const int DRAG_DISP   = sp303::BTN_COUNT + 1;
static const int DRAG_KNOB_0 = sp303::BTN_COUNT + 2;

struct Drag {
    int target = DRAG_NONE;
    int offx   = 0;
    int offy   = 0;
};

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(void) {
    RendererController controller{};
    renderer_controller_init(&controller);

    bool config_open = false;

    sp303::Device* dev    = sp303::create();
    Layout         layout = load_layout();
    Drag           drag;
    int            pressed_btn = -1;
    int            active_knob = -1;

    auto keymap = load_keymap();
    std::unordered_map<int, sp303::ButtonID> key_held;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SW, SH, "SP-303");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();
        int  mx    = (int)mouse.x;
        int  my    = (int)mouse.y;
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        auto release_live_inputs = [&]() {
            if (pressed_btn >= 0) {
                sp303::button_up(dev, (sp303::ButtonID)pressed_btn);
                pressed_btn = -1;
            }
            for (auto& [key, btn] : key_held) {
                (void)key;
                sp303::button_up(dev, btn);
            }
            key_held.clear();
            active_knob = -1;
        };

        auto trigger_pad_audio = [&](int slot) {
            if (!controller.audio || !sp303::pad_has_sample(dev, slot)) return;
            bool loop_mode = sp303::get_pad_loop_mode(dev, slot);
            bool gate_mode = sp303::get_pad_gate_mode(dev, slot);
            bool reverse_mode = sp303::get_pad_reverse_mode(dev, slot);
            int hold_frames = sp303::audio_get_pad_led_hold_frames(controller.audio, slot, reverse_mode);
            sp303::set_pad_led_hold_frames(dev, slot, hold_frames);
            sp303::audio_trigger_mode(controller.audio, slot, loop_mode, gate_mode, reverse_mode);
        };

        if (IsKeyPressed(KEY_TAB)) config_open = !config_open;
        if (!config_open && IsKeyPressed(KEY_F5)) {
            ProjectIoResult res = project_save(QUICKSAVE_DIR, dev, controller.audio, controller.audio_cfg);
            std::printf("[PROJECT] %s\n", res.message.c_str());
        }
        if (!config_open && IsKeyPressed(KEY_F9)) {
            release_live_inputs();
            drag.target = DRAG_NONE;
            sp303::Device* loaded_dev = sp303::create();
            ProjectIoResult res = project_load(QUICKSAVE_DIR, loaded_dev, controller.audio, controller.audio_cfg);
            if (res.ok) {
                sp303::destroy(dev);
                dev = loaded_dev;
            } else {
                sp303::destroy(loaded_dev);
            }
            std::printf("[PROJECT] %s\n", res.message.c_str());
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && config_open) {
            // Config screen consumes all clicks
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !config_open) {
            if (shift) {
                for (int i = 0; i < sp303::BTN_COUNT && drag.target == DRAG_NONE; ++i) {
                    if (i > sp303::BTN_PAD_8 && i <= sp303::BTN_PAD_32) continue;
                    if (hit_btn(mx, my, layout.buttons[i]))
                        drag = { i, mx - layout.buttons[i].x, my - layout.buttons[i].y };
                }
                if (drag.target == DRAG_NONE && hit_circle(mx, my, layout.peak))
                    drag = { DRAG_PEAK, mx - layout.peak.x, my - layout.peak.y };
                if (drag.target == DRAG_NONE && hit_disp(mx, my, layout.disp))
                    drag = { DRAG_DISP, mx - layout.disp.x, my - layout.disp.y };
                for (int i = 0; i < sp303::KNOB_COUNT && drag.target == DRAG_NONE; ++i) {
                    if (hit_knob(mx, my, layout.knobs[i]))
                        drag = { DRAG_KNOB_0 + i, mx - layout.knobs[i].x, my - layout.knobs[i].y };
                }
            } else {
                for (int i = 0; i < sp303::KNOB_COUNT; ++i) {
                    if (hit_knob(mx, my, layout.knobs[i])) {
                        active_knob = i;
                        float v = std::clamp((float)(mx - layout.knobs[i].x) / layout.knobs[i].len, 0.0f, 1.0f);
                        sp303::knob_set(dev, (sp303::KnobID)i, v);
                        break;
                    }
                }
                if (active_knob < 0) {
                    sp303::State cur = sp303::get_state(dev);
                    bool resample_source = sp303::is_resample_source_select(dev);
                    bool resample_dest = sp303::is_resample_dest_select(dev);
                    bool resample_armed = sp303::is_resample_armed(dev);
                    for (int i = 0; i < 8; ++i) {
                        if (hit_btn(mx, my, layout.buttons[sp303::BTN_PAD_1 + i])) {
                            int pad_id = sp303::BTN_PAD_1 + cur.active_bank * 8 + i;
                            sp303::button_down(dev, (sp303::ButtonID)pad_id);
                            pressed_btn = pad_id;
                            int slot = pad_id - sp303::BTN_PAD_1;
                            if (resample_source) {
                                sp303::note_pad_played(dev, slot);
                                trigger_pad_audio(slot);
                            } else if (resample_armed) {
                                if (slot == sp303::get_resample_source_pad(dev)) {
                                    sp303::note_pad_played(dev, slot);
                                    trigger_pad_audio(slot);
                                }
                            } else if (!resample_source &&
                                !resample_dest &&
                                !sp303::is_sampling_standby(dev) &&
                                !sp303::is_sampling_ready(dev) &&
                                !sp303::is_recording(dev) &&
                                !sp303::is_threshold_mode(dev) &&
                                !sp303::is_delete_mode(dev) &&
                                !cur.buttons[sp303::BTN_REMAIN].pressed) {
                                sp303::note_pad_played(dev, slot);
                                trigger_pad_audio(slot);
                            }
                            break;
                        }
                    }
                    if (pressed_btn < 0) {
                        for (int i = 0; i < sp303::BTN_COUNT; ++i) {
                            if (i >= sp303::BTN_PAD_1 && i <= sp303::BTN_PAD_32) continue;
                            if (hit_btn(mx, my, layout.buttons[i])) {
                                sp303::button_down(dev, (sp303::ButtonID)i);
                                pressed_btn = i;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (drag.target != DRAG_NONE) {
                drag.target = DRAG_NONE;
                save_layout(layout);
            }
            if (pressed_btn >= 0) {
                sp303::button_up(dev, (sp303::ButtonID)pressed_btn);
                if (controller.audio &&
                    pressed_btn >= sp303::BTN_PAD_1 &&
                    pressed_btn <= sp303::BTN_PAD_32) {
                    int slot = pressed_btn - sp303::BTN_PAD_1;
                    if (sp303::get_pad_gate_mode(dev, slot)) {
                        sp303::audio_note_off(controller.audio, slot);
                    }
                }
                pressed_btn = -1;
            }
            active_knob = -1;
        }

        if (drag.target != DRAG_NONE) {
            int nx = snap_to_grid(mx - drag.offx);
            int ny = snap_to_grid(my - drag.offy);
            int t  = drag.target;
            if (t < sp303::BTN_COUNT) {
                layout.buttons[t].x = nx;
                layout.buttons[t].y = ny;
                if (t >= sp303::BTN_PAD_1 && t <= sp303::BTN_PAD_8) {
                    int slot = t - sp303::BTN_PAD_1;
                    BtnPos p = layout.buttons[t];
                    for (int b = 1; b < 4; ++b)
                        layout.buttons[sp303::BTN_PAD_1 + b*8 + slot] = p;
                }
            } else if (t == DRAG_PEAK) {
                layout.peak.x = nx; layout.peak.y = ny;
            } else if (t == DRAG_DISP) {
                layout.disp.x = nx; layout.disp.y = ny;
            } else if (t >= DRAG_KNOB_0 && t < DRAG_KNOB_0 + sp303::KNOB_COUNT) {
                layout.knobs[t - DRAG_KNOB_0].x = nx;
                layout.knobs[t - DRAG_KNOB_0].y = ny;
            }
        }

        if (active_knob >= 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float v = std::clamp((float)(mx - layout.knobs[active_knob].x) / layout.knobs[active_knob].len, 0.0f, 1.0f);
            sp303::knob_set(dev, (sp303::KnobID)active_knob, v);
        }

        if (!shift && !config_open) {
            sp303::State cur = sp303::get_state(dev);
            bool sampling_active =
                sp303::is_sampling_standby(dev) ||
                sp303::is_sampling_ready(dev) ||
                sp303::is_recording(dev) ||
                sp303::is_threshold_mode(dev) ||
                sp303::is_delete_mode(dev) ||
                sp303::is_resampling_mode(dev);
            bool resample_source = sp303::is_resample_source_select(dev);
            bool resample_dest = sp303::is_resample_dest_select(dev);
            bool resample_armed = sp303::is_resample_armed(dev);
            for (auto& [key, btn] : keymap) {
                if (IsKeyPressed(key)) {
                    sp303::ButtonID actual = btn;
                    if (btn >= sp303::BTN_PAD_1 && btn <= sp303::BTN_PAD_8)
                        actual = (sp303::ButtonID)(sp303::BTN_PAD_1 + cur.active_bank * 8 + (btn - sp303::BTN_PAD_1));
                    sp303::button_down(dev, actual);
                    key_held[key] = actual;
                    if (actual >= sp303::BTN_PAD_1 && actual <= sp303::BTN_PAD_32) {
                        int slot = actual - sp303::BTN_PAD_1;
                        if (resample_source) {
                            sp303::note_pad_played(dev, slot);
                            trigger_pad_audio(slot);
                        } else if (resample_armed) {
                            if (slot == sp303::get_resample_source_pad(dev)) {
                                sp303::note_pad_played(dev, slot);
                                trigger_pad_audio(slot);
                            }
                        } else if (!sampling_active &&
                                   !cur.buttons[sp303::BTN_REMAIN].pressed &&
                                   !resample_source &&
                                   !resample_dest) {
                            sp303::note_pad_played(dev, slot);
                            trigger_pad_audio(slot);
                        }
                    }
                }
                if (IsKeyReleased(key)) {
                    auto it = key_held.find(key);
                    if (it != key_held.end()) {
                        sp303::ButtonID released = it->second;
                        if (controller.audio &&
                            released >= sp303::BTN_PAD_1 &&
                            released <= sp303::BTN_PAD_32) {
                            int slot = released - sp303::BTN_PAD_1;
                            if (sp303::get_pad_gate_mode(dev, slot)) {
                                sp303::audio_note_off(controller.audio, slot);
                            }
                        }
                        sp303::button_up(dev, it->second);
                        key_held.erase(it);
                    }
                }
            }
        }

        sp303::State state = renderer_controller_step(&controller, dev, active_knob);

        BeginDrawing();
        ClearBackground(C_BG);

        for (int i = 0; i < sp303::BTN_COUNT; ++i) {
            if (i >= sp303::BTN_PAD_1 && i <= sp303::BTN_PAD_32) continue;
            draw_button(layout.buttons[i], sp303::BUTTON_DEFS[i],
                        state.buttons[i], drag.target == i);
        }

        int bank_off = state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            int pad_id    = sp303::BTN_PAD_1 + bank_off + i;
            int layout_id = sp303::BTN_PAD_1 + i;
            draw_button(layout.buttons[layout_id], sp303::BUTTON_DEFS[pad_id],
                        state.buttons[pad_id], drag.target == layout_id);
        }

        for (int i = 0; i < sp303::KNOB_COUNT; ++i)
            draw_knob(layout.knobs[i], sp303::KNOB_DEFS[i],
                      state.knobs[i].value, active_knob == i || drag.target == DRAG_KNOB_0 + i);

        draw_display(layout.disp, state.display);
        draw_peak(layout.peak, state.indicators[sp303::IND_PEAK].lit);
        IndPos stereo_pos = { layout.peak.x + 36, layout.peak.y, layout.peak.r };
        draw_stereo_activity(stereo_pos, controller.stereo_activity_lit);

        if (!config_open && shift)
            DrawText("SHIFT + drag: reposition  |  release: save", 10, SH - 18, 9, C_ALT);
        if (!config_open)
            DrawText("[F5] quick-save  [F9] quick-load  [TAB] audio config", SW - 330, SH - 18, 9, C_ALT);

        if (config_open) {
            bool apply = draw_config_screen(
                controller.sel_out, controller.sel_in, controller.sel_rate, controller.sel_buf, controller.peak_threshold,
                controller.out_devs, controller.in_devs, controller.playback_ok,
                mx, my, IsMouseButtonPressed(MOUSE_BUTTON_LEFT), IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                controller.config_input_peak);

            if (apply) {
                renderer_controller_apply_audio_config(&controller);
            }
        }

        EndDrawing();
    }

    save_layout(layout);
    sp303::destroy(dev);
    renderer_controller_shutdown(&controller);
    CloseWindow();
    return 0;
}
