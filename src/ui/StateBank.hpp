// SPDX-License-Identifier: GPL-3.0-only
// Presets for the instruments whose sound lives in an engine (K-WASUB's firmware, UMLAUF's VADER engine), not in the
// knobs alone: the same header field and menus as PresetBank.hpp (GRUND, FUNKE, SCHMELZ), but
//  - a factory preset is loaded by the engine thread (the module's request(i)); the module reports each load it
//    makes (from the list, its CV, a reset) with loadSerial / current, and the bank catches up in poll() (UI thread),
//    where a load asked for here becomes one undo step (the module before and after);
//  - a user preset is the module's whole saved state (knobs and engine), loaded the way Rack loads a preset of its
//    own (Engine::moduleFromJson, the engine held), the GENRA connection of the module kept.
// The patch keeps the preset's name ("presetName") and the knobs it had ("loaded", for the dot).
#pragma once
#include "GenraUI.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <functional>
#include <vector>

namespace genra_states {

namespace ui = genra_ui;

struct Factory {
	int count = 0;
	std::function<const char*(int)> style, name, info;
};

struct Bank {
	struct User {
		std::string name, file;
	};
	Module* m = nullptr;
	Factory f;
	std::string dir;
	std::vector<User> user;
	int preset = -1;  // −1: none; < f.count: factory; else user
	std::string presetName;
	std::vector<float> loaded;
	// the module's side (engine thread writes, UI reads)
	std::function<void(int)> request;  // load factory preset i (on the engine thread)
	std::atomic<int>* loadSerial = nullptr;
	std::atomic<int>* current = nullptr;  // the factory preset the engine loaded last (−1: none)
	int seen = 0;
	json_t* undoOld = nullptr;  // a load asked for here, waiting for the engine: the module before it
	int undoAt = 0;             // … and the serial it waits past

	void init(Module* module, const Factory& factory, const std::string& userDir, std::atomic<int>* serial,
	          std::atomic<int>* cur, std::function<void(int)> req) {
		m = module, f = factory, dir = userDir, loadSerial = serial, current = cur, request = req;
		loaded.assign(m->params.size(), 0.f);
		seen = serial->load();
		remember();
		scan();
	}
	~Bank() {
		if (undoOld)
			json_decref(undoOld);
	}
	int numParams() const { return (int) m->params.size(); }
	/** The module's JSON for an undo step or a preset: its engine state is the sound (the module's dataFromJson
	 * then lets the engine set the expander's knobs, not the other way round). */
	json_t* moduleJ() const {
		json_t* j = m->toJson();
		if (json_t* d = json_object_get(j, "data"))
			json_object_set_new(d, "engineWins", json_true());
		return j;
	}
	int count() const { return f.count + (int) user.size(); }
	std::string nameOf(int i) const {
		if (i < 0 || i >= count())
			return "INIT";
		return i < f.count ? f.name(i) : user[i - f.count].name;
	}
	std::string styleOf(int i) const { return i >= 0 && i < f.count ? f.style(i) : (i >= f.count ? "USER" : ""); }
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

	/** UI thread: catch up with the loads the engine made; a load asked for here becomes an undo step. */
	void poll() {
		if (!loadSerial)
			return;
		int s = loadSerial->load();
		if (s == seen)
			return;
		seen = s;
		int c = current->load();
		if (c >= 0 && c < f.count) {
			preset = c;
			presetName = f.name(c);
		}
		remember();
		if (undoOld && s - undoAt > 0) {
			history::ModuleChange* h = new history::ModuleChange;
			h->name = "load preset " + presetName;
			h->moduleId = m->id;
			h->oldModuleJ = undoOld;
			h->newModuleJ = moduleJ();
			APP->history->push(h);
			undoOld = nullptr;
		}
	}

	/** Loads preset i (factory: on the engine thread, user: at once), undoable. */
	void load(int i) {
		if (i < 0 || i >= count())
			return;
		poll();
		if (i < f.count) {
			if (undoOld)
				json_decref(undoOld);
			undoOld = moduleJ();
			undoAt = loadSerial->load();
			request(i);
			return;
		}
		const User& u = user[i - f.count];
		json_t* fileJ = json_load_file(u.file.c_str(), 0, nullptr);
		if (!fileJ)
			return;
		json_t* savedJ = json_object_get(fileJ, "module");
		if (!json_is_object(savedJ)) {
			json_decref(fileJ);
			return;
		}
		json_t* oldJ = moduleJ();
		json_t* newJ = json_deep_copy(oldJ);
		if (json_t* p = json_object_get(savedJ, "params"))
			json_object_set(newJ, "params", p);
		json_t* data = json_deep_copy(json_object_get(savedJ, "data"));
		if (!json_is_object(data)) {
			if (data)
				json_decref(data);
			data = json_object();
		}
		// the GENRA connection stays the module's
		json_t* oldData = json_object_get(oldJ, "data");
		if (json_t* fol = oldData ? json_object_get(oldData, "followGenra") : nullptr)
			json_object_set(data, "followGenra", fol);
		else
			json_object_del(data, "followGenra");
		json_object_set_new(data, "presetName", json_string(u.name.c_str()));
		json_object_set_new(data, "engineWins", json_true());
		json_object_set_new(newJ, "data", data);
		json_decref(fileJ);
		APP->engine->moduleFromJson(m, newJ);
		preset = i;
		presetName = u.name;
		remember();
		history::ModuleChange* h = new history::ModuleChange;
		h->name = "load preset " + presetName;
		h->moduleId = m->id;
		h->oldModuleJ = oldJ;
		h->newModuleJ = moduleJ();
		APP->history->push(h);
		json_decref(newJ);
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
				if (json_is_string(nameJ) && json_is_object(json_object_get(j, "module")))
					user.push_back({json_string_value(nameJ), file});
				json_decref(j);
			}
		}
		if (preset >= f.count || (preset < 0 && !presetName.empty())) {
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
		presetName = name;
		preset = f.count;  // found again by name in scan()
		remember();
		json_t* mj = m->toJson();
		json_t* saved = json_object();
		if (json_t* p = json_object_get(mj, "params"))
			json_object_set(saved, "params", p);
		if (json_t* d = json_object_get(mj, "data")) {
			json_t* data = json_deep_copy(d);
			json_object_del(data, "followGenra");
			json_object_set_new(saved, "data", data);
		}
		json_decref(mj);
		system::createDirectories(dir);
		json_t* j = json_object();
		json_object_set_new(j, "name", json_string(name.c_str()));
		json_object_set_new(j, "module", saved);
		json_dump_file(j, system::join(dir, file + ".json").c_str(), JSON_INDENT(2));
		json_decref(j);
		scan();
	}
	/** Deletes user preset k (an index into user). */
	void remove(int k) {
		if (k < 0 || k >= (int) user.size())
			return;
		bool here = preset == f.count + k;
		system::remove(user[k].file);
		if (here)
			preset = -1, presetName.clear();
		scan();
	}

	void toJson(json_t* j) const {
		json_object_set_new(j, "presetName", json_string(preset < 0 ? "" : presetName.c_str()));
		json_t* a = json_array();
		for (float v : loaded)
			json_array_append_new(a, json_real(v));
		json_object_set_new(j, "loaded", a);
	}
	/** After the knobs are set. fallback: the factory preset of a patch from before (−1: none). */
	void fromJson(json_t* j, int fallback) {
		json_t* pJ = json_object_get(j, "presetName");
		preset = -1;
		presetName.clear();
		if (json_is_string(pJ)) {
			presetName = json_string_value(pJ);
			for (int i = 0; i < count() && !presetName.empty(); i++)
				if (nameOf(i) == presetName) {
					preset = i;
					break;
				}
		} else if (fallback >= 0 && fallback < f.count) {
			preset = fallback;
			presetName = f.name(fallback);
		}
		remember();
		json_t* a = json_object_get(j, "loaded");
		int n = json_is_array(a) ? (int) json_array_size(a) : 0;
		for (int i = 0; i < n && i < numParams(); i++)
			loaded[i] = (float) json_number_value(json_array_get(a, i));
		if (loadSerial)
			seen = loadSerial->load();  // (the engine's own catch-up load after a patch is not a new preset)
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
		menu->addChild(createSubmenuItem(st, here ? "✔" : string::f("%d", j - i), [=](Menu* sub) {
			for (int k = i; k < j; k++)
				sub->addChild(createCheckMenuItem(string::f("%02d  %s", k + 1, f.name(k)), f.info ? f.info(k) : "",
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

/** The header field: number, name (a dot when a knob has moved), the style on the right. */
struct PresetField : OpaqueWidget {
	Bank* bank = nullptr;
	NVGcolor accent = ui::WHITE;
	std::string sample, sampleStyle;  // in the module browser
	void step() override {
		if (bank)
			bank->poll();
		OpaqueWidget::step();
	}
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
			std::string style = bank ? bank->styleOf(idx) : sampleStyle;
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
			if (!style.empty())
				ui::text(args.vg, font, box.size.x - mm2px(6.f), y - 0.3f, style, 5.2f, ui::DIM, NVG_ALIGN_RIGHT, 0.6f);
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
	b->poll();
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

}  // namespace genra_states
