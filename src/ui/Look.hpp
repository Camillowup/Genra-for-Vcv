// SPDX-License-Identifier: GPL-3.0-only
// The pieces of the instruments' look (docs/DESIGN.md › Aussehen: after GENRA MONO, GRUND, FUNKE and SCHMELZ) that a
// module whose sound lives in an engine needs on top of the panel: the preset field in the header, selection fields,
// texts that follow a mode. Every field: a click opens the list, the mouse wheel steps. (GENRA Instruments' src/ui
// forwards to this file; GENRA Test takes it from here too.)
#pragma once
#include "GenraUI.hpp"
#include <functional>

namespace genra_look {

namespace ui = genra_ui;

/** The preset field in the header: number and name; the list from the module (factory presets of an engine). */
struct PickField : OpaqueWidget {
	NVGcolor accent = ui::WHITE;
	std::function<int()> count = [] { return 1; };
	std::function<std::string(int)> name = [](int) { return std::string("INIT"); };
	std::function<std::string(int)> style;  // a group per style in the list (optional)
	std::function<int()> current = [] { return 0; };
	std::function<void(int)> choose = [](int) {};
	std::string title = "Preset";
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			Menu* menu = createMenu();
			menu->addChild(createMenuLabel(title));
			int n = count();
			auto fn = name;
			auto ch = choose;
			auto cur = current;
			if (style) {
				auto st = style;
				for (int i = 0; i < n;) {
					std::string s = st(i);
					int j = i;
					while (j < n && st(j) == s)
						j++;
					menu->addChild(createSubmenuItem(s, "", [=](Menu* sub) {
						for (int k = i; k < j; k++)
							sub->addChild(createCheckMenuItem(string::f("%02d  %s", k + 1, fn(k).c_str()), "",
							                                  [=]() { return cur() == k; }, [=]() { ch(k); }));
					}));
					i = j;
				}
			} else {
				for (int k = 0; k < n; k++)
					menu->addChild(createCheckMenuItem(string::f("%02d  %s", k + 1, fn(k).c_str()), "",
					                                  [=]() { return cur() == k; }, [=]() { ch(k); }));
			}
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (e.scrollDelta.y != 0.f) {
			int n = std::max(1, count());
			choose(((current() + (e.scrollDelta.y < 0.f ? 1 : -1)) % n + n) % n);
			e.consume(this);
			return;
		}
		OpaqueWidget::onHoverScroll(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			int i = current();
			float y = box.size.y / 2 + 2.6f;
			auto f = ui::boldFont();
			ui::text(args.vg, f, mm2px(2.f), y, string::f("%02d", i + 1), 6.4f, accent, NVG_ALIGN_LEFT, 0.8f);
			std::string s = name(i);
			// as much of the name as fits before the chevron
			float room = box.size.x - mm2px(6.6f) - mm2px(6.f);
			while (s.size() > 1) {
				float bounds[4];
				nvgFontFaceId(args.vg, f->handle);
				nvgFontSize(args.vg, 7.6f);
				nvgTextLetterSpacing(args.vg, 0.9f);
				if (nvgTextBounds(args.vg, 0, 0, s.c_str(), nullptr, bounds) <= room)
					break;
				s.pop_back();
			}
			ui::text(args.vg, f, mm2px(6.6f), y, s, 7.6f, ui::TXT, NVG_ALIGN_LEFT, 0.9f);
		}
		OpaqueWidget::drawLayer(args, layer);
	}
};

/** A selection field for a switch parameter: the name of the choice and a short info; undoable. */
struct ParamField : app::ParamWidget {
	std::function<std::string(int)> nameOf = [](int i) { return string::f("%d", i); };
	std::function<std::string(int)> infoOf;
	int count = 1;
	std::string title, undo;
	int value() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq ? clamp((int) std::round(pq->getValue() - pq->getMinValue()), 0, count - 1) : 0;
	}
	void set(int t) {
		engine::ParamQuantity* pq = getParamQuantity();
		if (!pq)
			return;
		float old = pq->getValue(), nv = pq->getMinValue() + t;
		if (old == nv)
			return;
		pq->setValue(nv);
		history::ParamChange* h = new history::ParamChange;
		h->name = undo;
		h->moduleId = module->id;
		h->paramId = paramId;
		h->oldValue = old;
		h->newValue = nv;
		APP->history->push(h);
	}
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			Menu* menu = createMenu();
			menu->addChild(createMenuLabel(title));
			for (int i = 0; i < count; i++)
				menu->addChild(createCheckMenuItem(nameOf(i), infoOf ? infoOf(i) : "", [=]() { return value() == i; },
				                                   [=]() { set(i); }));
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (module && e.scrollDelta.y != 0.f) {
			set(clamp(value() + (e.scrollDelta.y < 0.f ? 1 : -1), 0, count - 1));
			e.consume(this);
			return;
		}
		ParamWidget::onHoverScroll(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			int t = module ? value() : 0;
			auto f = ui::boldFont();
			float y = box.size.y / 2 + 2.6f;
			ui::text(args.vg, f, mm2px(2.f), y, nameOf(t), 7.6f, ui::TXT, NVG_ALIGN_LEFT, 0.9f);
			if (infoOf)
				ui::text(args.vg, f, box.size.x - mm2px(6.f), y - 0.3f, infoOf(t), 5.6f, ui::DIM, NVG_ALIGN_RIGHT, 0.7f);
		}
		ParamWidget::drawLayer(args, layer);
	}
};

/** A text the module draws on the panel (a knob's name that follows a mode, a section's title): centred in its box,
 * on the baseline at its bottom; in the panel's colours (knob names 1.7 mm, titles in the accent). */
struct PanelText : TransparentWidget {
	std::function<std::string()> text;
	NVGcolor color = ui::TXT;
	float size = 5.0f, spacing = 0.9f;
	void draw(const DrawArgs& args) override {
		if (text)
			ui::text(args.vg, ui::boldFont(), box.size.x / 2, box.size.y - 0.4f, text(), size, color, NVG_ALIGN_CENTER,
			         spacing);
	}
};

}  // namespace genra_look
