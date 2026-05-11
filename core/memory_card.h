#pragma once

#include "sp303.h"

#include <array>
#include <vector>

namespace sp303 {

struct MemoryCardSampleSlot {
    bool has_sample = false;
    PadProjectState state{};
};

struct MemoryCardSampleData {
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

struct MemoryCardPatternEvent {
    int type = 0;
    int tick = 0;
    int sample_pad = 0;
    int velocity = 127;
};

struct MemoryCardPatternSlot {
    PatternProjectSlot slot{};
    std::vector<MemoryCardPatternEvent> events;
};

struct MemoryCardBackupSlot {
    MemoryCardBackupKind kind = MEMORY_CARD_BACKUP_EMPTY;
    std::array<PadProjectState, 16> sample_states{};
    std::array<MemoryCardSampleData, 16> sample_data{};
    std::array<PatternProjectSlot, 16> pattern_slots{};
    std::array<std::vector<MemoryCardPatternEvent>, 16> pattern_events{};
    int pattern_bpm = 120;
};

struct MemoryCardState {
    bool formatted = false;
    bool write_protected = false;

    std::array<MemoryCardSampleSlot, 16> live_sample_slots_cd{};
    std::array<MemoryCardPatternSlot, 16> live_pattern_slots_cd{};
    std::array<MemoryCardBackupSlot, 8> backup_slots{};
};

void memory_card_reset(MemoryCardState* card);
void memory_card_format(MemoryCardState* card);

} // namespace sp303
