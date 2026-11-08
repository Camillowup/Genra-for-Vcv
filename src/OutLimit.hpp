// SPDX-License-Identifier: GPL-3.0-only
// The instruments' output safety limiter: transparent up to ±6 V, then a soft knee towards a ±8 V ceiling, so a
// preset can be loud and a knob turned up does not clip hard. No state, no lookahead.
#pragma once
#include <cmath>

namespace genra_out {
static const float KNEE = 6.f, CEIL = 8.f;
inline float limit(float x) {
	float a = std::fabs(x);
	if (a <= KNEE)
		return x;
	float y = KNEE + (CEIL - KNEE) * std::tanh((a - KNEE) / (CEIL - KNEE));
	return x < 0.f ? -y : y;
}
}  // namespace genra_out
