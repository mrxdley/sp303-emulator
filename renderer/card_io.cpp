#include "card_io.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

static constexpr int CARD_VERSION = 1;

struct LoadedSample {
    sp303::PadProjectState state{};
    std::vector<float> pcm;
    uint32_t channels = 1;
    uint32_t frames = 0;
    int start_127 = 0;
    int end_127 = 127;
    int level_127 = 127;
    int bpm_adjust = 0;
    int time_mode = 0;
    int time_target_bpm = -1;
};

static void write_u16(std::ofstream& out, uint16_t v) {
    char bytes[2] = { (char)(v & 0xff), (char)((v >> 8) & 0xff) };
    out.write(bytes, 2);
}

static void write_u32(std::ofstream& out, uint32_t v) {
    char bytes[4] = {
        (char)(v & 0xff), (char)((v >> 8) & 0xff), (char)((v >> 16) & 0xff), (char)((v >> 24) & 0xff)
    };
    out.write(bytes, 4);
}

static uint16_t read_u16(std::ifstream& in) {
    uint8_t bytes[2] = {};
    in.read(reinterpret_cast<char*>(bytes), 2);
    return (uint16_t)(bytes[0] | (bytes[1] << 8));
}

static uint32_t read_u32(std::ifstream& in) {
    uint8_t bytes[4] = {};
    in.read(reinterpret_cast<char*>(bytes), 4);
    return (uint32_t)(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
}

static bool write_wav_f32(const std::filesystem::path& path,
                          const std::vector<float>& pcm,
                          uint32_t channels,
                          uint32_t sample_rate) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;
    const uint32_t data_size = (uint32_t)(pcm.size() * sizeof(float));
    const uint32_t riff_size = 4 + 8 + 16 + 8 + data_size;
    const uint16_t block_align = (uint16_t)(channels * sizeof(float));
    const uint32_t byte_rate = sample_rate * block_align;
    out.write("RIFF", 4);
    write_u32(out, riff_size);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write_u32(out, 16);
    write_u16(out, 3);
    write_u16(out, (uint16_t)channels);
    write_u32(out, sample_rate);
    write_u32(out, byte_rate);
    write_u16(out, block_align);
    write_u16(out, 32);
    out.write("data", 4);
    write_u32(out, data_size);
    out.write(reinterpret_cast<const char*>(pcm.data()), (std::streamsize)data_size);
    return out.good();
}

static bool read_wav(const std::filesystem::path& path,
                     std::vector<float>* pcm,
                     uint32_t* channels,
                     uint32_t* sample_rate) {
    if (!pcm || !channels || !sample_rate) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    char riff[4], wave[4];
    in.read(riff, 4);
    (void)read_u32(in);
    in.read(wave, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0 || std::strncmp(wave, "WAVE", 4) != 0) return false;

    uint16_t fmt_code = 0, fmt_channels = 0, bits_per_sample = 0;
    uint32_t fmt_rate = 0;
    std::vector<uint8_t> data_chunk;
    while (in.good() && !in.eof()) {
        char chunk_id[4];
        in.read(chunk_id, 4);
        if (in.gcount() != 4) break;
        uint32_t chunk_size = read_u32(in);
        if (std::strncmp(chunk_id, "fmt ", 4) == 0) {
            fmt_code = read_u16(in);
            fmt_channels = read_u16(in);
            fmt_rate = read_u32(in);
            (void)read_u32(in);
            (void)read_u16(in);
            bits_per_sample = read_u16(in);
            if (chunk_size > 16) in.seekg(chunk_size - 16, std::ios::cur);
        } else if (std::strncmp(chunk_id, "data", 4) == 0) {
            data_chunk.resize(chunk_size);
            in.read(reinterpret_cast<char*>(data_chunk.data()), chunk_size);
        } else {
            in.seekg(chunk_size, std::ios::cur);
        }
        if (chunk_size & 1) in.seekg(1, std::ios::cur);
    }
    if (fmt_channels == 0 || fmt_rate == 0 || data_chunk.empty()) return false;
    pcm->clear();
    if (fmt_code == 3 && bits_per_sample == 32) {
        const size_t sample_count = data_chunk.size() / sizeof(float);
        pcm->resize(sample_count);
        std::memcpy(pcm->data(), data_chunk.data(), sample_count * sizeof(float));
    } else if (fmt_code == 1 && bits_per_sample == 16) {
        const size_t sample_count = data_chunk.size() / sizeof(int16_t);
        pcm->reserve(sample_count);
        const int16_t* src = reinterpret_cast<const int16_t*>(data_chunk.data());
        for (size_t i = 0; i < sample_count; ++i) pcm->push_back(std::clamp(src[i] / 32768.0f, -1.0f, 1.0f));
    } else {
        return false;
    }
    *channels = fmt_channels;
    *sample_rate = fmt_rate;
    return true;
}

static std::string stem_for_slot(const char* prefix, int index) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s_%02d.wav", prefix, index + 1);
    return std::string(buf);
}

static CardIoResult make_result(bool ok, std::string message) {
    return CardIoResult{ok, std::move(message)};
}

} // namespace

CardIoResult card_save(const std::filesystem::path& card_dir,
                       sp303::Device* dev,
                       sp303::Audio* audio,
                       const sp303::AudioConfig& cfg) {
    if (!dev || !audio) return make_result(false, "card save failed: device/audio unavailable");
    std::error_code ec;
    const auto temp_dir = card_dir.string() + ".tmp";
    std::filesystem::remove_all(temp_dir, ec);
    std::filesystem::create_directories(std::filesystem::path(temp_dir) / "samples", ec);
    if (ec) return make_result(false, "card save failed: could not create temp card directory");

    json root{
        {"version", CARD_VERSION},
        {"audio_sample_rate", cfg.sample_rate},
        {"formatted", sp303::get_memory_card_formatted(dev)},
        {"write_protected", sp303::get_memory_card_write_protected(dev)},
        {"live_pads_cd", json::array()},
        {"live_patterns_cd", json::array()},
        {"backup_slots", json::array()},
    };

    for (int slot = 16; slot < 32; ++slot) {
        sp303::PadProjectState pad{};
        sp303::get_pad_project_state(dev, slot, &pad);
        json jpad{
            {"slot", slot},
            {"has_sample", pad.has_sample},
            {"loop", pad.loop_mode},
            {"gate", pad.gate_mode},
            {"reverse", pad.reverse_mode},
            {"has_effect", pad.has_effect},
            {"bpm_adjust", 0},
            {"time_mode", 0},
            {"time_target_bpm", -1},
        };
        if (pad.has_sample) {
            std::vector<float> pcm;
            uint32_t channels = 1, frames = 0;
            if (!sp303::audio_export_sample(audio, slot, &pcm, &channels, &frames)) {
                return make_result(false, "card save failed: could not export live card sample");
            }
            std::string wav_name = stem_for_slot("live", slot - 16);
            if (!write_wav_f32(std::filesystem::path(temp_dir) / "samples" / wav_name, pcm, channels, cfg.sample_rate)) {
                return make_result(false, "card save failed: could not write live card sample wav");
            }
            jpad["sample_file"] = wav_name;
            jpad["start_127"] = sp303::audio_get_sample_start(audio, slot);
            jpad["end_127"] = sp303::audio_get_sample_end(audio, slot);
            jpad["level_127"] = sp303::audio_get_sample_level(audio, slot);
            jpad["bpm_adjust"] = sp303::audio_get_sample_bpm_adjust(audio, slot);
            jpad["time_mode"] = sp303::audio_get_sample_time_mode(audio, slot);
            jpad["time_target_bpm"] = sp303::audio_get_sample_time_target_bpm(audio, slot);
        }
        root["live_pads_cd"].push_back(jpad);
    }

    for (int slot = 16; slot < 32; ++slot) {
        sp303::PatternProjectSlot pslot{};
        sp303::get_pattern_project_slot(dev, slot, &pslot);
        json jslot{
            {"slot", slot},
            {"assigned", pslot.assigned},
            {"length_measures", pslot.length_measures},
            {"quantize", pslot.quantize},
            {"metronome_level", pslot.metronome_level},
            {"events", json::array()},
        };
        int event_count = 0;
        sp303::get_pattern_project_events(dev, slot, nullptr, 0, &event_count);
        if (event_count > 0) {
            std::vector<sp303::PatternProjectEvent> events((size_t)event_count);
            sp303::get_pattern_project_events(dev, slot, events.data(), event_count, &event_count);
            for (const auto& ev : events) {
                jslot["events"].push_back({{"tick", ev.tick}, {"sample_pad", ev.sample_pad}, {"velocity", ev.velocity}});
            }
        }
        root["live_patterns_cd"].push_back(jslot);
    }

    for (int backup_slot = 0; backup_slot < 8; ++backup_slot) {
        json jbackup{
            {"slot", backup_slot},
            {"kind", sp303::get_memory_card_backup_kind(dev, backup_slot)},
            {"pattern_bpm", sp303::get_memory_card_backup_pattern_bpm(dev, backup_slot)},
            {"sample_states", json::array()},
            {"pattern_slots", json::array()},
        };
        for (int i = 0; i < 16; ++i) {
            sp303::PadProjectState state{};
            sp303::get_memory_card_backup_sample_state(dev, backup_slot, i, &state);
            json js{
                {"index", i},
                {"has_sample", state.has_sample},
                {"loop", state.loop_mode},
                {"gate", state.gate_mode},
                {"reverse", state.reverse_mode},
                {"has_effect", state.has_effect},
            };
            if (state.has_sample) {
                std::vector<float> pcm;
                uint32_t channels = 1, frames = 0;
                if (!sp303::get_memory_card_backup_sample_audio(dev, backup_slot, i, &pcm, &channels, &frames)) {
                    return make_result(false, "card save failed: could not export backup sample");
                }
                int start_127 = 0, end_127 = 127, level_127 = 127;
                int bpm_adjust = 0, time_mode = 0, time_target_bpm = -1;
                sp303::get_memory_card_backup_sample_edit(dev, backup_slot, i,
                    &start_127, &end_127, &level_127, &bpm_adjust, &time_mode, &time_target_bpm);
                std::string wav_name = "backup_" + std::to_string(backup_slot + 1) + "_" + std::to_string(i + 1) + ".wav";
                if (!write_wav_f32(std::filesystem::path(temp_dir) / "samples" / wav_name, pcm, channels, cfg.sample_rate)) {
                    return make_result(false, "card save failed: could not write backup sample wav");
                }
                js["sample_file"] = wav_name;
                js["start_127"] = start_127;
                js["end_127"] = end_127;
                js["level_127"] = level_127;
                js["bpm_adjust"] = bpm_adjust;
                js["time_mode"] = time_mode;
                js["time_target_bpm"] = time_target_bpm;
            }
            jbackup["sample_states"].push_back(js);
        }
        for (int i = 0; i < 16; ++i) {
            sp303::PatternProjectSlot pslot{};
            sp303::get_memory_card_backup_pattern_slot(dev, backup_slot, i, &pslot);
            json jp{
                {"index", i},
                {"assigned", pslot.assigned},
                {"length_measures", pslot.length_measures},
                {"quantize", pslot.quantize},
                {"metronome_level", pslot.metronome_level},
                {"events", json::array()},
            };
            int event_count = 0;
            sp303::get_memory_card_backup_pattern_events(dev, backup_slot, i, nullptr, 0, &event_count);
            if (event_count > 0) {
                std::vector<sp303::PatternProjectEvent> events((size_t)event_count);
                sp303::get_memory_card_backup_pattern_events(dev, backup_slot, i, events.data(), event_count, &event_count);
                for (const auto& ev : events) {
                    jp["events"].push_back({{"tick", ev.tick}, {"sample_pad", ev.sample_pad}, {"velocity", ev.velocity}});
                }
            }
            jbackup["pattern_slots"].push_back(jp);
        }
        root["backup_slots"].push_back(jbackup);
    }

    std::ofstream manifest(std::filesystem::path(temp_dir) / "card.json");
    if (!manifest.is_open()) return make_result(false, "card save failed: could not write manifest");
    manifest << root.dump(2);
    manifest.close();

    std::filesystem::remove_all(card_dir, ec);
    std::filesystem::rename(temp_dir, card_dir, ec);
    if (ec) return make_result(false, "card save failed: could not move card image into place");
    return make_result(true, "saved card to " + card_dir.string());
}

CardIoResult card_load(const std::filesystem::path& card_dir,
                       sp303::Device* dev,
                       sp303::Audio* audio,
                       const sp303::AudioConfig& cfg) {
    if (!dev || !audio) return make_result(false, "card load failed: device/audio unavailable");
    const auto manifest_path = card_dir / "card.json";
    if (!std::filesystem::exists(manifest_path)) {
        return make_result(true, "card image not found; using blank card");
    }
    std::ifstream manifest(manifest_path);
    if (!manifest.is_open()) return make_result(false, "card load failed: could not open manifest");
    json root;
    try { manifest >> root; } catch (...) { return make_result(false, "card load failed: invalid card.json"); }
    if (root.value("version", 0) != CARD_VERSION) return make_result(false, "card load failed: unsupported card version");

    uint32_t card_rate = root.value("audio_sample_rate", cfg.sample_rate);
    sp303::set_memory_card_formatted(dev, root.value("formatted", true));
    sp303::set_memory_card_write_protected(dev, root.value("write_protected", false));

    for (int slot = 16; slot < 32; ++slot) {
        sp303::audio_clear_sample(audio, slot);
        sp303::pad_clear_sample(dev, slot);
    }
    sp303::set_pattern_bpm(dev, sp303::get_pattern_bpm(dev));
    for (int slot = 16; slot < 32; ++slot) {
        sp303::PatternProjectSlot empty{};
        empty.assigned = false;
        sp303::set_pattern_project_slot(dev, slot, empty);
        sp303::set_pattern_project_events(dev, slot, nullptr, 0);
    }
    for (int b = 0; b < 8; ++b) {
        sp303::set_memory_card_backup_kind(dev, b, sp303::MEMORY_CARD_BACKUP_EMPTY);
        sp303::set_memory_card_backup_pattern_bpm(dev, b, 120);
        for (int i = 0; i < 16; ++i) {
            sp303::PadProjectState empty_state{};
            sp303::set_memory_card_backup_sample_state(dev, b, i, empty_state);
            sp303::set_memory_card_backup_sample_audio(dev, b, i, nullptr, 0, 1);
            sp303::set_memory_card_backup_sample_edit(dev, b, i, 0, 127, 127, 0, 0, -1);
            sp303::PatternProjectSlot empty_pattern{};
            sp303::set_memory_card_backup_pattern_slot(dev, b, i, empty_pattern);
            sp303::set_memory_card_backup_pattern_events(dev, b, i, nullptr, 0);
        }
    }

    if (root.contains("live_pads_cd") && root["live_pads_cd"].is_array()) {
        for (const auto& jpad : root["live_pads_cd"]) {
            int slot = jpad.value("slot", -1);
            if (slot < 16 || slot >= 32) continue;
            sp303::PadProjectState state{};
            state.has_sample = jpad.value("has_sample", false);
            state.loop_mode = jpad.value("loop", false);
            state.gate_mode = jpad.value("gate", false);
            state.reverse_mode = jpad.value("reverse", false);
            state.has_effect = jpad.value("has_effect", false);
            if (state.has_sample) {
                std::vector<float> pcm;
                uint32_t channels = 1, wav_rate = card_rate;
                auto wav_name = jpad.value("sample_file", std::string{});
                if (!wav_name.empty() &&
                    read_wav(card_dir / "samples" / wav_name, &pcm, &channels, &wav_rate) &&
                    !pcm.empty()) {
                    uint32_t frames = (uint32_t)(pcm.size() / std::max(1u, channels));
                    sp303::audio_import_sample(audio, slot, pcm.data(), frames, channels);
                    sp303::audio_set_sample_start(audio, slot, jpad.value("start_127", 0));
                    sp303::audio_set_sample_end(audio, slot, jpad.value("end_127", 127));
                    sp303::audio_set_sample_level(audio, slot, jpad.value("level_127", 127));
                    sp303::audio_set_sample_bpm_adjust(audio, slot, jpad.value("bpm_adjust", 0));
                    sp303::audio_set_sample_time_mode(audio, slot, jpad.value("time_mode", 0), jpad.value("time_target_bpm", -1));
                } else {
                    state.has_sample = false;
                }
            }
            sp303::set_pad_project_state(dev, slot, state);
        }
    }

    if (root.contains("live_patterns_cd") && root["live_patterns_cd"].is_array()) {
        for (const auto& jp : root["live_patterns_cd"]) {
            int slot = jp.value("slot", -1);
            if (slot < 16 || slot >= 32) continue;
            sp303::PatternProjectSlot pslot{};
            pslot.assigned = jp.value("assigned", false);
            pslot.length_measures = jp.value("length_measures", 1);
            pslot.quantize = jp.value("quantize", 4);
            pslot.metronome_level = jp.value("metronome_level", 100);
            sp303::set_pattern_project_slot(dev, slot, pslot);
            std::vector<sp303::PatternProjectEvent> events;
            if (jp.contains("events") && jp["events"].is_array()) {
                for (const auto& je : jp["events"]) {
                    events.push_back({je.value("tick", 0), je.value("sample_pad", 0), std::clamp(je.value("velocity", 127), 1, 127)});
                }
            }
            sp303::set_pattern_project_events(dev, slot, events.data(), (int)events.size());
        }
    }

    if (root.contains("backup_slots") && root["backup_slots"].is_array()) {
        for (const auto& jb : root["backup_slots"]) {
            int backup_slot = jb.value("slot", -1);
            if (backup_slot < 0 || backup_slot >= 8) continue;
            sp303::set_memory_card_backup_kind(dev, backup_slot, jb.value("kind", 0));
            sp303::set_memory_card_backup_pattern_bpm(dev, backup_slot, jb.value("pattern_bpm", 120));
            if (jb.contains("sample_states") && jb["sample_states"].is_array()) {
                for (const auto& js : jb["sample_states"]) {
                    int index = js.value("index", -1);
                    if (index < 0 || index >= 16) continue;
                    sp303::PadProjectState state{};
                    state.has_sample = js.value("has_sample", false);
                    state.loop_mode = js.value("loop", false);
                    state.gate_mode = js.value("gate", false);
                    state.reverse_mode = js.value("reverse", false);
                    state.has_effect = js.value("has_effect", false);
                    sp303::set_memory_card_backup_sample_state(dev, backup_slot, index, state);
                    if (state.has_sample) {
                        std::vector<float> pcm;
                        uint32_t channels = 1, wav_rate = card_rate;
                        auto wav_name = js.value("sample_file", std::string{});
                        if (!wav_name.empty() && read_wav(card_dir / "samples" / wav_name, &pcm, &channels, &wav_rate) && !pcm.empty()) {
                            uint32_t frames = (uint32_t)(pcm.size() / std::max(1u, channels));
                            sp303::set_memory_card_backup_sample_audio(dev, backup_slot, index, pcm.data(), frames, channels);
                            sp303::set_memory_card_backup_sample_edit(dev, backup_slot, index,
                                js.value("start_127", 0), js.value("end_127", 127), js.value("level_127", 127),
                                js.value("bpm_adjust", 0), js.value("time_mode", 0), js.value("time_target_bpm", -1));
                        } else {
                            state.has_sample = false;
                            sp303::set_memory_card_backup_sample_state(dev, backup_slot, index, state);
                        }
                    }
                }
            }
            if (jb.contains("pattern_slots") && jb["pattern_slots"].is_array()) {
                for (const auto& jp : jb["pattern_slots"]) {
                    int index = jp.value("index", -1);
                    if (index < 0 || index >= 16) continue;
                    sp303::PatternProjectSlot pslot{};
                    pslot.assigned = jp.value("assigned", false);
                    pslot.length_measures = jp.value("length_measures", 1);
                    pslot.quantize = jp.value("quantize", 4);
                    pslot.metronome_level = jp.value("metronome_level", 100);
                    sp303::set_memory_card_backup_pattern_slot(dev, backup_slot, index, pslot);
                    std::vector<sp303::PatternProjectEvent> events;
                    if (jp.contains("events") && jp["events"].is_array()) {
                        for (const auto& je : jp["events"]) {
                            events.push_back({je.value("tick", 0), je.value("sample_pad", 0), std::clamp(je.value("velocity", 127), 1, 127)});
                        }
                    }
                    sp303::set_memory_card_backup_pattern_events(dev, backup_slot, index, events.data(), (int)events.size());
                }
            }
        }
    }

    sp303::consume_memory_card_dirty(dev);
    return make_result(true, "loaded card from " + card_dir.string());
}
