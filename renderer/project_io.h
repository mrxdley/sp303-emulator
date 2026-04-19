#pragma once

#include "sp303.h"
#include "sp303_audio.h"

#include <filesystem>
#include <string>

struct ProjectIoResult {
    bool ok = false;
    std::string message;
};

ProjectIoResult project_save(const std::filesystem::path& project_dir,
                             sp303::Device* dev,
                             sp303::Audio* audio,
                             const sp303::AudioConfig& cfg);

ProjectIoResult project_load(const std::filesystem::path& project_dir,
                             sp303::Device* dev,
                             sp303::Audio* audio,
                             const sp303::AudioConfig& cfg);
