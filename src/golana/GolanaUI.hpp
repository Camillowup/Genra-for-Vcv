// SPDX-License-Identifier: GPL-3.0-only
// GOLANA and GOLANA FX: the dropdown field both panels use (the value's name; a click opens the list, the wheel steps).
#pragma once
#include "../plugin.hpp"
#include "../ui/GenraUI.hpp"

namespace golana_ui {

namespace ui = genra_ui;

struct Field : app::ParamWidget {
	std::vector<std::string> names;
	std::string hint;  // dim text at the right (the filter's "2 × SVF · 24 DB"), per value or one for all
	std::vector<std::string> hints;
	int value() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq ? clamp((int) std::round(pq->getValue()), 0, (int) names.size() - 1) : 0;
	}
	void set(int v) {
		engine::ParamQuantity* pq = getParamQuantity();
		if (!pq)
			return;
		float old = pq->getValue();
		if ((int) std::round(old) == v)
			return;
		pq->setValue((float) v);
		history::ParamChange* h = new history::ParamChange;
		h->name = "change " + pq->name;
		h->moduleId = module->id;
		h->paramId = paramId;
		h->oldValue = old;
		h->newValue = (float) v;
		APP->history->push(h);
	}
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			Menu* menu = createMenu();
			engine::ParamQuantity* pq = getParamQuantity();
			if (pq)
				menu->addChild(createMenuLabel(pq->name));
			for (int i = 0; i < (int) names.size(); i++)
				menu->addChild(createCheckMenuItem(names[i], i < (int) hints.size() ? hints[i] : "",
				                                   [=]() { return value() == i; }, [=]() { set(i); }));
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (module && e.scrollDelta.y != 0.f) {
			set(clamp(value() + (e.scrollDelta.y < 0.f ? 1 : -1), 0, (int) names.size() - 1));
			e.consume(this);
			return;
		}
		ParamWidget::onHoverScroll(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && !names.empty()) {
			int v = module ? value() : 0;
			auto f = ui::boldFont();
			bool big = box.size.y > mm2px(5.f);
			float y = box.size.y / 2 + (big ? 2.6f : 2.2f);
			ui::text(args.vg, f, mm2px(1.8f), y, names[v], big ? 7.4f : 6.3f, ui::TXT, NVG_ALIGN_LEFT, 0.9f);
			const std::string& h = v < (int) hints.size() ? hints[v] : hint;
			if (!h.empty())
				ui::text(args.vg, f, box.size.x - mm2px(4.8f), y, h, 5.4f, ui::DIM, NVG_ALIGN_RIGHT, 0.8f);
		}
		ParamWidget::drawLayer(args, layer);
	}
};

/** A field on the panel at r = {x, y, w, h} mm. */
template <class TW>
inline Field* addField(TW* w, Module* m, const float* r, int id, const std::vector<std::string>& names,
                       const std::vector<std::string>& hints = {}) {
	Field* f = createParam<Field>(mm2px(Vec(r[0], r[1])), m, id);
	f->box.size = mm2px(Vec(r[2], r[3]));
	f->names = names;
	f->hints = hints;
	w->addParam(f);
	return f;
}

inline std::vector<std::string> list(const char* const* a, int n) { return std::vector<std::string>(a, a + n); }

}  // namespace golana_ui
