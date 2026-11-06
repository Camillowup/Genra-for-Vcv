// SPDX-License-Identifier: GPL-3.0-only
// The GENRA look, shared by SCHMELZ, MIX and K-WASUB: colours, fonts, text and the lamp row for switch parameters
// (as src/jiant/JiantUI.hpp; these modules came from the vcv-synthis repo with their own copy).
#pragma once
#include "../plugin.hpp"

namespace genra_ui {

static const NVGcolor WHITE = nvgRGB(0xf2, 0xf3, 0xf7), TXT = nvgRGB(0xd9, 0xdb, 0xe2), DIM = nvgRGB(0x8a, 0x8d, 0x99),
                      OFF = nvgRGB(0x2a, 0x2b, 0x33), EDGE = nvgRGB(0x34, 0x36, 0x3f), LAMP = nvgRGB(0x3d, 0x3f, 0x48);

inline Vec mm(const float* p) { return mm2px(Vec(p[0], p[1])); }

inline std::shared_ptr<window::Font> boldFont() {
	return APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
}
inline std::shared_ptr<window::Font> monoFont() {
	return APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
}

/** Text at a baseline; align NVG_ALIGN_LEFT / CENTER / RIGHT. Returns the advance (px). */
inline float text(NVGcontext* vg, const std::shared_ptr<window::Font>& f, float x, float y, const std::string& s,
                  float size, NVGcolor c, int align = NVG_ALIGN_LEFT, float spacing = 0.4f) {
	if (!f || f->handle < 0 || s.empty())
		return 0.f;
	nvgFontFaceId(vg, f->handle);
	nvgFontSize(vg, size);
	nvgTextLetterSpacing(vg, spacing);
	nvgTextAlign(vg, align | NVG_ALIGN_BASELINE);
	nvgFillColor(vg, c);
	return nvgText(vg, x, y, s.c_str(), nullptr) - x;
}

/** The light of a lit button, in the module's colour. */
template <int R, int G, int B>
struct AccentLight : GrayModuleLightWidget {
	AccentLight() { addBaseColor(nvgRGB(R, G, B)); }
};

/** A row of lamps for a switch parameter: the one in use lit; a click picks one. The names are on the panel.
 * place(): first lamp at (x, y) mm, the next ones dx mm apart. */
struct LampRow : app::ParamWidget {
	NVGcolor accent = WHITE;
	int n = 3;
	float dx = 40.f;
	void place(const float* p, int count) {
		n = count;
		dx = mm2px(p[2]);
		Vec c = mm2px(Vec(p[0], p[1]));
		box.pos = Vec(c.x - dx / 2, c.y - 8);
		box.size = Vec(dx * n, 16);
	}
	int value() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq ? (int) std::round(pq->getValue()) : 0;
	}
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (engine::ParamQuantity* pq = getParamQuantity())
				pq->setValue((float) clamp((int) (e.pos.x / dx), 0, n - 1));
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void draw(const DrawArgs& args) override {
		int v = value();
		for (int i = 0; i < n; i++) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, dx * (i + 0.5f), box.size.y / 2, 3.6f);
			nvgFillColor(args.vg, i == v ? accent : LAMP);
			nvgFill(args.vg);
		}
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			float cx = dx * (value() + 0.5f), cy = box.size.y / 2;
			NVGcolor glow = accent;
			glow.a = 0.35f;
			NVGcolor none = accent;
			none.a = 0.f;
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, 8.f);
			nvgFillPaint(args.vg, nvgRadialGradient(args.vg, cx, cy, 2.f, 8.f, glow, none));
			nvgFill(args.vg);
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, 3.6f);
			nvgFillColor(args.vg, accent);
			nvgFill(args.vg);
		}
		ParamWidget::drawLayer(args, layer);
	}
};

}  // namespace genra_ui
