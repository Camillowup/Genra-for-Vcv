#pragma once
// MIDI export without a file dialog: every MIDI export of the plugin writes straight into one folder (default
// <Documents>/Midi Files from Genra). A save dialog can stall some audio drivers (ASIO on Windows) and with them
// Rack's engine, so the dialog is only used once, when the user picks another folder.
#include "../plugin.hpp"
#include <vector>

namespace midifolder {
/** The export folder (from the plugin's settings, else the default). */
std::string folder();
/** Writes data as <folder>/<YYYY-MM-DD>_<name>.mid (name cleaned; -2, -3 … instead of overwriting).
 *  Returns the file name written, or "" with err set. UI thread. */
std::string save(const std::string& name, const std::vector<uint8_t>& data, std::string* err = nullptr);
/** "MIDI export folder…" (pick another folder once) and "Open MIDI export folder". */
void appendMenu(ui::Menu* menu);
}  // namespace midifolder
