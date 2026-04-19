#pragma once

#include "sp303.h"

#include <array>
#include <vector>

namespace sp303 {

enum MemoryCardBackupKind {
    MEMORY_CARD_BACKUP_EMPTY = 0,
    MEMORY_CARD_BACKUP_SAMPLES,
    MEMORY_CARD_BACKUP_PATTERNS,
};

struct MemoryCardSampleSlot {
    bool has_sample = false;
    PadProjectState state{};
};

struct MemoryCardPatternEvent {
    int tick = 0;
    int sample_pad = 0;
};

struct MemoryCardPatternSlot {
    PatternProjectSlot slot{};
    std::vector<MemoryCardPatternEvent> events;
};

struct MemoryCardBackupSlot {
    MemoryCardBackupKind kind = MEMORY_CARD_BACKUP_EMPTY;
    std::array<PadProjectState, 16> sample_states{};
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
