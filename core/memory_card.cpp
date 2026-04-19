#include "memory_card.h"

namespace sp303 {

void memory_card_reset(MemoryCardState* card) {
    if (!card) return;
    *card = {};
    card->formatted = false;
    card->write_protected = false;
}

void memory_card_format(MemoryCardState* card) {
    if (!card) return;
    bool write_protected = card->write_protected;
    *card = {};
    card->formatted = true;
    card->write_protected = write_protected;
}

} // namespace sp303
