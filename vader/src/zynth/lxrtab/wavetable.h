/* UMLAUF: band-limited tables in place of the LXR02 firmware's (tools/umlauf_wavetables.py, generated). */
#ifndef WAVETABLES_H_
#define WAVETABLES_H_
#include <stdint.h>
#define TABLESIZE 4096
#define TABLES 128
extern const int16_t sine_table[TABLESIZE + 1];
/* one table per octave: MIDI notes 0, 12, 24, ... 120 */
extern const int16_t recTable[11][1024];
extern const int16_t triTable[11][1024];
extern const int16_t sawTable[11][1024];
#endif
