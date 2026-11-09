#pragma once
// A large momentary (or latching) pad: GENRA's and ABLAUF's transport. The panel has the unlit pad with its icon;
// lit (by its light), the pad fills with its colour and the icon turns dark.
#include <rack.hpp>

namespace genraui {
using namespace rack;

struct BigPad : app::ParamWidget {
	enum Icon { I_FILL, I_DICE, I_PLAY, I_STOP, I_TAP, I_SONG, I_LOOP };
	int icon = I_PLAY;
	NVGcolor col = nvgRGB(0xff, 0xff, 0xff);
	int lightId = -1;
	bool held = false;
	bool latch = false; // a switch (0/1): each click toggles it
	float sizeMm = 9.4f;
	BigPad() { box.size = mm2px(Vec(sizeMm, sizeMm)); }
	void setSize(float mm) {
		Vec c = box.getCenter();
		sizeMm = mm;
		box.size = mm2px(Vec(mm, mm));
		box.pos = c.minus(box.size.div(2.f));
	}
	void onDragStart(const DragStartEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		held = true;
		if (ParamQuantity* pq = getParamQuantity())
			pq->setValue(latch ? (pq->getValue() > 0.5f ? 0.f : 1.f) : 1.f);
	}
	void onDragEnd(const DragEndEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		held = false;
		if (!latch)
			if (ParamQuantity* pq = getParamQuantity())
				pq->setValue(0.f);
	}
	void drawIcon(NVGcontext* vg, NVGcolor c) {
		float cx = box.size.x / 2.f, cy = box.size.y / 2.f, u = mm2px(1.f) * sizeMm / 9.4f;
		nvgFillColor(vg, c);
		nvgStrokeColor(vg, c);
		nvgStrokeWidth(vg, 0.5f * u);
		switch (icon) {
			case I_PLAY:
				nvgBeginPath(vg);
				nvgMoveTo(vg, cx - 1.6f * u, cy - 2.2f * u);
				nvgLineTo(vg, cx + 2.2f * u, cy);
				nvgLineTo(vg, cx - 1.6f * u, cy + 2.2f * u);
				nvgClosePath(vg);
				nvgFill(vg);
				break;
			case I_STOP:
				nvgBeginPath(vg);
				nvgRoundedRect(vg, cx - 1.9f * u, cy - 1.9f * u, 3.8f * u, 3.8f * u, 0.4f * u);
				nvgFill(vg);
				break;
			case I_FILL:
				for (int k = 0; k < 4; k++) {
					nvgBeginPath(vg);
					nvgRoundedRect(vg, cx + (-2.6f + k * 1.4f) * u, cy + (1.6f - k * 1.1f) * u, 0.9f * u, (0.9f + k * 1.1f) * u, 0.2f * u);
					nvgFill(vg);
				}
				break;
			case I_TAP:
				nvgBeginPath(vg);
				nvgCircle(vg, cx, cy, 2.3f * u);
				nvgStroke(vg);
				nvgBeginPath(vg);
				nvgCircle(vg, cx, cy, 0.9f * u);
				nvgFill(vg);
				break;
			case I_SONG: {
				const float ww[3] = {4.6f, 3.2f, 4.0f};
				for (int k = 0; k < 3; k++) {
					nvgBeginPath(vg);
					nvgRoundedRect(vg, cx - 2.4f * u, cy + (-1.9f + k * 1.6f) * u, ww[k] * u, 0.9f * u, 0.3f * u);
					nvgFill(vg);
				}
				break;
			}
			case I_LOOP: { // a circular arrow
				nvgBeginPath(vg);
				nvgArc(vg, cx, cy, 2.1f * u, -1.1f, 2.f * M_PI - 1.9f, NVG_CW);
				nvgLineCap(vg, NVG_ROUND);
				nvgStroke(vg);
				nvgBeginPath(vg);
				nvgMoveTo(vg, cx + 0.2f * u, cy - 3.0f * u);
				nvgLineTo(vg, cx + 1.7f * u, cy - 1.8f * u);
				nvgLineTo(vg, cx + 0.1f * u, cy - 0.8f * u);
				nvgClosePath(vg);
				nvgFill(vg);
				break;
			}
			default: { // dice
				nvgBeginPath(vg);
				nvgRoundedRect(vg, cx - 2.4f * u, cy - 2.4f * u, 4.8f * u, 4.8f * u, 0.9f * u);
				nvgStroke(vg);
				const float d[5][2] = {{-1.1f, -1.1f}, {1.1f, 1.1f}, {0.f, 0.f}, {1.1f, -1.1f}, {-1.1f, 1.1f}};
				for (int k = 0; k < 5; k++) {
					nvgBeginPath(vg);
					nvgCircle(vg, cx + d[k][0] * u, cy + d[k][1] * u, 0.42f * u);
					nvgFill(vg);
				}
				break;
			}
		}
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			float b = lightId >= 0 ? module->lights[lightId].getBrightness() : 0.f;
			if (held)
				b = std::max(b, 0.6f);
			if (b > 0.02f) {
				NVGcontext* vg = args.vg;
				nvgBeginPath(vg);
				nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, mm2px(1.6f * sizeMm / 9.4f));
				nvgFillColor(vg, nvgTransRGBAf(col, clamp(b, 0.f, 1.f)));
				nvgFill(vg);
				drawIcon(vg, nvgTransRGBAf(nvgRGB(0x0d, 0x0e, 0x11), clamp(b * 1.5f, 0.f, 1.f)));
			}
		}
		ParamWidget::drawLayer(args, layer);
	}
};


} // namespace genraui
