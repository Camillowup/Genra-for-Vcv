// SPDX-License-Identifier: GPL-3.0-only
// Presets for FUNKE and SCHMELZ (the same as GRUND's, without the expander): the factory list by style, then the
// user presets as JSON files in a folder of the Rack user directory. The header field shows number and name (a dot
// when a knob has moved), a click opens the list, the wheel steps. Loading is undoable; the patch keeps the name.
#pragma once
#include "GenraUI.hpp"
#include <algorithm>
#include <cctype>
#include <functional>
#include <vector>

namespace genra_presets {

namespace ui = genra_ui;

/** The factory presets: how many, and per index style, name, a short info (model, registration) and the knobs. */
struct Factory {
	int count = 0;
	std::function<const char*(int)> style, name, info;
	std::function<void(int, float*)> values;
};

struct Bank {
	struct User {
		std::string name, file;
		std::vector<float> v;
	};
	Module* m = nullptr;
	Factory f;
	std::string dir;
	std::vector<User> user;
	int preset = -1;  // -1: none (INIT, or a patch from before the presets)
	std::string presetName;
	std::vector<float> loaded;

	void init(Module* module, const Factory& factory, const std::string& userDir) {
		m = module, f = factory, dir = userDir;
		loaded.assign(m->params.size(), 0.f);
		remember();
		scan();
	}
	int numParams() const { return (int) m->params.size(); }
	int count() const { return f.count + (int) user.size(); }
	std::string nameOf(int i) const {
		if (i < 0 || i >= count())
			return "INIT";
		return i < f.count ? f.name(i) : user[i - f.count].name;
	}
	void remember() {
		for (int i = 0; i < numParams(); i++)
			loaded[i] = m->params[i].getValue();
	}
	bool edited() const {
		if (preset < 0)
			return false;
		for (int i = 0; i < numParams(); i++)
			if (std::fabs(m->params[i].getValue() - loaded[i]) > 1e-4f)
				return true;
		return false;
	}
	/** Loads preset i, undoable. */
	void load(int i) {
		if (i < 0 || i >= count())
			return;
		json_t* oldJ = m->toJson();
		std::vector<float> v(numParams());
		for (int k = 0; k < numParams(); k++)
			v[k] = m->paramQuantities[k]->getDefaultValue();
		if (i < f.count)
			f.values(i, v.data());
		else
			for (int k = 0; k < numParams() && k < (int) user[i - f.count].v.size(); k++)
				v[k] = user[i - f.count].v[k];
		for (int k = 0; k < numParams(); k++)
			m->params[k].setValue(clamp(v[k], m->paramQuantities[k]->getMinValue(), m->paramQuantities[k]->getMaxValue()));
		preset = i;
		presetName = nameOf(i);
		remember();
		history::ModuleChange* h = new history::ModuleChange;
		h->name = "load preset " + presetName;
		h->moduleId = m->id;
		h->oldModuleJ = oldJ;
		h->newModuleJ = m->toJson();
		APP->history->push(h);
	}
	int step(int d) const {
		int n = count();
		return preset < 0 ? (d > 0 ? 0 : n - 1) : (preset + d + n) % n;
	}
	void scan() {
		user.clear();
		if (system::isDirectory(dir)) {
			std::vector<std::string> files = system::getEntries(dir);
			std::sort(files.begin(), files.end());
			for (const std::string& file : files) {
				if (system::getExtension(file) != ".json")
					continue;
				json_t* j = json_load_file(file.c_str(), 0, nullptr);
				if (!j)
					continue;
				json_t* nameJ = json_object_get(j, "name");
				json_t* valsJ = json_object_get(j, "params");
				int n = json_is_array(valsJ) ? (int) json_array_size(valsJ) : 0;
				if (json_is_string(nameJ) && n > 0) {
					User u;
					u.name = json_string_value(nameJ);
					u.file = file;
					for (int i = 0; i < numParams(); i++)
						u.v.push_back(i < n ? (float) json_number_value(json_array_get(valsJ, i))
						                    : m->paramQuantities[i]->getDefaultValue());
					user.push_back(u);
				}
				json_decref(j);
			}
		}
		if (preset >= f.count) {
			preset = -1;
			for (size_t i = 0; i < user.size(); i++)
				if (user[i].name == presetName)
					preset = f.count + (int) i;
		}
	}
	void save(std::string name) {
		name = string::uppercase(string::trim(name));
		if (name.empty())
			return;
		std::string file;
		for (char c : name)
			file += std::isalnum((unsigned char) c) ? (char) std::tolower((unsigned char) c) : '_';
		system::createDirectories(dir);
		json_t* j = json_object();
		json_object_set_new(j, "name", json_string(name.c_str()));
		json_t* a = json_array();
		for (int i = 0; i < numParams(); i++)
			json_array_append_new(a, json_real(m->params[i].getValue()));
		json_object_set_new(j, "params", a);
		json_dump_file(j, system::join(dir, file + ".json").c_str(), JSON_INDENT(2));
		json_decref(j);
		presetName = name;
		preset = f.count;  // found again by name in scan()
		scan();
		remember();
	}
	/** Deletes user preset k (an index into user). */
	void remove(int k) {
		if (k < 0 || k >= (int) user.size())
			return;
		bool current = preset == f.count + k;
		system::remove(user[k].file);
		if (current)
			preset = -1, presetName.clear();
		scan();
	}
	void reset() {
		preset = -1;
		presetName.clear();
		remember();
	}

	void toJson(json_t* j) const {
		json_object_set_new(j, "preset", json_string(preset < 0 ? "" : presetName.c_str()));
		json_t* a = json_array();
		for (float v : loaded)
			json_array_append_new(a, json_real(v));
		json_object_set_new(j, "loaded", a);
	}
	/** After the knobs are set: the preset by name ("--" when the patch has none), and the knobs it had. */
	void fromJson(json_t* j) {
		json_t* pJ = json_object_get(j, "preset");
		preset = -1;
		presetName = json_is_string(pJ) ? json_string_value(pJ) : "";
		for (int i = 0; i < count() && !presetName.empty(); i++)
			if (nameOf(i) == presetName) {
				preset = i;
				break;
			}
		remember();
		json_t* a = json_object_get(j, "loaded");
		int n = json_is_array(a) ? (int) json_array_size(a) : 0;
		for (int i = 0; i < n && i < numParams(); i++)
			loaded[i] = (float) json_number_value(json_array_get(a, i));
	}
};

/** The preset list: one submenu per style, the user presets at the end. */
inline void appendList(Menu* menu, Bank* b) {
	const Factory& f = b->f;
	for (int i = 0; i < f.count;) {
		std::string st = f.style(i);
		int j = i;
		while (j < f.count && st == f.style(j))
			j++;
		bool here = b->preset >= i && b->preset < j;
		menu->addChild(createSubmenuItem(st, here ? "✔" : "", [=](Menu* sub) {
			for (int k = i; k < j; k++)
				sub->addChild(createCheckMenuItem(string::f("%02d  %s", k + 1, f.name(k)), f.info(k),
				                                  [=]() { return b->preset == k; }, [=]() { b->load(k); }));
		}));
		i = j;
	}
	bool here = b->preset >= f.count;
	menu->addChild(createSubmenuItem("USER", here ? "✔" : string::f("%d", (int) b->user.size()), [=](Menu* sub) {
		if (b->user.empty())
			sub->addChild(createMenuLabel("Save one with right-click → Save preset as …"));
		for (int k = f.count; k < b->count(); k++)
			sub->addChild(createCheckMenuItem(b->nameOf(k), "", [=]() { return b->preset == k; }, [=]() { b->load(k); }));
	}));
}

/** The header field. */
struct PresetField : OpaqueWidget {
	Bank* bank = nullptr;
	NVGcolor accent = ui::WHITE;
	std::string sample;  // the name in the module browser
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && bank) {
			bank->scan();
			Menu* menu = createMenu();
			menu->addChild(createMenuLabel("Preset"));
			appendList(menu, bank);
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (bank && e.scrollDelta.y != 0.f) {
			bank->load(bank->step(e.scrollDelta.y < 0.f ? 1 : -1));
			e.consume(this);
			return;
		}
		OpaqueWidget::onHoverScroll(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			int idx = bank ? bank->preset : 0;
			int nf = bank ? bank->f.count : 1;
			std::string name = bank ? bank->nameOf(idx) : sample;
			std::string num = idx < 0 ? "--" : idx >= nf ? "U" : string::f("%02d", idx + 1);
			float y = box.size.y / 2 + 2.6f;
			auto font = ui::boldFont();
			ui::text(args.vg, font, mm2px(2.f), y, num, 6.4f, accent, NVG_ALIGN_LEFT, 0.8f);
			float w = ui::text(args.vg, font, mm2px(6.6f), y, name, 7.6f, ui::TXT, NVG_ALIGN_LEFT, 0.9f);
			if (bank && bank->edited()) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, mm2px(6.6f) + w + mm2px(1.4f), y - 2.6f, mm2px(0.55f));
				nvgFillColor(args.vg, accent);
				nvgFill(args.vg);
			}
		}
		OpaqueWidget::drawLayer(args, layer);
	}
};

/** The name field of "Save preset as …": Enter saves. */
struct SaveField : rack::ui::TextField {
	Bank* bank = nullptr;
	void onAction(const ActionEvent& e) override {
		if (bank)
			bank->save(text);
		if (rack::ui::MenuOverlay* o = getAncestorOfType<rack::ui::MenuOverlay>())
			o->requestDelete();
		e.consume(this);
	}
};

/** Into the module's context menu: the list, save, delete, the folder. */
inline void appendMenu(Menu* menu, Bank* b) {
	b->scan();
	menu->addChild(createSubmenuItem("Preset", b->nameOf(b->preset), [=](Menu* sub) { appendList(sub, b); }));
	menu->addChild(createSubmenuItem("Save preset as …", "", [=](Menu* sub) {
		SaveField* tf = new SaveField;
		tf->bank = b;
		tf->box.size.x = 160.f;
		tf->placeholder = "Name, Enter saves";
		tf->text = b->preset >= b->f.count ? b->presetName : "";
		sub->addChild(tf);
		APP->event->setSelectedWidget(tf);
	}));
	menu->addChild(createSubmenuItem("Delete user preset", b->user.empty() ? "none" : "", [=](Menu* sub) {
		if (b->user.empty())
			sub->addChild(createMenuLabel("No user presets"));
		for (int k = 0; k < (int) b->user.size(); k++)
			sub->addChild(createMenuItem(b->user[k].name, "", [=]() { b->remove(k); }));
	}));
	std::string dir = b->dir;
	menu->addChild(createMenuItem("Open user preset folder", "", [=]() {
		system::createDirectories(dir);
		system::openDirectory(dir);
	}));
}

}  // namespace genra_presets
