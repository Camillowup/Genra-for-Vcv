// SPDX-License-Identifier: GPL-3.0-only
// VOICE and its expander VOICE BEND (placed right of it): what each reads of the other, without cables.
#pragma once
#include <atomic>

namespace jiant_voice {

/** VOICE BEND's knobs as SPELL's chip takes them (sb.h sp_param_t), set by VOICE BEND, read by VOICE. */
struct BendSource {
	std::atomic<int> on{0};       // the bends act (BEND lit)
	std::atomic<int> clock{64};   // 0..127, 64 = 8 kHz
	std::atomic<int> bend{0};     // 0..127 corrupted coefficients
	std::atomic<int> stutter{0};  // 0..127
	std::atomic<int> crush{0};    // 0..5 bits off the DAC
};

/** What VOICE BEND shows of VOICE. */
struct SpellSink {
	std::atomic<int> spellOn{0};  // VOICE's LANG is SPELL
	std::atomic<int> bendSeen{0}; // VOICE found the expander on its right
};

}  // namespace jiant_voice
