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
#include <queue>
#include <filesystem>

using json = nlohmann::json;

// ─── Screen ───────────────────────────────────────────────────────────────────

static const int SW = 1280;
static const int SH =  780;
static const int DRAG_GRID = 5;

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
static const char* WINDOW_FILE  = "window.json";
static const char* BACKGROUND_CANDIDATES[] = {
    "background.png",
    "../background.png",
    "../../background.png",
};
static const char* KNOB_CANDIDATES[] = {
    "knob.png",
    "../knob.png",
    "../../knob.png",
};
static const bool ENABLE_KNOB_SPRITE = false;

// ─── Layout structs ───────────────────────────────────────────────────────────

struct BtnPos  { int x, y, w, h; };
struct IndPos  { int x, y, r;    };
struct DispPos { int x, y, dw, dh, gap; };
struct KnobPos { int x, y, r;    };

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
        {"m",         "MFX"},
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

// Bindings that are always guaranteed regardless of keymap.json content.
static void apply_fixed_bindings(std::unordered_map<int, sp303::ButtonID>& out) {
    out.emplace(KEY_M, sp303::BTN_MFX);
}

static std::unordered_map<int, sp303::ButtonID> load_keymap() {
    std::ifstream f(KEYMAP_FILE);
    if (!f.is_open()) {
        auto m = write_default_keymap();
        apply_fixed_bindings(m);
        return m;
    }
    try {
        json j = json::parse(f);
        std::unordered_map<int, sp303::ButtonID> out;
        for (auto& [k, v] : j.items()) {
            int             key = key_from_name(k);
            sp303::ButtonID btn = btn_from_name(v.get<std::string>());
            if (key && btn != (sp303::ButtonID)-1) out[key] = btn;
        }
        apply_fixed_bindings(out);
        return out;
    } catch (...) {
        auto m = write_default_keymap();
        apply_fixed_bindings(m);
        return m;
    }
}

// ─── Config screen ────────────────────────────────────────────────────────────

struct WindowPrefs {
    int w = SW;
    int h = SH;
    bool show_hitboxes = true;
};

struct BackgroundAsset {
    Texture2D texture{};
    Image image{};
    std::string path;
    std::string cache_key;
    bool loaded = false;
};

struct KnobSpriteAsset {
    Texture2D texture{};
    bool loaded = false;
};

struct OverlayCacheEntry {
    BtnPos rect{0, 0, 0, 0};
    Texture2D pressed{};
    Texture2D lit{};
    Texture2D litpressed{};
    bool valid = false;
};

enum OverlayState {
    OVERLAY_PRESSED = 0,
    OVERLAY_LIT = 1,
    OVERLAY_LITPRESSED = 2,
};

static BackgroundAsset load_background_asset() {
    for (const char* path : BACKGROUND_CANDIDATES) {
        if (!FileExists(path)) continue;
        Image img = LoadImage(path);
        if (!img.data) continue;
        Texture2D tex = LoadTextureFromImage(img);
        if (tex.id != 0) {
            std::string cache_key = "default";
            try {
                auto stamp = std::filesystem::last_write_time(path).time_since_epoch().count();
                cache_key = std::to_string(img.width) + "x" + std::to_string(img.height) + "_" + std::to_string(stamp);
            } catch (...) {}
            return { tex, img, path, cache_key, true };
        }
        UnloadImage(img);
    }
    return {};
}

static KnobSpriteAsset load_knob_sprite_asset() {
    if (!ENABLE_KNOB_SPRITE) return {};
    for (const char* path : KNOB_CANDIDATES) {
        if (!FileExists(path)) continue;
        Image img = LoadImage(path);
        if (!img.data) continue;
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        Texture2D tex = LoadTextureFromImage(img);
        UnloadImage(img);
        if (tex.id != 0) return { tex, true };
    }
    return {};
}

static int background_width(const BackgroundAsset& bg) {
    return bg.loaded ? bg.texture.width : SW;
}

static int background_height(const BackgroundAsset& bg) {
    return bg.loaded ? bg.texture.height : SH;
}

static bool same_btn_rect(const BtnPos& a, const BtnPos& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static bool is_exact_blue_seed(Color c) {
    return c.r == 0x87 && c.g == 0xb4 && c.b == 0xcd;
}

static bool is_exact_gray_seed(Color c) {
    return c.r == 0xbc && c.g == 0xc5 && c.b == 0xce;
}

static bool is_seed_color(Color c) {
    return is_exact_blue_seed(c) || is_exact_gray_seed(c);
}

static bool is_blue_family(Color c) {
    if (c.a == 0) return false;
    if (c.r == 0 && c.g == 0 && c.b == 0) return false;
    if (c.b < 18) return false;
    if (c.g < c.r - 20) return false;
    return c.b >= c.g - 24;
}

static bool is_gray_family(Color c) {
    if (c.a == 0) return false;
    if (c.r == 0 && c.g == 0 && c.b == 0) return false;
    int hi = std::max({(int)c.r, (int)c.g, (int)c.b});
    int lo = std::min({(int)c.r, (int)c.g, (int)c.b});
    return (hi - lo) <= 44;
}

static bool is_fill_family(Color c) {
    return is_blue_family(c) || is_gray_family(c);
}

static std::string hitbox_cache_base(const BackgroundAsset& bg, const BtnPos& rect) {
    std::string dir = "hitbox_cache/" + bg.cache_key;
    return dir + "/" + std::to_string(rect.x) + "_" + std::to_string(rect.y) + "_" +
           std::to_string(rect.w) + "_" + std::to_string(rect.h);
}

static void flood_fill_mask_from_hitbox(Image& mask, const BackgroundAsset& bg, const BtnPos& rect) {
    const int w = std::max(bg.image.width, 1);
    const int h = std::max(bg.image.height, 1);
    std::vector<unsigned char> visited((size_t)w * (size_t)h, 0);
    std::queue<std::pair<int, int>> q;

    const int rx0 = std::max(rect.x, 0);
    const int ry0 = std::max(rect.y, 0);
    const int rx1 = std::min(rect.x + rect.w, bg.image.width);
    const int ry1 = std::min(rect.y + rect.h, bg.image.height);

    for (int y = ry0; y < ry1; ++y) {
        for (int x = rx0; x < rx1; ++x) {
            Color c = GetImageColor(bg.image, x, y);
            if (!is_seed_color(c)) continue;
            size_t idx = (size_t)y * (size_t)w + (size_t)x;
            if (visited[idx]) continue;
            visited[idx] = 1;
            q.push({x, y});

            while (!q.empty()) {
                auto [cx, cy] = q.front();
                q.pop();
                Color cc = GetImageColor(bg.image, cx, cy);
                if (!is_fill_family(cc)) continue;

                ImageDrawPixel(&mask, cx, cy, WHITE);

                static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
                static const int DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
                for (int i = 0; i < 8; ++i) {
                    int nx = cx + DX[i];
                    int ny = cy + DY[i];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                    size_t nidx = (size_t)ny * (size_t)w + (size_t)nx;
                    if (visited[nidx]) continue;
                    Color nc = GetImageColor(bg.image, nx, ny);
                    if (!is_fill_family(nc)) continue;
                    visited[nidx] = 1;
                    q.push({nx, ny});
                }
            }
        }
    }
}

static std::string overlay_state_suffix(OverlayState state) {
    switch (state) {
        case OVERLAY_PRESSED: return "_pressed.png";
        case OVERLAY_LIT: return "_lit.png";
        case OVERLAY_LITPRESSED: return "_litpressed.png";
    }
    return "_pressed.png";
}

static Color tint_lit(Color c) {
    if (c.r == 0 && c.g == 0 && c.b == 0) return c;
    Color out{};
    out.r = (unsigned char)std::clamp((int)std::lround(c.r * 0.45f + 175.0f * 0.40f), 0, 255);
    out.g = (unsigned char)std::clamp((int)std::lround(c.g * 0.45f + 18.0f * 0.10f), 0, 255);
    out.b = (unsigned char)std::clamp((int)std::lround(c.b * 0.45f + 18.0f * 0.10f), 0, 255);
    out.a = 255;
    return out;
}

static Color tint_litpressed(Color c) {
    if (c.r == 0 && c.g == 0 && c.b == 0) return c;
    Color lit = tint_lit(c);
    Color out{};
    out.r = (unsigned char)std::clamp((int)std::lround(lit.r * 0.62f), 0, 255);
    out.g = (unsigned char)std::clamp((int)std::lround(lit.g * 0.62f), 0, 255);
    out.b = (unsigned char)std::clamp((int)std::lround(lit.b * 0.62f), 0, 255);
    out.a = 255;
    return out;
}

static Image build_overlay_image_from_mask(const Image& mask, const BackgroundAsset& bg, OverlayState state) {
    Image overlay = GenImageColor(bg.image.width, bg.image.height, BLANK);
    for (int y = 0; y < bg.image.height; ++y) {
        for (int x = 0; x < bg.image.width; ++x) {
            Color m = GetImageColor(mask, x, y);
            if (m.a == 0) continue;
            Color src = GetImageColor(bg.image, x, y);
            Color out = src;
            if (state == OVERLAY_PRESSED) {
                out = {
                    (unsigned char)(src.r / 2),
                    (unsigned char)(src.g / 2),
                    (unsigned char)(src.b / 2),
                    255
                };
            } else if (state == OVERLAY_LIT) {
                out = tint_lit(src);
            } else if (state == OVERLAY_LITPRESSED) {
                out = tint_litpressed(src);
            }
            ImageDrawPixel(&overlay, x, y, out);
        }
    }
    return overlay;
}

static Texture2D load_or_build_hitbox_overlay_texture(const BackgroundAsset& bg, const BtnPos& rect, OverlayState state, const Image& mask) {
    std::string cache_file = hitbox_cache_base(bg, rect) + overlay_state_suffix(state);
    if (FileExists(cache_file.c_str())) {
        Image cached = LoadImage(cache_file.c_str());
        if (cached.data) {
            Texture2D tex = LoadTextureFromImage(cached);
            UnloadImage(cached);
            if (tex.id != 0) return tex;
        }
    }

    try {
        std::filesystem::create_directories(std::filesystem::path(cache_file).parent_path());
    } catch (...) {
    }
    Image overlay = build_overlay_image_from_mask(mask, bg, state);
    ExportImage(overlay, cache_file.c_str());
    Texture2D tex = LoadTextureFromImage(overlay);
    UnloadImage(overlay);
    return tex;
}

static void build_hitbox_overlay_entry(OverlayCacheEntry& entry, const BackgroundAsset& bg, const BtnPos& rect) {
    if (entry.valid && same_btn_rect(entry.rect, rect)) return;
    if (entry.valid) {
        if (entry.pressed.id != 0) UnloadTexture(entry.pressed);
        if (entry.lit.id != 0) UnloadTexture(entry.lit);
        if (entry.litpressed.id != 0) UnloadTexture(entry.litpressed);
    }
    entry.rect = rect;
    Image mask = GenImageColor(bg.image.width, bg.image.height, BLANK);
    flood_fill_mask_from_hitbox(mask, bg, rect);
    entry.pressed = load_or_build_hitbox_overlay_texture(bg, rect, OVERLAY_PRESSED, mask);
    entry.lit = load_or_build_hitbox_overlay_texture(bg, rect, OVERLAY_LIT, mask);
    entry.litpressed = load_or_build_hitbox_overlay_texture(bg, rect, OVERLAY_LITPRESSED, mask);
    UnloadImage(mask);
    entry.valid = (entry.pressed.id != 0 || entry.lit.id != 0 || entry.litpressed.id != 0);
}

static Texture2D get_hitbox_overlay_texture(OverlayCacheEntry& entry, const BackgroundAsset& bg, const BtnPos& rect, OverlayState state) {
    build_hitbox_overlay_entry(entry, bg, rect);
    if (state == OVERLAY_LIT) return entry.lit;
    if (state == OVERLAY_LITPRESSED) return entry.litpressed;
    return entry.pressed;
}

static void unload_hitbox_overlay_cache(std::vector<OverlayCacheEntry>& cache) {
    for (auto& entry : cache) {
        if (entry.valid && entry.pressed.id != 0) UnloadTexture(entry.pressed);
        if (entry.valid && entry.lit.id != 0) UnloadTexture(entry.lit);
        if (entry.valid && entry.litpressed.id != 0) UnloadTexture(entry.litpressed);
        entry.valid = false;
        entry.pressed = {};
        entry.lit = {};
        entry.litpressed = {};
    }
}

static void prebuild_hitbox_overlay_cache(std::vector<OverlayCacheEntry>& cache, const BackgroundAsset& bg, const Layout& layout) {
    if (!bg.loaded) return;
    for (int i = 0; i < sp303::BTN_COUNT; ++i) {
        if (i >= sp303::BTN_PAD_1 && i <= sp303::BTN_PAD_32) continue;
        if (i < 0 || i >= (int)cache.size()) continue;
        build_hitbox_overlay_entry(cache[i], bg, layout.buttons[i]);
    }
    for (int i = 0; i < 8; ++i) {
        int cache_id = sp303::BTN_PAD_1 + i;
        if (cache_id < 0 || cache_id >= (int)cache.size()) continue;
        build_hitbox_overlay_entry(cache[cache_id], bg, layout.buttons[cache_id]);
    }
}

static void save_window_prefs(int w, int h, bool show_hitboxes) {
    json j = {
        {"w", std::max(w, 1)},
        {"h", std::max(h, 1)},
        {"show_hitboxes", show_hitboxes}
    };
    std::ofstream f(WINDOW_FILE);
    f << j.dump(2);
}

static WindowPrefs load_window_prefs() {
    std::ifstream f(WINDOW_FILE);
    if (!f.is_open()) return {};
    try {
        json j = json::parse(f);
        WindowPrefs p;
        p.w = std::max((int)j.value("w", SW), 1);
        p.h = std::max((int)j.value("h", SH), 1);
        p.show_hitboxes = (bool)j.value("show_hitboxes", true);
        return p;
    } catch (...) {
        return {};
    }
}

static const uint32_t SAMPLE_RATES[] = {44100, 48000, 96000};
static const uint32_t BUFFER_SIZES[] = {128, 256, 512, 1024, 2048};
static const int      N_RATES        = 3;
static const int      N_BUFS         = 5;

static bool draw_config_screen(
    int screen_w, int screen_h,
    int& sel_out, int& sel_in, int& sel_rate, int& sel_buf, int& sel_card, float& peak_threshold,
    const std::vector<sp303::AudioDeviceInfo>& out_devs,
    const std::vector<sp303::AudioDeviceInfo>& in_devs,
    const std::vector<std::string>& card_dirs,
    bool playback_ok, int mx, int my, bool clicked, bool mouse_down,
    float input_peak, bool& show_hitboxes)
{
    DrawRectangle(0, 0, screen_w, screen_h, {0, 0, 0, 170});

    const int PW = std::min(960, screen_w - 80);
    const int PH = 520;
    const int PX = (screen_w - PW) / 2;
    const int PY = std::max(40, (screen_h - PH) / 2);
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

    std::string card_name = card_dirs.empty() ? "(no cards)" : card_dirs[sel_card];
    d = selector(4, "Card folder:", card_name);
    if (d && !card_dirs.empty())
        sel_card = (sel_card + d + (int)card_dirs.size()) % (int)card_dirs.size();

    const int TOGGLE_Y = PY + 72 + 5 * 72;
    const int CBX = PX + 174;
    const int CBY = TOGGLE_Y + 3;
    DrawText("Show hitboxes:", PX + 24, TOGGLE_Y - 1, 10, C_ALT);
    DrawRectangle(CBX, CBY, 22, 22, C_UNLIT);
    DrawRectangleLines(CBX, CBY, 22, 22, C_BORDER);
    if (show_hitboxes) {
        DrawLine(CBX + 4, CBY + 11, CBX + 9, CBY + 16, C_TEXT);
        DrawLine(CBX + 9, CBY + 16, CBX + 18, CBY + 5, C_TEXT);
    }
    if (clicked && mx >= CBX && mx < CBX + 22 && my >= CBY && my < CBY + 22) {
        show_hitboxes = !show_hitboxes;
    }

    const int SRY = PY + 72 + 6 * 72;
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

    const int METER_Y = PY + 72 + 7 * 72 - 8;
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

    const int PW = 100, PH = 100, PG = 10;
    const int POY = OY + 4*SY + 10;
    for (int i = 0; i < 4; ++i) {
        BtnPos top = { OX + i*(PW+PG), POY,        PW, PH };
        BtnPos bot = { OX + i*(PW+PG), POY+PH+PG,  PW, PH };
        for (int b = 0; b < 4; ++b) {
            L.buttons[sp303::BTN_PAD_1 + b*8 + i]     = top;
            L.buttons[sp303::BTN_PAD_1 + b*8 + 4 + i] = bot;
        }
    }

    const int DW = 56, DH = 104, DG = 12;
    L.disp = { OX + 7*SX, 30, DW, DH, DG };

    L.peak = { L.disp.x + 3*DW + 2*DG + 26, 30 + DH/2, 7 };

    const int KY = 100, KR = 26, KSX = 220;
    for (int i = 0; i < sp303::KNOB_COUNT; ++i)
        L.knobs[i] = { OX + i*KSX, KY, KR };

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
        j["knobs"][std::to_string(i)] = { {"x",k.x},{"y",k.y},{"r",k.r} };
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
                    if (i >= sp303::BTN_PAD_1 && i <= sp303::BTN_PAD_8) {
                        p.w = (int)std::lround((float)p.w / (float)DRAG_GRID) * DRAG_GRID;
                        p.h = (int)std::lround((float)p.h / (float)DRAG_GRID) * DRAG_GRID;
                    }
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
            L.peak = { j["peak"]["x"], j["peak"]["y"], std::clamp((int)j["peak"]["r"], 4, 7) };
        if (j.contains("display")) {
            auto& d = j["display"];
            L.disp = { d["x"], d["y"], d["dw"], d["dh"], d["gap"] };
            if (L.disp.dw <= 28 && L.disp.dh <= 52) {
                L.disp.dw *= 2;
                L.disp.dh *= 2;
                L.disp.gap *= 2;
            }
        }
        if (j.contains("knobs")) {
            for (int i = 0; i < sp303::KNOB_COUNT; ++i) {
                auto key = std::to_string(i);
                if (j["knobs"].contains(key)) {
                    auto& k = j["knobs"][key];
                    int r = k.contains("r") ? (int)k["r"] :
                            (k.contains("len") ? std::clamp((int)k["len"] / 3, 40, 96) : L.knobs[i].r);
                    L.knobs[i] = { k["x"], k["y"], r };
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
    const int t = std::max(2, std::min(dw / 5, dh / 10));
    const int side_x = std::max(1, t / 2);
    const int hseg_x = x + side_x;
    const int hseg_w = std::max(dw - 2 * side_x, 1);
    const int top_y = y;
    const int mid_y = y + (dh - t) / 2;
    const int bot_y = y + dh - t;
    const int upper_y = y + t;
    const int lower_y = mid_y + t;
    const int side_h = std::max(mid_y - upper_y, 1);
    auto col = [&](uint8_t bit) { return (mask & bit) ? C_SEG_ON : C_SEG_OFF; };

    DrawRectangle(hseg_x, top_y, hseg_w, t, col(sp303::SEG_A));
    DrawRectangle(x + dw - t, upper_y, t, side_h, col(sp303::SEG_B));
    DrawRectangle(x + dw - t, lower_y, t, side_h, col(sp303::SEG_C));
    DrawRectangle(hseg_x, bot_y, hseg_w, t, col(sp303::SEG_D));
    DrawRectangle(x, lower_y, t, side_h, col(sp303::SEG_E));
    DrawRectangle(x, upper_y, t, side_h, col(sp303::SEG_F));
    DrawRectangle(hseg_x, mid_y, hseg_w, t, col(sp303::SEG_G));
    if (mask & sp303::SEG_DP)
        DrawRectangle(x + dw + std::max(1, side_x / 2), y + dh - t, t, t, C_SEG_ON);
}

static void draw_display(const DispPos& dp, const sp303::Display& disp) {
    int tw = 3*dp.dw + 2*dp.gap;
    DrawRectangle(dp.x-6, dp.y-6, tw+12, dp.dh+12, C_DISP_BG);
    DrawRectangleLines(dp.x-6, dp.y-6, tw+12, dp.dh+12, C_BORDER);
    for (int i = 0; i < 3; ++i)
        draw_7seg(dp.x + i*(dp.dw+dp.gap), dp.y, dp.dw, dp.dh, disp.digit[i]);
}

// ─── Button drawing ───────────────────────────────────────────────────────────

static const char* display_button_label(sp303::ButtonID id, const sp303::ButtonDef& def) {
    switch (id) {
        case sp303::BTN_BANK_A: return "A";
        case sp303::BTN_BANK_B: return "B";
        case sp303::BTN_BANK_C: return "C";
        case sp303::BTN_BANK_D: return "D";
        default: return def.primary;
    }
}

static void draw_button(sp303::ButtonID id, const BtnPos& r, const sp303::ButtonDef& def,
                        const sp303::ButtonState& s, bool dragging) {
    Color fill;
    if      ( s.lit &&  s.pressed) fill = C_LIT_PRESSED;
    else if (!s.lit &&  s.pressed) fill = C_PRESSED;
    else if ( s.lit && !s.pressed) fill = C_LIT;
    else                           fill = C_UNLIT;

    DrawRectangle(r.x, r.y, r.w, r.h, fill);
    DrawRectangleLines(r.x, r.y, r.w, r.h, dragging ? C_DRAG : C_BORDER);

    const char* primary = display_button_label(id, def);
    int fs = 9;
    int tw = MeasureText(primary, fs);
    while (tw > r.w - 4 && fs > 6) { --fs; tw = MeasureText(primary, fs); }

    bool has_alt = (def.alt1 != nullptr);
    int  ty = r.y + (r.h - fs) / 2 - (has_alt ? 5 : 0);
    DrawText(primary, r.x + (r.w - tw)/2, ty, fs, C_TEXT);

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

static void draw_knob(const KnobPos& k, const sp303::KnobDef& def, float value, bool dragging, const KnobSpriteAsset& sprite) {
    (void)def;
    (void)dragging;
    const int cx = k.x;
    const int cy = k.y;
    const float start_angle = 135.0f;
    const float end_angle   = 405.0f;
    const float angle_deg   = start_angle + std::clamp(value, 0.0f, 1.0f) * (end_angle - start_angle);
    const float angle_rad   = angle_deg * (PI / 180.0f);
    const int pointer_len   = std::max(8, k.r - 7);
    if (sprite.loaded) {
        float scale = (float)(k.r * 2) / (float)std::max(sprite.texture.width, sprite.texture.height);
        float draw_w = (float)sprite.texture.width * scale;
        float draw_h = (float)sprite.texture.height * scale;
        Vector2 pos = { (float)cx - draw_w / 2.0f, (float)cy - draw_h / 2.0f };
        DrawTextureEx(sprite.texture, pos, 0.0f, scale, WHITE);
        int px = cx + (int)std::lround(std::cos(angle_rad) * pointer_len);
        int py = cy + (int)std::lround(std::sin(angle_rad) * pointer_len);
        DrawLineEx({(float)cx, (float)cy}, {(float)px, (float)py}, std::max(2.0f, k.r / 7.0f), C_KNOB_THUMB);
        DrawCircle(cx, cy, std::max(3.0f, k.r / 8.0f), C_KNOB_THUMB);
        return;
    }
    DrawCircle(cx, cy, (float)(k.r + 2), C_BORDER);
    DrawCircle(cx, cy, (float)k.r, C_KNOB_TRACK);
    DrawCircle(cx, cy, (float)(k.r - 5), C_BG);
    int px = cx + (int)std::lround(std::cos(angle_rad) * pointer_len);
    int py = cy + (int)std::lround(std::sin(angle_rad) * pointer_len);
    DrawLineEx({(float)cx, (float)cy}, {(float)px, (float)py}, std::max(3.0f, k.r / 7.0f), C_KNOB_THUMB);
    DrawCircle(cx, cy, std::max(6.0f, k.r / 4.0f), C_KNOB_THUMB);
}

// ─── PEAK indicator ───────────────────────────────────────────────────────────

static void draw_peak(const IndPos& p, bool lit) {
    DrawCircle(p.x, p.y, (float)(p.r + 1), C_BORDER);
    DrawCircle(p.x, p.y, (float)p.r,       lit ? C_PEAK_ON : C_PEAK_OFF);
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
    int dx = mx - k.x;
    int dy = my - k.y;
    return dx*dx + dy*dy <= (k.r + 10) * (k.r + 10);
}

static int snap_to_grid(int v) {
    if (DRAG_GRID <= 1) return v;
    float g = (float)DRAG_GRID;
    return (int)std::lround(v / g) * DRAG_GRID;
}

struct RenderTransform {
    float scale = 1.0f;
    int offx = 0;
    int offy = 0;
};

static RenderTransform compute_transform(int screen_w, int screen_h, int design_w, int design_h) {
    RenderTransform t{};
    if (design_w <= 0 || design_h <= 0) return t;
    float sx = (float)screen_w / (float)design_w;
    float sy = (float)screen_h / (float)design_h;
    t.scale = std::max(std::min(sx, sy), 0.01f);
    int drawn_w = (int)std::lround((float)design_w * t.scale);
    int drawn_h = (int)std::lround((float)design_h * t.scale);
    t.offx = (screen_w - drawn_w) / 2;
    t.offy = (screen_h - drawn_h) / 2;
    return t;
}

static int to_screen_x(int x, const RenderTransform& t) {
    return t.offx + (int)std::lround((float)x * t.scale);
}

static int to_screen_y(int y, const RenderTransform& t) {
    return t.offy + (int)std::lround((float)y * t.scale);
}

static int to_design_x(int x, const RenderTransform& t) {
    return (int)std::lround((float)(x - t.offx) / t.scale);
}

static int to_design_y(int y, const RenderTransform& t) {
    return (int)std::lround((float)(y - t.offy) / t.scale);
}

static BtnPos scale_btn(const BtnPos& r, const RenderTransform& t) {
    return {
        to_screen_x(r.x, t),
        to_screen_y(r.y, t),
        std::max((int)std::lround((float)r.w * t.scale), 1),
        std::max((int)std::lround((float)r.h * t.scale), 1)
    };
}

static IndPos scale_ind(const IndPos& p, const RenderTransform& t) {
    return {
        to_screen_x(p.x, t),
        to_screen_y(p.y, t),
        std::max((int)std::lround((float)p.r * t.scale), 1)
    };
}

static DispPos scale_disp(const DispPos& d, const RenderTransform& t) {
    return {
        to_screen_x(d.x, t),
        to_screen_y(d.y, t),
        std::max((int)std::lround((float)d.dw * t.scale), 1),
        std::max((int)std::lround((float)d.dh * t.scale), 1),
        std::max((int)std::lround((float)d.gap * t.scale), 1)
    };
}

static KnobPos scale_knob(const KnobPos& k, const RenderTransform& t) {
    return {
        to_screen_x(k.x, t),
        to_screen_y(k.y, t),
        std::max((int)std::lround((float)k.r * t.scale), 1)
    };
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

static void mirror_pad_button(Layout& layout, int id) {
    if (id < sp303::BTN_PAD_1 || id > sp303::BTN_PAD_8) return;
    int slot = id - sp303::BTN_PAD_1;
    BtnPos p = layout.buttons[id];
    for (int b = 1; b < 4; ++b)
        layout.buttons[sp303::BTN_PAD_1 + b*8 + slot] = p;
}

static void resize_button_vertical(Layout& layout, int id, int delta) {
    if (id < 0 || id >= sp303::BTN_COUNT) return;
    BtnPos& b = layout.buttons[id];
    int new_h = std::clamp(b.h + delta, 22, 220);
    b.y -= (new_h - b.h) / 2;
    b.h = new_h;
    mirror_pad_button(layout, id);
}

static void resize_button_horizontal(Layout& layout, int id, int delta) {
    if (id < 0 || id >= sp303::BTN_COUNT) return;
    BtnPos& b = layout.buttons[id];
    int new_w = std::clamp(b.w + delta, 28, 260);
    b.x -= (new_w - b.w) / 2;
    b.w = new_w;
    mirror_pad_button(layout, id);
}

static void resize_display_linear(Layout& layout, int delta) {
    int old_dw = std::max(layout.disp.dw, 1);
    int new_dw = std::clamp(old_dw + delta, 12, 240);
    float scale = (float)new_dw / (float)old_dw;
    layout.disp.x -= (int)std::lround((float)(new_dw - old_dw) * 1.5f);
    layout.disp.y -= (int)std::lround((float)(layout.disp.dh * scale - layout.disp.dh) * 0.5f);
    layout.disp.dw = new_dw;
    layout.disp.dh = std::max((int)std::lround((float)layout.disp.dh * scale), 16);
    layout.disp.gap = std::max((int)std::lround((float)layout.disp.gap * scale), 1);
}

static void resize_knob_linear(Layout& layout, int knob_index, int delta) {
    if (knob_index < 0 || knob_index >= sp303::KNOB_COUNT) return;
    layout.knobs[knob_index].r = std::clamp(layout.knobs[knob_index].r + delta, 8, 160);
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(void) {
    RendererController controller{};
    renderer_controller_init(&controller);

    bool config_open = false;

    sp303::Device* dev    = sp303::create();
    renderer_controller_mount_card(&controller, dev);
    Layout         layout = load_layout();
    KnobSpriteAsset knob_sprite = load_knob_sprite_asset();
    Drag           drag;
    int            pressed_btn = -1;
    int            active_knob = -1;
    int            knob_drag_start_y = 0;
    float          knob_drag_start_value = 0.0f;
    std::vector<OverlayCacheEntry> hitbox_overlays(sp303::BTN_PAD_8 + 1);

    auto keymap = load_keymap();
    std::unordered_map<int, sp303::ButtonID> key_held;
    WindowPrefs window_prefs = load_window_prefs();
    bool show_hitboxes = window_prefs.show_hitboxes;
    int last_saved_w = window_prefs.w;
    int last_saved_h = window_prefs.h;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(window_prefs.w, window_prefs.h, "SP-303");
    BackgroundAsset background = load_background_asset();
    if (background.loaded) {
        SetTextureFilter(background.texture, TEXTURE_FILTER_BILINEAR);
        int bg_w = std::max(background_width(background) / 2, 1);
        int bg_h = std::max(background_height(background) / 2, 1);
        if (GetScreenWidth() != bg_w || GetScreenHeight() != bg_h) {
            SetWindowSize(bg_w, bg_h);
        }
        save_window_prefs(bg_w, bg_h, show_hitboxes);
        last_saved_w = bg_w;
        last_saved_h = bg_h;
        prebuild_hitbox_overlay_cache(hitbox_overlays, background, layout);
    }
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        int screen_w = GetScreenWidth();
        int screen_h = GetScreenHeight();
        if (screen_w != last_saved_w || screen_h != last_saved_h) {
            save_window_prefs(screen_w, screen_h, show_hitboxes);
            last_saved_w = screen_w;
            last_saved_h = screen_h;
        }
        int design_w = background.loaded ? background_width(background) : SW;
        int design_h = background.loaded ? background_height(background) : SH;
        RenderTransform xf = compute_transform(screen_w, screen_h, design_w, design_h);
        Vector2 mouse = GetMousePosition();
        int  mx    = (int)mouse.x;
        int  my    = (int)mouse.y;
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

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
                renderer_controller_mount_card(&controller, loaded_dev);
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
            if (shift && show_hitboxes) {
                for (int i = 0; i < sp303::BTN_COUNT && drag.target == DRAG_NONE; ++i) {
                    if (i > sp303::BTN_PAD_8 && i <= sp303::BTN_PAD_32) continue;
                    BtnPos sb = scale_btn(layout.buttons[i], xf);
                    if (hit_btn(mx, my, sb))
                        drag = { i, to_design_x(mx, xf) - layout.buttons[i].x, to_design_y(my, xf) - layout.buttons[i].y };
                }
                if (drag.target == DRAG_NONE && hit_circle(mx, my, scale_ind(layout.peak, xf)))
                    drag = { DRAG_PEAK, to_design_x(mx, xf) - layout.peak.x, to_design_y(my, xf) - layout.peak.y };
                if (drag.target == DRAG_NONE && hit_disp(mx, my, scale_disp(layout.disp, xf)))
                    drag = { DRAG_DISP, to_design_x(mx, xf) - layout.disp.x, to_design_y(my, xf) - layout.disp.y };
                for (int i = 0; i < sp303::KNOB_COUNT && drag.target == DRAG_NONE; ++i) {
                    if (hit_knob(mx, my, scale_knob(layout.knobs[i], xf)))
                        drag = { DRAG_KNOB_0 + i, to_design_x(mx, xf) - layout.knobs[i].x, to_design_y(my, xf) - layout.knobs[i].y };
                }
            } else {
                for (int i = 0; i < sp303::KNOB_COUNT; ++i) {
                    if (hit_knob(mx, my, scale_knob(layout.knobs[i], xf))) {
                        active_knob = i;
                        knob_drag_start_y = my;
                        knob_drag_start_value = sp303::get_state(dev).knobs[i].value;
                        break;
                    }
                }
                if (active_knob < 0) {
                    sp303::State cur = sp303::get_state(dev);
                    bool resample_source = sp303::is_resample_source_select(dev);
                    bool resample_dest = sp303::is_resample_dest_select(dev);
                    bool resample_armed = sp303::is_resample_armed(dev);
                    bool resample_recording = sp303::is_resample_recording(dev);
                    bool pattern_mode = sp303::is_pattern_mode(dev);
                    bool pattern_recording = sp303::is_pattern_recording(dev);
                    bool pattern_erase_mode = sp303::is_pattern_erase_mode(dev);
                    bool mark_held = cur.buttons[sp303::BTN_MARK].pressed;
                    for (int i = 0; i < 8; ++i) {
                        if (hit_btn(mx, my, scale_btn(layout.buttons[sp303::BTN_PAD_1 + i], xf))) {
                            int pad_id = sp303::BTN_PAD_1 + cur.active_bank * 8 + i;
                            sp303::button_down(dev, (sp303::ButtonID)pad_id);
                            pressed_btn = pad_id;
                            int slot = pad_id - sp303::BTN_PAD_1;
                            if (pattern_recording && !pattern_erase_mode) {
                                sp303::note_pad_played(dev, slot);
                                trigger_pad_audio(slot);
                            } else if (resample_source) {
                                sp303::note_pad_played(dev, slot);
                                trigger_pad_audio(slot);
                            } else if (resample_armed) {
                                if (sp303::pad_has_sample(dev, slot)) {
                                    sp303::note_pad_played(dev, slot);
                                    trigger_pad_audio(slot);
                                }
                            } else if (resample_recording) {
                                if (sp303::pad_has_sample(dev, slot)) {
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
                                !(pattern_mode && cur.buttons[sp303::BTN_PATTERN_SELECT].lit) &&
                                !cur.buttons[sp303::BTN_REMAIN].pressed) {
                                sp303::note_pad_played(dev, slot);
                                trigger_pad_audio(slot);
                            } else if (mark_held && sp303::pad_has_sample(dev, slot)) {
                                sp303::note_pad_played(dev, slot);
                                trigger_pad_audio(slot);
                            }
                            break;
                        }
                    }
                    if (pressed_btn < 0) {
                        for (int i = 0; i < sp303::BTN_COUNT; ++i) {
                            if (i >= sp303::BTN_PAD_1 && i <= sp303::BTN_PAD_32) continue;
                            if (hit_btn(mx, my, scale_btn(layout.buttons[i], xf))) {
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

        if (shift && show_hitboxes && !config_open) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                int delta = (wheel > 0.0f) ? 1 : -1;
                bool handled = false;
                if (hit_disp(mx, my, scale_disp(layout.disp, xf))) {
                    resize_display_linear(layout, delta);
                    save_layout(layout);
                    handled = true;
                }
                for (int i = 0; i < sp303::KNOB_COUNT && !handled; ++i) {
                    if (hit_knob(mx, my, scale_knob(layout.knobs[i], xf))) {
                        resize_knob_linear(layout, i, delta);
                        save_layout(layout);
                        handled = true;
                    }
                }
                if (handled) {
                    wheel = 0.0f;
                }
                if (!handled) {
                    for (int i = 0; i < sp303::BTN_COUNT; ++i) {
                        if (i > sp303::BTN_PAD_8 && i <= sp303::BTN_PAD_32) continue;
                        if (hit_btn(mx, my, scale_btn(layout.buttons[i], xf))) {
                            if (ctrl) resize_button_horizontal(layout, i, delta);
                            else resize_button_vertical(layout, i, delta);
                            save_layout(layout);
                            break;
                        }
                    }
                }
            }
        }

        if (drag.target != DRAG_NONE) {
            int t  = drag.target;
            if (t < sp303::BTN_COUNT) {
                int nx = snap_to_grid(to_design_x(mx, xf) - drag.offx);
                int ny = snap_to_grid(to_design_y(my, xf) - drag.offy);
                layout.buttons[t].x = nx;
                layout.buttons[t].y = ny;
                mirror_pad_button(layout, t);
            } else if (t == DRAG_PEAK) {
                int nx = snap_to_grid(to_design_x(mx, xf) - drag.offx);
                int ny = snap_to_grid(to_design_y(my, xf) - drag.offy);
                layout.peak.x = nx; layout.peak.y = ny;
            } else if (t == DRAG_DISP) {
                int nx = snap_to_grid(to_design_x(mx, xf) - drag.offx);
                int ny = snap_to_grid(to_design_y(my, xf) - drag.offy);
                layout.disp.x = nx; layout.disp.y = ny;
            } else if (t >= DRAG_KNOB_0 && t < DRAG_KNOB_0 + sp303::KNOB_COUNT) {
                int nx = to_design_x(mx, xf) - drag.offx;
                int ny = snap_to_grid(to_design_y(my, xf) - drag.offy);
                layout.knobs[t - DRAG_KNOB_0].x = nx;
                layout.knobs[t - DRAG_KNOB_0].y = ny;
            }
        }

        if (active_knob >= 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float delta = (float)(knob_drag_start_y - my) / 160.0f;
            float v = std::clamp(knob_drag_start_value + delta, 0.0f, 1.0f);
            sp303::knob_set(dev, (sp303::KnobID)active_knob, v);
        }

        if (!shift && !config_open) {
            if (IsKeyPressed(KEY_Q)) {
                sp303::button_down(dev, sp303::BTN_CANCEL);
                key_held[KEY_Q] = sp303::BTN_CANCEL;
            }
            if (IsKeyReleased(KEY_Q)) {
                auto it = key_held.find(KEY_Q);
                if (it != key_held.end()) {
                    sp303::button_up(dev, it->second);
                    key_held.erase(it);
                }
            }

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
            bool resample_recording = sp303::is_resample_recording(dev);
            bool pattern_mode = sp303::is_pattern_mode(dev);
            bool pattern_recording = sp303::is_pattern_recording(dev);
            bool pattern_erase_mode = sp303::is_pattern_erase_mode(dev);
            bool mark_held = cur.buttons[sp303::BTN_MARK].pressed;
            for (auto& [key, btn] : keymap) {
                if (IsKeyPressed(key)) {
                    sp303::ButtonID actual = btn;
                    if (btn >= sp303::BTN_PAD_1 && btn <= sp303::BTN_PAD_8)
                        actual = (sp303::ButtonID)(sp303::BTN_PAD_1 + cur.active_bank * 8 + (btn - sp303::BTN_PAD_1));
                    sp303::button_down(dev, actual);
                    key_held[key] = actual;
                    if (actual >= sp303::BTN_PAD_1 && actual <= sp303::BTN_PAD_32) {
                        int slot = actual - sp303::BTN_PAD_1;
                        if (pattern_recording && !pattern_erase_mode) {
                            sp303::note_pad_played(dev, slot);
                            trigger_pad_audio(slot);
                        } else if (resample_source) {
                            sp303::note_pad_played(dev, slot);
                            trigger_pad_audio(slot);
                        } else if (resample_armed) {
                            if (sp303::pad_has_sample(dev, slot)) {
                                sp303::note_pad_played(dev, slot);
                                trigger_pad_audio(slot);
                            }
                        } else if (resample_recording) {
                            if (sp303::pad_has_sample(dev, slot)) {
                                sp303::note_pad_played(dev, slot);
                                trigger_pad_audio(slot);
                            }
                        } else if (!sampling_active &&
                                   !cur.buttons[sp303::BTN_REMAIN].pressed &&
                                   !(pattern_mode && cur.buttons[sp303::BTN_PATTERN_SELECT].lit) &&
                                   !resample_source &&
                                   !resample_dest) {
                            sp303::note_pad_played(dev, slot);
                            trigger_pad_audio(slot);
                        } else if (mark_held && sp303::pad_has_sample(dev, slot)) {
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
        if (background.loaded) {
            Rectangle src = { 0.0f, 0.0f, (float)background.texture.width, (float)background.texture.height };
            Rectangle dst = {
                (float)xf.offx,
                (float)xf.offy,
                (float)std::lround((float)design_w * xf.scale),
                (float)std::lround((float)design_h * xf.scale)
            };
            DrawTexturePro(background.texture, src, dst, {0.0f, 0.0f}, 0.0f, WHITE);
        }

        if (background.loaded) {
            for (int i = 0; i < sp303::BTN_COUNT; ++i) {
                if (i >= sp303::BTN_PAD_1 && i <= sp303::BTN_PAD_32) continue;
                bool pressed = state.buttons[i].pressed;
                bool lit = state.buttons[i].lit;
                if (!pressed && !lit) continue;
                if (i < 0 || i >= (int)hitbox_overlays.size()) continue;
                BtnPos design_rect = layout.buttons[i];
                OverlayState overlay_state =
                    (pressed && lit) ? OVERLAY_LITPRESSED :
                    (lit ? OVERLAY_LIT : OVERLAY_PRESSED);
                Texture2D overlay = get_hitbox_overlay_texture(hitbox_overlays[i], background, design_rect, overlay_state);
                if (overlay.id == 0) continue;
                Rectangle osrc = { 0.0f, 0.0f, (float)overlay.width, (float)overlay.height };
                Rectangle odst = {
                    (float)xf.offx,
                    (float)xf.offy,
                    (float)std::lround((float)design_w * xf.scale),
                    (float)std::lround((float)design_h * xf.scale)
                };
                DrawTexturePro(overlay, osrc, odst, {0.0f, 0.0f}, 0.0f, WHITE);
            }

            int bank_off = state.active_bank * 8;
            for (int i = 0; i < 8; ++i) {
                int pad_id = sp303::BTN_PAD_1 + bank_off + i;
                bool pressed = state.buttons[pad_id].pressed;
                bool lit = state.buttons[pad_id].lit;
                if (!pressed && !lit) continue;
                int cache_id = sp303::BTN_PAD_1 + i;
                BtnPos design_rect = layout.buttons[cache_id];
                OverlayState overlay_state =
                    (pressed && lit) ? OVERLAY_LITPRESSED :
                    (lit ? OVERLAY_LIT : OVERLAY_PRESSED);
                Texture2D overlay = get_hitbox_overlay_texture(hitbox_overlays[cache_id], background, design_rect, overlay_state);
                if (overlay.id == 0) continue;
                Rectangle osrc = { 0.0f, 0.0f, (float)overlay.width, (float)overlay.height };
                Rectangle odst = {
                    (float)xf.offx,
                    (float)xf.offy,
                    (float)std::lround((float)design_w * xf.scale),
                    (float)std::lround((float)design_h * xf.scale)
                };
                DrawTexturePro(overlay, osrc, odst, {0.0f, 0.0f}, 0.0f, WHITE);
            }
        }

        if (show_hitboxes) {
            for (int i = 0; i < sp303::BTN_COUNT; ++i) {
                if (i >= sp303::BTN_PAD_1 && i <= sp303::BTN_PAD_32) continue;
                draw_button((sp303::ButtonID)i, scale_btn(layout.buttons[i], xf), sp303::BUTTON_DEFS[i],
                            state.buttons[i], drag.target == i);
            }

            int bank_off = state.active_bank * 8;
            for (int i = 0; i < 8; ++i) {
                int pad_id    = sp303::BTN_PAD_1 + bank_off + i;
                int layout_id = sp303::BTN_PAD_1 + i;
                draw_button((sp303::ButtonID)pad_id, scale_btn(layout.buttons[layout_id], xf), sp303::BUTTON_DEFS[pad_id],
                            state.buttons[pad_id], drag.target == layout_id);
            }
        }

        for (int i = 0; i < sp303::KNOB_COUNT; ++i)
            draw_knob(scale_knob(layout.knobs[i], xf), sp303::KNOB_DEFS[i],
                      state.knobs[i].value, active_knob == i || drag.target == DRAG_KNOB_0 + i,
                      knob_sprite);

        draw_display(scale_disp(layout.disp, xf), state.display);
        draw_peak(scale_ind(layout.peak, xf), state.indicators[sp303::IND_PEAK].lit);

        if (!config_open && shift && show_hitboxes)
            DrawText("SHIFT + drag: move  |  SHIFT + wheel on button: resize height  |  SHIFT + CTRL + wheel on button: resize width", 10, screen_h - 18, 9, C_ALT);
        if (!config_open)
            DrawText("[F5] quick-save  [F9] quick-load  [TAB] audio config", screen_w - 330, screen_h - 18, 9, C_ALT);

        if (config_open) {
            bool apply = draw_config_screen(
                screen_w, screen_h,
                controller.sel_out, controller.sel_in, controller.sel_rate, controller.sel_buf, controller.sel_card, controller.peak_threshold,
                controller.out_devs, controller.in_devs, controller.card_dirs, controller.playback_ok,
                mx, my, IsMouseButtonPressed(MOUSE_BUTTON_LEFT), IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                controller.config_input_peak, show_hitboxes);

            save_window_prefs(screen_w, screen_h, show_hitboxes);

            if (apply) {
                if (!controller.card_dirs.empty()) {
                    controller.card_path = controller.card_dirs[controller.sel_card];
                }
                renderer_controller_apply_audio_config(&controller);
                renderer_controller_refresh_cards(&controller);
                renderer_controller_mount_card(&controller, dev);
            }
        }

        EndDrawing();
    }

    save_layout(layout);
    unload_hitbox_overlay_cache(hitbox_overlays);
    if (background.loaded) {
        UnloadImage(background.image);
        UnloadTexture(background.texture);
    }
    if (knob_sprite.loaded) {
        UnloadTexture(knob_sprite.texture);
    }
    sp303::destroy(dev);
    renderer_controller_shutdown(&controller);
    CloseWindow();
    return 0;
}
