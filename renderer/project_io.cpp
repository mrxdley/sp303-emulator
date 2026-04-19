#include "project_io.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

static constexpr int PROJECT_VERSION = 1;

struct LoadedPad {
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
    char bytes[2] = {
        static_cast<char>(v & 0xff),
        static_cast<char>((v >> 8) & 0xff),
    };
    out.write(bytes, 2);
}

static void write_u32(std::ofstream& out, uint32_t v) {
    char bytes[4] = {
        static_cast<char>(v & 0xff),
        static_cast<char>((v >> 8) & 0xff),
        static_cast<char>((v >> 16) & 0xff),
        static_cast<char>((v >> 24) & 0xff),
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

static std::string pad_file_stem(int slot) {
    const int bank = slot / 8;
    const int pad = (slot % 8) + 1;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%c%d", 'A' + bank, pad);
    return std::string(buf);
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

    char riff[4];
    char wave[4];
    in.read(riff, 4);
    (void)read_u32(in);
    in.read(wave, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0 || std::strncmp(wave, "WAVE", 4) != 0) {
        return false;
    }

    uint16_t fmt_code = 0;
    uint16_t fmt_channels = 0;
    uint32_t fmt_rate = 0;
    uint16_t bits_per_sample = 0;
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
            if (chunk_size > 16) {
                in.seekg(chunk_size - 16, std::ios::cur);
            }
        } else if (std::strncmp(chunk_id, "data", 4) == 0) {
            data_chunk.resize(chunk_size);
            in.read(reinterpret_cast<char*>(data_chunk.data()), chunk_size);
        } else {
            in.seekg(chunk_size, std::ios::cur);
        }

        if (chunk_size & 1) {
            in.seekg(1, std::ios::cur);
        }
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
        for (size_t i = 0; i < sample_count; ++i) {
            pcm->push_back(std::clamp(src[i] / 32768.0f, -1.0f, 1.0f));
        }
    } else {
        return false;
    }

    *channels = fmt_channels;
    *sample_rate = fmt_rate;
    return true;
}

static ProjectIoResult make_result(bool ok, std::string message) {
    ProjectIoResult out;
    out.ok = ok;
    out.message = std::move(message);
    return out;
}

} // namespace

ProjectIoResult project_save(const std::filesystem::path& project_dir,
                             sp303::Device* dev,
                             sp303::Audio* audio,
                             const sp303::AudioConfig& cfg) {
    if (!dev || !audio) {
        return make_result(false, "save failed: device/audio unavailable");
    }

    const auto temp_dir = project_dir.string() + ".tmp";
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
    std::filesystem::create_directories(std::filesystem::path(temp_dir) / "samples", ec);
    if (ec) {
        return make_result(false, "save failed: could not create temp project directory");
    }

    json root;
    root["version"] = PROJECT_VERSION;
    root["audio_sample_rate"] = cfg.sample_rate;
    root["sample_level_threshold"] = sp303::get_sample_level_threshold(dev);
    root["active_effect_button"] = sp303::get_active_effect_btn(dev);
    root["pads"] = json::array();

    for (int slot = 0; slot < sp303::AUDIO_SLOTS; ++slot) {
        sp303::PadProjectState pad{};
        sp303::get_pad_project_state(dev, slot, &pad);

        json jpad;
        jpad["slot"] = slot;
        jpad["has_sample"] = pad.has_sample;
        jpad["loop"] = pad.loop_mode;
        jpad["gate"] = pad.gate_mode;
        jpad["reverse"] = pad.reverse_mode;
        jpad["has_effect"] = pad.has_effect;

        if (pad.has_sample && sp303::audio_has_sample(audio, slot)) {
            std::vector<float> pcm;
            uint32_t channels = 1;
            uint32_t frames = 0;
            if (!sp303::audio_export_sample(audio, slot, &pcm, &channels, &frames)) {
                return make_result(false, "save failed: could not export sample data");
            }

            const std::string stem = pad_file_stem(slot);
            const std::string wav_name = stem + ".wav";
            const auto wav_path = std::filesystem::path(temp_dir) / "samples" / wav_name;
            if (!write_wav_f32(wav_path, pcm, channels, cfg.sample_rate)) {
                return make_result(false, "save failed: could not write wav file");
            }

            jpad["sample_file"] = wav_name;
            jpad["start_127"] = sp303::audio_get_sample_start(audio, slot);
            jpad["end_127"] = sp303::audio_get_sample_end(audio, slot);
            jpad["level_127"] = sp303::audio_get_sample_level(audio, slot);
            jpad["bpm_adjust"] = sp303::audio_get_sample_bpm_adjust(audio, slot);
            jpad["time_mode"] = sp303::audio_get_sample_time_mode(audio, slot);
            jpad["time_target_bpm"] = sp303::audio_get_sample_time_target_bpm(audio, slot);
            jpad["channels"] = channels;
            jpad["frames"] = frames;
        }

        root["pads"].push_back(jpad);
    }

    std::ofstream manifest(std::filesystem::path(temp_dir) / "project.json");
    if (!manifest.is_open()) {
        return make_result(false, "save failed: could not write manifest");
    }
    manifest << root.dump(2);
    manifest.close();

    std::filesystem::remove_all(project_dir, ec);
    ec.clear();
    std::filesystem::rename(temp_dir, project_dir, ec);
    if (ec) {
        return make_result(false, "save failed: could not move temp project into place");
    }
    return make_result(true, "saved project to " + project_dir.string());
}

ProjectIoResult project_load(const std::filesystem::path& project_dir,
                             sp303::Device* dev,
                             sp303::Audio* audio,
                             const sp303::AudioConfig& cfg) {
    if (!dev || !audio) {
        return make_result(false, "load failed: device/audio unavailable");
    }

    const auto manifest_path = project_dir / "project.json";
    std::ifstream manifest(manifest_path);
    if (!manifest.is_open()) {
        return make_result(false, "load failed: project.json not found");
    }

    json root;
    try {
        root = json::parse(manifest);
    } catch (...) {
        return make_result(false, "load failed: invalid project.json");
    }

    if (root.value("version", 0) != PROJECT_VERSION) {
        return make_result(false, "load failed: unsupported project version");
    }
    if (!root.contains("pads") || !root["pads"].is_array() || root["pads"].size() != sp303::AUDIO_SLOTS) {
        return make_result(false, "load failed: invalid pad list");
    }

    std::vector<LoadedPad> loaded(sp303::AUDIO_SLOTS);
    uint32_t project_rate = root.value("audio_sample_rate", cfg.sample_rate);

    for (const auto& jpad : root["pads"]) {
        int slot = jpad.value("slot", -1);
        if (slot < 0 || slot >= sp303::AUDIO_SLOTS) {
            return make_result(false, "load failed: invalid pad slot");
        }

        auto& dst = loaded[slot];
        dst.state.has_sample = jpad.value("has_sample", false);
        dst.state.loop_mode = jpad.value("loop", false);
        dst.state.gate_mode = jpad.value("gate", false);
        dst.state.reverse_mode = jpad.value("reverse", false);
        dst.state.has_effect = jpad.value("has_effect", false);
        dst.start_127 = std::clamp(jpad.value("start_127", 0), 0, 127);
        dst.end_127 = std::clamp(jpad.value("end_127", 127), 0, 127);
        dst.level_127 = std::clamp(jpad.value("level_127", 127), 0, 127);
        dst.bpm_adjust = std::clamp(jpad.value("bpm_adjust", 0), -1, 1);
        dst.time_mode = std::clamp(jpad.value("time_mode", 0), 0, 2);
        dst.time_target_bpm = jpad.value("time_target_bpm", -1);

        if (!dst.state.has_sample) {
            continue;
        }

        const std::string wav_name = jpad.value("sample_file", "");
        if (wav_name.empty()) {
            return make_result(false, "load failed: missing sample file name");
        }

        uint32_t wav_rate = 0;
        if (!read_wav(project_dir / "samples" / wav_name, &dst.pcm, &dst.channels, &wav_rate)) {
            return make_result(false, "load failed: could not read sample wav");
        }
        dst.frames = (uint32_t)(dst.pcm.size() / std::max(1u, dst.channels));
        if (dst.frames == 0) {
            return make_result(false, "load failed: empty sample wav");
        }
        if (wav_rate != project_rate) {
            std::printf("[PROJECT] warning: sample %s rate=%u project_rate=%u\n",
                        wav_name.c_str(), wav_rate, project_rate);
        }
    }

    sp303::reset_project_state(dev);
    for (int slot = 0; slot < sp303::AUDIO_SLOTS; ++slot) {
        sp303::audio_clear_sample(audio, slot);
    }

    sp303::set_sample_level_threshold(dev, std::clamp(root.value("sample_level_threshold", 5), 0, 8));
    sp303::set_active_effect_btn(dev, root.value("active_effect_button", -1));

    for (int slot = 0; slot < sp303::AUDIO_SLOTS; ++slot) {
        const auto& src = loaded[slot];
        if (src.state.has_sample) {
            if (!sp303::audio_import_sample(audio, slot, src.pcm.data(), src.frames, src.channels)) {
                return make_result(false, "load failed: could not import sample");
            }
            sp303::audio_set_sample_start(audio, slot, src.start_127);
            sp303::audio_set_sample_end(audio, slot, src.end_127);
            sp303::audio_set_sample_level(audio, slot, src.level_127);
            sp303::audio_set_sample_bpm_adjust(audio, slot, src.bpm_adjust);
            sp303::audio_set_sample_time_mode(audio, slot, src.time_mode, src.time_target_bpm);
        }
        sp303::set_pad_project_state(dev, slot, src.state);
    }

    std::string message = "loaded project from " + project_dir.string();
    if (project_rate != cfg.sample_rate) {
        message += " (warning: project sample rate differs from current audio rate)";
    }
    return make_result(true, message);
}
