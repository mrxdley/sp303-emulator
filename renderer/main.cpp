#include "raylib.h"
#include "sp303.h"
#include "sp303_audio.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <cstring>

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

// ─── Audio config persistence ─────────────────────────────────────────────────

static const char* AUDIO_CFG_FILE = "audio_config.json";

static SP303AudioConfig load_audio_config() {
    SP303AudioConfig cfg;
    sp303_audio_config_default(&cfg);
    std::ifstream f(AUDIO_CFG_FILE);
    if (!f.is_open()) return cfg;
    try {
        auto j = json::parse(f);
        if (j.contains("output")) std::strncpy(cfg.output_name, j["output"].get<std::string>().c_str(), SP303_AUDIO_NAME_LEN - 1);
        if (j.contains("input"))  std::strncpy(cfg.input_name,  j["input"] .get<std::string>().c_str(), SP303_AUDIO_NAME_LEN - 1);
        if (j.contains("rate"))   cfg.sample_rate   = j["rate"];
        if (j.contains("buffer")) cfg.buffer_frames = j["buffer"];
    } catch (...) {}
    return cfg;
}

static void save_audio_config(const SP303AudioConfig& cfg) {
    json j;
    j["output"] = cfg.output_name;
    j["input"]  = cfg.input_name;
    j["rate"]   = cfg.sample_rate;
    j["buffer"] = cfg.buffer_frames;
    std::ofstream f(AUDIO_CFG_FILE);
    f << j.dump(2);
}

// ─── Config screen ────────────────────────────────────────────────────────────

static const uint32_t SAMPLE_RATES[] = {44100, 48000, 96000};
static const uint32_t BUFFER_SIZES[] = {128, 256, 512, 1024, 2048};
static const int      N_RATES        = 3;
static const int      N_BUFS         = 5;

// Returns true when APPLY is clicked.
// sel_* are indices into SAMPLE_RATES / BUFFER_SIZES / device lists.
static bool draw_config_screen(
    int& sel_out, int& sel_in, int& sel_rate, int& sel_buf,
    const std::vector<SP303AudioDeviceInfo>& out_devs,
    const std::vector<SP303AudioDeviceInfo>& in_devs,
    bool playback_ok, int mx, int my, bool clicked)
{
    // Darken main view
    DrawRectangle(0, 0, SW, SH, {0, 0, 0, 170});

    const int PX = 160, PY = 140, PW = 960, PH = 440;
    DrawRectangle(PX, PY, PW, PH, {22, 22, 22, 255});
    DrawRectangleLines(PX, PY, PW, PH, C_BORDER);

    DrawText("AUDIO CONFIGURATION", PX + 24, PY + 18, 14, C_TEXT);
    DrawText("[TAB] close", PX + PW - 90, PY + 18, 9, C_ALT);

    // ── Selector row helper ────────────────────────────────────────────────
    // Returns -1 (prev clicked), 0 (nothing), +1 (next clicked)
    auto selector = [&](int row, const char* label, const std::string& value) -> int {
        const int RY  = PY + 72 + row * 72;
        const int LX  = PX + 24;
        const int VX  = LX + 150;
        const int BW2 = 26, BH2 = 26;

        DrawText(label, LX, RY + 5, 10, C_ALT);

        // < button
        DrawRectangle(VX, RY, BW2, BH2, C_UNLIT);
        DrawText("<", VX + 9, RY + 6, 10, C_TEXT);

        // value text
        const int max_val_w = PW - 330;
        std::string disp = value;
        while (MeasureText(disp.c_str(), 10) > max_val_w && disp.size() > 1)
            disp = disp.substr(0, disp.size() - 1);
        DrawText(disp.c_str(), VX + BW2 + 8, RY + 6, 10, C_TEXT);

        // > button
        const int NX = VX + BW2 + 8 + MeasureText(disp.c_str(), 10) + 8;
        DrawRectangle(NX, RY, BW2, BH2, C_UNLIT);
        DrawText(">", NX + 8, RY + 6, 10, C_TEXT);

        if (!clicked) return 0;
        if (mx >= VX && mx < VX + BW2 && my >= RY && my < RY + BH2) return -1;
        if (mx >= NX && mx < NX + BW2 && my >= RY && my < RY + BH2) return +1;
        return 0;
    };

    // Output device
    std::string out_name = out_devs.empty() ? "(no devices)" :
        (out_devs[sel_out].is_default
            ? std::string(out_devs[sel_out].name) + "  [default]"
            : out_devs[sel_out].name);
    int d = selector(0, "Output device:", out_name);
    if (d && !out_devs.empty())
        sel_out = (sel_out + d + (int)out_devs.size()) % (int)out_devs.size();

    // Input device
    std::string in_name = in_devs.empty() ? "(no devices)" :
        (in_devs[sel_in].is_default
            ? std::string(in_devs[sel_in].name) + "  [default]"
            : in_devs[sel_in].name);
    d = selector(1, "Input device:", in_name);
    if (d && !in_devs.empty())
        sel_in = (sel_in + d + (int)in_devs.size()) % (int)in_devs.size();

    // Sample rate
    d = selector(2, "Sample rate:", std::to_string(SAMPLE_RATES[sel_rate]) + " Hz");
    if (d) sel_rate = (sel_rate + d + N_RATES) % N_RATES;

    // Buffer size
    d = selector(3, "Buffer size:", std::to_string(BUFFER_SIZES[sel_buf]) + " frames");
    if (d) sel_buf = (sel_buf + d + N_BUFS) % N_BUFS;

    // Status row
    const char* status = playback_ok ? "output: OK" : "output: FAILED — check device";
    DrawText(status, PX + 24, PY + PH - 52, 9, playback_ok ? C_ALT : C_LIT);

    // APPLY button
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

    // Pads: 2 rows of 4, square — same 8 positions shared across all 4 banks
    const int PW = 96, PH = 90, PG = 10;
    const int POY = OY + 4*SY + 10;
    for (int i = 0; i < 4; ++i) {
        BtnPos top = { OX + i*(PW+PG), POY,        PW, PH };
        BtnPos bot = { OX + i*(PW+PG), POY+PH+PG,  PW, PH };
        for (int b = 0; b < 4; ++b) {
            L.buttons[SP303_BTN_PAD_1 + b*8 + i]     = top;
            L.buttons[SP303_BTN_PAD_1 + b*8 + 4 + i] = bot;
        }
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
                    // Mirror pad positions to all banks
                    if (i >= SP303_BTN_PAD_1 && i <= SP303_BTN_PAD_8) {
                        int slot = i - SP303_BTN_PAD_1;
                        for (int b = 1; b < 4; ++b)
                            L.buttons[SP303_BTN_PAD_1 + b*8 + slot] = p;
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
    // ── Audio ─────────────────────────────────────────────────────────────────
    SP303AudioConfig audio_cfg = load_audio_config();
    SP303Audio*      audio     = sp303_audio_create(&audio_cfg);

    // Pre-load a 1s 440 Hz sine wave into slot 0 (PAD 1, Bank A) for testing
    if (audio) {
        const uint32_t sr     = audio_cfg.sample_rate;
        const float    TWO_PI = 6.28318530718f;
        std::vector<float> sine(sr);
        for (uint32_t i = 0; i < sr; ++i)
            sine[i] = 0.5f * std::sin(TWO_PI * 440.0f * i / (float)sr);
        sp303_audio_load_sample(audio, 0, sine.data(), sr);
    }

    // Cache device lists for the config UI
    std::vector<SP303AudioDeviceInfo> out_devs(64), in_devs(64);
    int n_out = audio ? sp303_audio_list_outputs(audio, out_devs.data(), 64) : 0;
    int n_in  = audio ? sp303_audio_list_inputs (audio, in_devs.data(),  64) : 0;
    out_devs.resize(n_out);
    in_devs .resize(n_in);

    // Find initial selected indices matching the loaded config
    int sel_out = 0, sel_in = 0, sel_rate = 0, sel_buf = 2; // default: 512 frames
    for (int i = 0; i < n_out; ++i)
        if (std::strncmp(out_devs[i].name, audio_cfg.output_name, SP303_AUDIO_NAME_LEN) == 0)
            { sel_out = i; break; }
    for (int i = 0; i < n_in; ++i)
        if (std::strncmp(in_devs[i].name, audio_cfg.input_name, SP303_AUDIO_NAME_LEN) == 0)
            { sel_in = i; break; }
    for (int i = 0; i < N_RATES; ++i)
        if (SAMPLE_RATES[i] == audio_cfg.sample_rate)  { sel_rate = i; break; }
    for (int i = 0; i < N_BUFS; ++i)
        if (BUFFER_SIZES[i] == audio_cfg.buffer_frames) { sel_buf  = i; break; }

    bool config_open   = false;
    bool playback_ok   = (audio != nullptr);

    // ── Core + renderer state ─────────────────────────────────────────────────
    SP303Device* dev    = sp303_create();
    Layout       layout = load_layout();
    Drag         drag;
    int          pressed_btn  = -1;
    int          active_knob  = -1;

    auto keymap = load_keymap();
    std::unordered_map<int, SP303ButtonID> key_held;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SW, SH, "SP-303");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();
        int  mx    = (int)mouse.x;
        int  my    = (int)mouse.y;
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        if (IsKeyPressed(KEY_TAB)) config_open = !config_open;

        // ── Press ─────────────────────────────────────────────────────────────
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && config_open) {
            // Config screen consumes all clicks
            // (draw_config_screen handles the interaction — evaluated in draw phase)
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !config_open) {
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
                            if (audio) sp303_audio_trigger(audio, pad_id - SP303_BTN_PAD_1);
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
                    for (int b = 1; b < 4; ++b)
                        layout.buttons[SP303_BTN_PAD_1 + b*8 + slot] = p;
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
        if (!shift && !config_open) {
            SP303State cur = sp303_get_state(dev);
            for (auto& [key, btn] : keymap) {
                if (IsKeyPressed(key)) {
                    SP303ButtonID actual = btn;
                    if (btn >= SP303_BTN_PAD_1 && btn <= SP303_BTN_PAD_8)
                        actual = (SP303ButtonID)(SP303_BTN_PAD_1 + cur.active_bank * 8 + (btn - SP303_BTN_PAD_1));
                    sp303_button_down(dev, actual);
                    key_held[key] = actual;
                    if (audio && actual >= SP303_BTN_PAD_1 && actual <= SP303_BTN_PAD_32)
                        sp303_audio_trigger(audio, actual - SP303_BTN_PAD_1);
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

        // Drive PEAK indicator from the audio engine
        if (audio) {
            float peak = sp303_audio_peak(audio);
            sp303_indicator_set(dev, SP303_IND_PEAK, peak > 0.85f);
        }

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

        if (!config_open && shift)
            DrawText("SHIFT + drag: reposition  |  release: save", 10, SH - 18, 9, C_ALT);
        if (!config_open)
            DrawText("[TAB] audio config", SW - 130, SH - 18, 9, C_ALT);

        // Config overlay — draw + handle interaction in the draw phase
        if (config_open) {
            bool apply = draw_config_screen(
                sel_out, sel_in, sel_rate, sel_buf,
                out_devs, in_devs, playback_ok,
                mx, my, IsMouseButtonPressed(MOUSE_BUTTON_LEFT));

            if (apply && audio) {
                SP303AudioConfig new_cfg = audio_cfg;
                if (!out_devs.empty())
                    std::strncpy(new_cfg.output_name, out_devs[sel_out].name, SP303_AUDIO_NAME_LEN - 1);
                if (!in_devs.empty())
                    std::strncpy(new_cfg.input_name, in_devs[sel_in].name, SP303_AUDIO_NAME_LEN - 1);
                new_cfg.sample_rate   = SAMPLE_RATES[sel_rate];
                new_cfg.buffer_frames = BUFFER_SIZES[sel_buf];

                playback_ok = sp303_audio_reconfigure(audio, &new_cfg);
                if (playback_ok) {
                    audio_cfg = new_cfg;
                    save_audio_config(audio_cfg);
                }
            }
        }

        EndDrawing();
    }

    save_layout(layout);
    sp303_destroy(dev);
    sp303_audio_destroy(audio);
    CloseWindow();
    return 0;
}
