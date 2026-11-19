#pragma once
// MIDI-MAP for the controls we draw ourselves (2.41.2): a transparent ParamWidget over the field or button, with a
// param of its own (appended after the module's knobs). A click on it touches the param (MIDI-MAP learns it) and then
// goes on to the control below, as do the wheel, hover and double-click: the control works as before.
//   select: a knob goes through the list (0..1 over the entries); the entry is taken when the knob rests for 0.3 s.
//   press:  a value over half presses it (as a click), going back under releases it.
// Values the code sets (a patch, undo: the module's JSON) do nothing: the module bumps a counter in dataFromJson.
#include <rack.hpp>
#include <cmath>
#include <functional>

namespace genra_map {
using namespace rack;

inline void configSelect(engine::Module* m, int pid, const std::string& name) {
	m->configParam(pid, 0.f, 1.f, 0.f, name + " (for MIDI-MAP: a knob goes through the list)", " %", 0.f, 100.f);
	m->paramQuantities[pid]->randomizeEnabled = false;
	m->paramQuantities[pid]->resetEnabled = false;
}
inline void configPress(engine::Module* m, int pid, const std::string& name) {
	m->configButton(pid, name + " (for MIDI-MAP)");
	m->paramQuantities[pid]->randomizeEnabled = false;
}

struct Overlay : app::ParamWidget {
	widget::Widget* target = nullptr;  // the control below (same parent)
	// select
	std::function<int()> count, current;
	std::function<void(int)> choose;
	// press
	std::function<void()> press, release;
	// where it sits (parent coords) when that changes (chords of a display); none: the target's box
	std::function<bool(math::Rect&)> place;
	const uint32_t* code = nullptr;
	float seen = -1.f, lastV = 0.f;
	uint32_t seenCode = 0;
	double movedAt = 0.0;
	int want = -1;
	bool primed = false;

	void step() override {
		if (place) {
			math::Rect r;
			visible = place(r);
			if (visible)
				box = r;
		}
		ParamWidget::step();
		if (!module || paramId < 0 || paramId >= (int) module->params.size())
			return;
		float v = module->params[paramId].getValue();
		uint32_t c = code ? *code : 0;
		if (press || release) {
			if (primed && c == seenCode) {
				if (v >= 0.5f && lastV < 0.5f && press)
					press();
				if (v < 0.5f && lastV >= 0.5f && release)
					release();
			}
			primed = true;
			seenCode = c;
			lastV = v;
			return;
		}
		if (!count || !choose)
			return;
		if (c != seenCode || seen < 0.f) {
			seenCode = c;
			seen = v;
			want = -1;
			return;
		}
		double now = system::getTime();
		if (std::fabs(v - seen) > 1e-5f) { // count() only when it moved (it may look at the disk)
			seen = v;
			int n = count();
			if (n > 0) {
				want = clamp((int) std::round(v * (n - 1)), 0, n - 1);
				movedAt = now;
			}
		}
		if (want >= 0 && now - movedAt >= 0.3) {
			if (!current || current() != want)
				choose(want);
			want = -1;
		}
	}
	void draw(const DrawArgs& args) override { ParamWidget::draw(args); }
	// ---- everything else goes on to the control below (at the same place in it)
	Vec at(Vec p) { return target ? p.plus(box.pos).minus(target->box.pos) : p; }
	void onButton(const ButtonEvent& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS && (e.mods & RACK_MOD_MASK) == 0 && module)
			ParamWidget::onButton(e); // touch the param (MIDI-MAP learns it)
		if (target) {
			ButtonEvent f = e;
			f.pos = at(e.pos);
			target->onButton(f);
		}
		if (!target && e.button == GLFW_MOUSE_BUTTON_RIGHT)
			ParamWidget::onButton(e);
	}
	void onDoubleClick(const DoubleClickEvent& e) override {
		if (target)
			target->onDoubleClick(e);
		e.consume(this);
	}
	void onHover(const HoverEvent& e) override {
		if (target) {
			HoverEvent f = e;
			f.pos = at(e.pos);
			target->onHover(f);
		}
		e.consume(this);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (target) {
			HoverScrollEvent f = e;
			f.pos = at(e.pos);
			target->onHoverScroll(f);
		}
	}
	void onHoverKey(const HoverKeyEvent& e) override {
		if (target) {
			HoverKeyEvent f = e;
			f.pos = at(e.pos);
			target->onHoverKey(f);
		}
	}
	void onEnter(const EnterEvent& e) override {
		if (target)
			target->onEnter(e);
	}
	void onLeave(const LeaveEvent& e) override {
		if (target)
			target->onLeave(e);
	}
	void onDragStart(const DragStartEvent& e) override {
		if (target)
			target->onDragStart(e);
	}
	void onDragMove(const DragMoveEvent& e) override {
		if (target)
			target->onDragMove(e);
	}
	void onDragEnd(const DragEndEvent& e) override {
		if (target)
			target->onDragEnd(e);
	}
};

/** Over a field: a knob goes through its list. */
inline Overlay* select(widget::Widget* parent, widget::Widget* target, engine::Module* m, int pid, const uint32_t* code,
                       std::function<int()> count, std::function<int()> current, std::function<void(int)> choose) {
	Overlay* o = new Overlay;
	o->box = target->box;
	o->target = target;
	o->module = m;
	o->paramId = m ? pid : -1;
	o->code = code;
	o->count = count;
	o->current = current;
	o->choose = choose;
	parent->addChild(o);
	return o;
}
/** Over a button (or a part of a widget: give the rect in the parent's coordinates): a press presses it. */
inline Overlay* press(widget::Widget* parent, widget::Widget* target, engine::Module* m, int pid, const uint32_t* code,
                      std::function<void()> act, const math::Rect* r = nullptr) {
	Overlay* o = new Overlay;
	o->box = r ? *r : target->box;
	o->target = target;
	o->module = m;
	o->paramId = m ? pid : -1;
	o->code = code;
	o->press = act;
	parent->addChild(o);
	return o;
}

}  // namespace genra_map
