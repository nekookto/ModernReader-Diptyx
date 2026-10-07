#pragma once
#include <stdint.h>

// Two presses of the same button closer together than this (in ms) count as a double tap
#define DOUBLE_TAP_MS 350

// Per-button bookkeeping used to turn a quick second press into a "scroll a full page" action
struct DoubleTapTracker {
    uint32_t seenEdges = 0;  // number of physical presses that were already handled
    uint32_t anchorMs = 0;   // time of the press that started the pending single step
    bool armed = false;      // a single step was just made and a second tap may follow
    int origin = 0;          // selection index before that single step
    int dir = 0;             // -1 = up, +1 = down
    void reset() { armed = false; }
};
