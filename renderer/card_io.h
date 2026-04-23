#pragma once

#include "sp303.h"
#include "sp303_audio.h"

#include <filesystem>
#include <string>

struct CardIoResult {
    bool ok = false;
    std::string message;
};

CardIoResult card_save(const std::filesystem::path& card_dir,
                       sp303::Device* dev,
                       sp303::Audio* audio,
                       const sp303::AudioConfig& cfg);

CardIoResult card_load(const std::filesystem::path& card_dir,
                       sp303::Device* dev,
                       sp303::Audio* audio,
                       const sp303::AudioConfig& cfg);
