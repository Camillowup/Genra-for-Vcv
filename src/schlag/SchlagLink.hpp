// SPDX-License-Identifier: GPL-3.0-only
// What other modules need to know of SCHLAG, without its insides (Schlag.cpp):
// GRID takes SCHLAG's MIDI input ("Auto"), GENRA FX takes SCHLAG's SEND A / SEND B without cables.
#pragma once
#include "../plugin.hpp"
#include <string>

extern Model* modelSchlag;

namespace schlag_link {

inline bool isSchlag(Module* m) { return m && m->model == modelSchlag; }

/** UI thread: the MIDI input set on the first SCHLAG in the patch that has one. False if there is none. */
bool midiSource(int& driver, int& device, int& channel, std::string& label);

/** Audio thread (GENRA FX): SCHLAG's SEND A (side 0) or SEND B (side 1) as of the last frame. False when SCHLAG keeps
 * it to itself (a cable is in its SEND jack, or "Sends to GENRA FX" is off). Marks the send as taken (for its panel). */
bool sendFrame(Module* m, int side, float& l, float& r, int64_t frame);

}  // namespace schlag_link
