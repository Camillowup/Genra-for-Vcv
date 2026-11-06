// GENRA – generative 4-track sequencer, VCV Rack module (v3)
#include "GenraModule.hpp"

std::string VariantQuantity::getDisplayValueString() {
	Genra* m = dynamic_cast<Genra*>(module);
	if (!m)
		return ParamQuantity::getDisplayValueString();
	int g = m->knobGenre();
	int v = (int)std::round(getValue());
	int n = numVariants(g);
	std::string name = prof(profileOf(g, v)).variant;
	if (v >= n)
		name += " (nur " + std::to_string(n) + ")";
	return name;
}

// ================================================================= lights
template <int R, int G, int B>
struct ColorLight : GrayModuleLightWidget {
	ColorLight() { addBaseColor(nvgRGB(R, G, B)); }
};
using OrangeLight = ColorLight<0x7b, 0x6c, 0xff>;
using PinkLight = ColorLight<0xb8, 0xf2, 0x3a>;
using CyanLight = ColorLight<0x3d, 0xc9, 0xff>;
using PurpleLight = ColorLight<0xe0, 0x77, 0x9e>;
using MintLight = ColorLight<0x5d, 0xff, 0x8a>;
using TealLight = ColorLight<0x4f, 0xd1, 0xbd>;

// ================================================================= shared drawing
static void drawText(NVGcontext* vg, std::shared_ptr<window::Font> f, float xmm, float ymm, float sizeMm, NVGcolor c, const std::string& s,
                     int align = NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, float spacing = 0.f) {
	nvgFontFaceId(vg, f->handle);
	nvgFontSize(vg, mm2px(sizeMm));
	nvgTextLetterSpacing(vg, mm2px(spacing));
	nvgFillColor(vg, c);
	nvgTextAlign(vg, align);
	nvgText(vg, mm2px(xmm), mm2px(ymm), s.c_str(), NULL);
}

static float textWidthMm(NVGcontext* vg, std::shared_ptr<window::Font> f, float sizeMm, float spacing, const std::string& s) {
	nvgFontFaceId(vg, f->handle);
	nvgFontSize(vg, mm2px(sizeMm));
	nvgTextLetterSpacing(vg, mm2px(spacing));
	float b[4];
	nvgTextBounds(vg, 0, 0, s.c_str(), NULL, b);
	return (b[2] - b[0]) / mm2px(1.f);
}

// ================================================================= genre field (one per track)
struct GenreField : Widget {
	Genra* module = nullptr;
	int track = 0;
	bool hover = false;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		std::shared_ptr<window::Font> bold = APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
		if (!bold)
			return;
		NVGcontext* vg = args.vg;
		float w = box.size.x / mm2px(1.f), h = box.size.y / mm2px(1.f);
		NVGcolor col = TRACK_COLORS[track];
		NVGcolor dim = nvgRGB(0x8a, 0x8d, 0x99);
		std::string label = "GLOBAL";
		bool own = false, pending = false;
		if (module) {
			Genra* m = module;
			pending = m->pendingTrack[track];
			if (m->trackHasCv(track)) {
				label = "CV  " + profileLabel(m->trackTarget(track));
				own = true;
			}
			else if (m->trackSel[track] >= 0) {
				label = profileLabel(m->trackSel[track]);
				own = true;
			}
		}
		if (pending)
			label += " …";
		if (hover) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, mm2px(0.9f));
			nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x10));
			nvgFill(vg);
		}
		// shrink long names to fit
		float avail = w - 5.f, size = 2.1f;
		while (size > 1.4f && textWidthMm(vg, bold, size, 0.15f, label) > avail)
			size -= 0.1f;
		NVGcolor tc = own ? col : dim;
		if (pending)
			tc.a = 0.7f;
		drawText(vg, bold, 1.4f, h / 2.f, size, tc, label, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, 0.15f);
		// arrow
		float ax = w - 2.4f, ay = h / 2.f;
		nvgBeginPath(vg);
		nvgMoveTo(vg, mm2px(ax - 1.f), mm2px(ay - 0.5f));
		nvgLineTo(vg, mm2px(ax + 1.f), mm2px(ay - 0.5f));
		nvgLineTo(vg, mm2px(ax), mm2px(ay + 0.7f));
		nvgClosePath(vg);
		nvgFillColor(vg, own ? col : dim);
		nvgFill(vg);
		Widget::drawLayer(args, layer);
	}

	void onHover(const HoverEvent& e) override { e.consume(this); } // needed to receive Enter / Leave
	void onEnter(const EnterEvent& e) override { hover = true; }
	void onLeave(const LeaveEvent& e) override { hover = false; }

	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || (e.button != GLFW_MOUSE_BUTTON_LEFT && e.button != GLFW_MOUSE_BUTTON_RIGHT))
			return;
		openMenu();
		e.consume(this);
	}

	void openMenu() {
		Genra* m = module;
		int t = track;
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel(std::string("Genre der Spur ") + TRACK_NAMES[t]));
		if (m->trackHasCv(t))
			menu->addChild(createMenuLabel("CV-Eingang verbunden: die Spannung wählt das Genre"));
		menu->addChild(createCheckMenuItem("Global (folgt GENRE)", "", [=]() { return m->trackSel[t] < 0; }, [=]() { m->setTrackSel(t, -1); }));
		menu->addChild(new ui::MenuSeparator);
		for (int f = 0; f < (int)(sizeof(FAMILIES) / sizeof(FAMILIES[0])); f++) {
			const Family& fam = FAMILIES[f];
			int sel = m->trackSel[t];
			int sg = sel >= 0 ? genreOfProfile(sel) : -1;
			bool inFam = sg >= fam.first && sg < fam.first + fam.count;
			menu->addChild(createSubmenuItem(fam.name, inFam ? "✔" : "", [=](ui::Menu* sub) {
				for (int g = fam.first; g < fam.first + fam.count; g++) {
					int nv = numVariants(g);
					std::string gname = prof(profileOf(g, 0)).name;
					if (nv == 1) {
						int p = profileOf(g, 0);
						sub->addChild(createCheckMenuItem(gname, "", [=]() { return m->trackSel[t] == p; }, [=]() { m->setTrackSel(t, p); }));
						continue;
					}
					int cur = m->trackSel[t];
					bool here = cur >= 0 && genreOfProfile(cur) == g;
					sub->addChild(createSubmenuItem(gname, here ? "✔" : "", [=](ui::Menu* sub2) {
						for (int v = 0; v < nv; v++) {
							int p = profileOf(g, v);
							sub2->addChild(createCheckMenuItem(prof(p).variant, "", [=]() { return m->trackSel[t] == p; }, [=]() { m->setTrackSel(t, p); }));
						}
					}));
				}
			}));
		}
	}
};

// ================================================================= display
// geometry (mm, relative to the display's top-left)
static const float CHORD_X = 78.f;
static const float CHORD_W = 136.f;
static const float CHORD_Y = 2.6f;
static const float CHORD_H = 6.f;

struct GenraDisplay : Widget {
	Genra* module = nullptr;
	int hoverChord = -1;

	int nChordsShown() { return module ? std::max<int>(1, module->pat.nProg) : 4; }
	float chordBoxW() { return std::min(18.f, CHORD_W / nChordsShown()); }

	int chordAtPos(Vec pos) {
		Vec mm = pos.div(mm2px(Vec(1.f, 1.f)));
		if (mm.y < CHORD_Y || mm.y > CHORD_Y + CHORD_H)
			return -1;
		float x = mm.x - CHORD_X;
		if (x < 0.f)
			return -1;
		int i = (int)(x / chordBoxW());
		return i < nChordsShown() ? i : -1;
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		std::shared_ptr<window::Font> bold = APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
		std::shared_ptr<window::Font> mono = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!bold || !mono)
			return;
		NVGcontext* vg = args.vg;
		NVGcolor white = nvgRGB(0xf2, 0xf3, 0xf7), dim = nvgRGB(0x8a, 0x8d, 0x99), dark = nvgRGB(0x1a, 0x1b, 0x21);
		float wmm = box.size.x / mm2px(1.f);

		std::string genre = "JUNGLE", variant = "RAGGA", info = "F MINOR  170 BPM  SWING 54%", bar = "BAR 1/4", status;
		std::vector<std::string> chords = {"Fm", "Db", "Fm", "Gb"};
		std::string laneName[4] = {"JUNGLE", "DUB", "LO-FI", "AMBIENT"};
		bool lanePending[4] = {false, false, false, false};
		int curChord = 0;
		bool lanes[4][16] = {};
		int curStep = -1;

		if (module) {
			Genra* m = module;
			Params P = m->buildParams();
			int L = m->lengthSteps();
			int g = m->curGenre;
			genre = upper(prof(g).name);
			variant = upper(prof(g).variant);
			if (m->pendingGenre)
				variant += " ...";
			float swing = effSwing(P, m->pat.trackGenre[T_DRUMS]);
			char buf[128];
			bool ext = m->inputs[Genra::CLOCK_INPUT].isConnected();
			float bpm = ext ? (m->stepPeriod > 0.f ? 60.f * APP->engine->getSampleRate() / (m->stepPeriod * 4.f) : 0.f)
			                : m->params[Genra::BPM_PARAM].getValue();
			std::string scaleName = upper(SCALES[P.scale].name);
			snprintf(buf, sizeof(buf), "%s %s  %s%.0f BPM  SWING %.0f%%", noteName(P.key + P.transp, P.key + P.transp, P.scale), scaleName.c_str(),
			         ext ? "EXT " : "", bpm, swing * 100.f);
			info = buf;
			int s = std::max(0, m->step);
			snprintf(buf, sizeof(buf), "BAR %d/%d", s / 16 + 1, L / 16);
			bar = buf;
			int sec = m->arrSecPub.load();
			if (m->arrAlive.load() > 0.f && sec >= 0 && sec < NUM_SECTIONS)
				bar += std::string("   ") + SECTION_NAMES[sec]; // the song part ABLAUF plays
			std::vector<std::string> st;
			if (!m->running)
				st.push_back("STOP");
			if (m->chordLocked())
				st.push_back("CHORDS LOCKED");
			if (P.transp != 0) {
				snprintf(buf, sizeof(buf), "TRANSP %+d", P.transp);
				st.push_back(buf);
			}
			if (P.extChord)
				st.push_back("EXT CHORD");
			for (int t = 0; t < 4; t++)
				if (m->pat.fixed[t]) {
					st.push_back("MIDI");
					break;
				}
			for (size_t i = 0; i < st.size(); i++)
				status += (i ? "  " : "") + st[i];

			chords.clear();
			float color = effColor(P, m->pat.chordGenre);
			for (int i = 0; i < m->pat.nProg; i++)
				chords.push_back(chordName(resolveSpec(m->pat.prog[i], P, color), P.key + P.transp, P.scale));
			if (P.extChord) {
				Chord c = chordAt(m->pat, P, s);
				chords.assign(1, chordName(c, P.key + P.transp, P.scale));
				curChord = 0;
			}
			else
				curChord = m->pat.ev[eventAt(m->pat, s)].spec;

			for (int t = 0; t < 4; t++) {
				laneName[t] = upper(prof(m->pat.trackGenre[t]).name);
				lanePending[t] = m->pendingTrack[t] || (m->pendingGenre && !m->ownGenre(t) && m->params[Genra::LOCK_PARAM + t].getValue() < 0.5f);
			}

			int barStart = (s / 16) * 16;
			curStep = m->step >= 0 ? s % 16 : -1;
			int prev[4];
			std::memcpy(prev, m->padPrev, sizeof(prev));
			bool hp = false;
			for (int i = 0; i < 16; i++) {
				int st2 = barStart + i;
				if (st2 >= L)
					break;
				Hit hits[NUM_DRUMS];
				drumStep(m->pat, P, st2, L, false, hits);
				for (int v = 0; v < NUM_DRUMS; v++)
					lanes[0][i] |= hits[v].on;
				lanes[1][i] = bassAt(m->pat, P, st2, L).on;
				lanes[2][i] = melAt(m->pat, P, st2, L).on;
				lanes[3][i] = padAt(m->pat, P, st2, L, prev, hp).on;
			}
		}

		// left: genre + info
		drawText(vg, bold, 4.f, 7.2f, 5.2f, white, genre, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.3f);
		float wmmG = textWidthMm(vg, bold, 5.2f, 0.3f, genre);
		drawText(vg, bold, 4.f + wmmG + 2.f, 7.2f, 2.6f, dim, std::string("· ") + variant, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.3f);
		drawText(vg, mono, 4.f, 12.6f, 2.7f, dim, info);

		// chords
		int n = (int)chords.size();
		float bw = std::min(18.f, CHORD_W / std::max(1, n));
		for (int i = 0; i < n; i++) {
			float x = CHORD_X + i * bw;
			bool active = i == curChord;
			bool hover = i == hoverChord;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, mm2px(x), mm2px(CHORD_Y), mm2px(bw - 1.2f), mm2px(CHORD_H), mm2px(0.8f));
			nvgFillColor(vg, active ? HARMONY_COLOR : (hover ? nvgRGB(0x2a, 0x2b, 0x33) : dark));
			nvgFill(vg);
			float fs = n > 5 ? 2.3f : 2.7f;
			drawText(vg, bold, x + (bw - 1.2f) / 2.f, CHORD_Y + CHORD_H / 2.f, fs, active ? nvgRGB(0x0a, 0x0a, 0x0d) : dim, chords[i],
			         NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		}
		drawText(vg, mono, CHORD_X, 13.f, 2.4f, dim, bar);
		drawText(vg, mono, CHORD_X + CHORD_W - 1.2f, 13.f, 2.4f, HARMONY_COLOR, status, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);

		// lanes with the genre each track plays
		float lx = wmm - 4.f - 16 * 2.3f;
		for (int t = 0; t < 4; t++) {
			float y = 2.8f + t * 3.3f;
			NVGcolor lc = TRACK_COLORS[t];
			if (lanePending[t])
				lc.a = 0.55f;
			drawText(vg, mono, lx - 1.6f, y + 2.0f, 2.1f, lc, laneName[t] + (lanePending[t] ? " …" : ""), NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
			for (int i = 0; i < 16; i++) {
				float x = lx + i * 2.3f;
				bool on = module ? lanes[t][i] : ((i * 7 + t * 5) % 11 < (5 - t));
				nvgBeginPath(vg);
				nvgRoundedRect(vg, mm2px(x), mm2px(y), mm2px(1.8f), mm2px(2.4f), mm2px(0.3f));
				nvgFillColor(vg, on ? TRACK_COLORS[t] : nvgRGB(0x22, 0x23, 0x2a));
				nvgFill(vg);
				if (i == curStep) {
					nvgBeginPath(vg);
					nvgRoundedRect(vg, mm2px(x - 0.25f), mm2px(y - 0.25f), mm2px(2.3f), mm2px(2.9f), mm2px(0.4f));
					nvgStrokeColor(vg, white);
					nvgStrokeWidth(vg, mm2px(0.25f));
					nvgStroke(vg);
				}
			}
		}
		Widget::drawLayer(args, layer);
	}

	void onHover(const HoverEvent& e) override {
		hoverChord = chordAtPos(e.pos);
		Widget::onHover(e);
	}
	void onLeave(const LeaveEvent& e) override {
		hoverChord = -1;
		Widget::onLeave(e);
	}

	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || module->extChord)
			return;
		int i = chordAtPos(e.pos);
		if (i < 0 || i >= module->pat.nProg)
			return;
		Genra* m = module;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			m->pushChordEdit(Genra::CE_NEXT, i);
			e.consume(this);
		}
		else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			openChordMenu(i);
			e.consume(this);
		}
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		if (!module || module->extChord)
			return;
		int i = chordAtPos(e.pos);
		if (i < 0 || i >= module->pat.nProg)
			return;
		module->pushChordEdit(Genra::CE_COLOR, i, e.scrollDelta.y > 0 ? 1 : -1);
		e.consume(this);
	}

	void openChordMenu(int i) {
		Genra* m = module;
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel(string::f("Akkord %d ändern", i + 1)));
		Params P = m->buildParams();
		float color = effColor(P, m->pat.chordGenre);
		int cur = currentAlternative(m->pat.prog[i], P);
		for (int a = 0; a < numAlternatives(); a++) {
			std::string name = alternativeName(a, P, color);
			std::string roman = a < 7 ? string::f("Stufe %d", a + 1) : "geborgt";
			menu->addChild(createCheckMenuItem(name, roman, [=]() { return a == cur; }, [=]() { m->pushChordEdit(Genra::CE_SET, i, a); }));
		}
		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuItem("Mehr Farbe (Mausrad hoch)", "", [=]() { m->pushChordEdit(Genra::CE_COLOR, i, 1); }));
		menu->addChild(createMenuItem("Weniger Farbe (Mausrad runter)", "", [=]() { m->pushChordEdit(Genra::CE_COLOR, i, -1); }));
		menu->addChild(createMenuItem("Farbe zurücksetzen", "", [=]() { m->pushChordEdit(Genra::CE_COLOR_RESET, i); }));
	}
};

// ================================================================= widget
struct GenraWidget : ModuleWidget {
	GenraWidget(Genra* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Genra.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		auto P = [](const float* p) { return mm2px(Vec(p[0], p[1])); };
		auto knob = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(P(p), module, id)); };
		auto in = [&](const float* p, int id) { addInput(createInputCentered<PJ301MPort>(P(p), module, id)); };
		auto out = [&](const float* p, int id) { addOutput(createOutputCentered<PJ301MPort>(P(p), module, id)); };

		GenraDisplay* disp = createWidget<GenraDisplay>(mm2px(Vec(layout::DISPLAY[0], layout::DISPLAY[1])));
		disp->box.size = mm2px(Vec(layout::DISPLAY[2], layout::DISPLAY[3]));
		disp->module = module;
		addChild(disp);

		// global row
		addParam(createParamCentered<GLargeKnob>(P(layout::GENRE), module, Genra::GENRE_PARAM));
		knob(layout::BPM, Genra::BPM_PARAM);
		knob(layout::KEY, Genra::KEY_PARAM);
		knob(layout::SCALE, Genra::SCALE_PARAM);
		knob(layout::VARIANT, Genra::VARIANT_PARAM);
		knob(layout::COMPLEX, Genra::COMPLEX_PARAM);
		knob(layout::COLOR, Genra::COLOR_PARAM);
		knob(layout::MUTATE, Genra::MUTATE_PARAM);
		knob(layout::SWING, Genra::SWING_PARAM);
		knob(layout::LENGTH, Genra::LENGTH_PARAM);
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(P(layout::NEW), module, Genra::NEW_PARAM, Genra::NEW_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<MintLight>>(P(layout::RUN), module, Genra::RUN_PARAM, Genra::RUN_LIGHT));
		// harmony
		addParam(createLightParamCentered<VCVLightBezel<TealLight>>(P(layout::CHORDNEW), module, Genra::CHORDNEW_PARAM, Genra::CHORDNEW_LIGHT));
		addParam(createLightParamCentered<VCVLightBezelLatch<TealLight>>(P(layout::CHORDLOCK), module, Genra::CHORDLOCK_PARAM, Genra::CHORDLOCK_LIGHT));
		in(layout::IN_CHORDNEW, Genra::CHORDNEW_INPUT);

		// inputs
		in(layout::IN_CLOCK, Genra::CLOCK_INPUT);
		in(layout::IN_RESET, Genra::RESET_INPUT);
		in(layout::IN_RUN, Genra::RUN_INPUT);
		in(layout::IN_NEW, Genra::NEW_INPUT);
		in(layout::IN_FILL, Genra::FILL_INPUT);
		in(layout::IN_MUTATE, Genra::MUTATE_INPUT);
		in(layout::IN_GENRE, Genra::GENRE_INPUT);
		in(layout::IN_VARIANT, Genra::VARIANT_INPUT);
		in(layout::IN_TRANSP, Genra::TRANSP_INPUT);
		in(layout::IN_COMPLEX, Genra::COMPLEX_INPUT);
		in(layout::IN_COLOR, Genra::COLOR_INPUT);
		in(layout::IN_CHORD, Genra::CHORD_INPUT);
		in(layout::IN_SWING, Genra::SWING_INPUT);
		in(layout::IN_BPM, Genra::BPM_INPUT);
		in(layout::IN_LENGTH, Genra::LENGTH_INPUT);
		out(layout::OUT_CLOCK, Genra::CLOCK_OUTPUT);
		out(layout::OUT_RESET, Genra::RESET_OUTPUT);
		out(layout::OUT_RUN, Genra::RUN_OUTPUT);
		out(layout::OUT_BAR, Genra::BAR_OUTPUT);
		out(layout::OUT_END, Genra::END_OUTPUT);
		out(layout::OUT_FILL, Genra::FILL_OUTPUT);
		out(layout::OUT_ROOT, Genra::ROOT_OUTPUT);
		out(layout::OUT_CHORD, Genra::CHORD_OUTPUT);
		out(layout::OUT_DRUMS, Genra::DRUMS_OUTPUT);


		// tracks
		const float* dens[4] = {layout::DENS_0, layout::DENS_1, layout::DENS_2, layout::DENS_3};
		const float* densIn[4] = {layout::IN_DENS_0, layout::IN_DENS_1, layout::IN_DENS_2, layout::IN_DENS_3};
		const float* field[4] = {layout::FIELD_0, layout::FIELD_1, layout::FIELD_2, layout::FIELD_3};
		const float* genreIn[4] = {layout::IN_GENRE_0, layout::IN_GENRE_1, layout::IN_GENRE_2, layout::IN_GENRE_3};
		const float* mute[4] = {layout::MUTE_0, layout::MUTE_1, layout::MUTE_2, layout::MUTE_3};
		const float* lock[4] = {layout::LOCK_0, layout::LOCK_1, layout::LOCK_2, layout::LOCK_3};
		const float* tnew[4] = {layout::NEW_0, layout::NEW_1, layout::NEW_2, layout::NEW_3};
		const float* tin[4] = {layout::IN_NEW_0, layout::IN_NEW_1, layout::IN_NEW_2, layout::IN_NEW_3};
		for (int t = 0; t < 4; t++) {
			addParam(createParamCentered<GKnob>(P(dens[t]), module, Genra::DENS_PARAM + t));
			in(densIn[t], Genra::DENS_INPUT + t);
			GenreField* gf = createWidget<GenreField>(mm2px(Vec(field[t][0], field[t][1])));
			gf->box.size = mm2px(Vec(field[t][2], field[t][3]));
			gf->module = module;
			gf->track = t;
			addChild(gf);
			in(genreIn[t], Genra::TGENRE_INPUT + t);
			addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(P(tnew[t]), module, Genra::TNEW_PARAM + t, Genra::TNEW_LIGHT + t));
			in(tin[t], Genra::TNEW_INPUT + t);
		}
		knob(layout::FILL, Genra::FILL_PARAM);
		knob(layout::GHOST, Genra::GHOST_PARAM);
		knob(layout::MOVE, Genra::MOVE_PARAM);
		knob(layout::RANGE, Genra::RANGE_PARAM);
		knob(layout::VOICING, Genra::VOICING_PARAM);
		const float* oct[3] = {layout::OCT_0, layout::OCT_1, layout::OCT_2};
		for (int i = 0; i < 3; i++)
			addParam(createParamCentered<GTrimpot>(P(oct[i]), module, Genra::OCT_PARAM + i));

		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(P(mute[0]), module, Genra::MUTE_PARAM + 0, Genra::MUTE_LIGHT + 0));
		addParam(createLightParamCentered<VCVLightBezelLatch<PinkLight>>(P(mute[1]), module, Genra::MUTE_PARAM + 1, Genra::MUTE_LIGHT + 1));
		addParam(createLightParamCentered<VCVLightBezelLatch<CyanLight>>(P(mute[2]), module, Genra::MUTE_PARAM + 2, Genra::MUTE_LIGHT + 2));
		addParam(createLightParamCentered<VCVLightBezelLatch<PurpleLight>>(P(mute[3]), module, Genra::MUTE_PARAM + 3, Genra::MUTE_LIGHT + 3));
		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(P(lock[0]), module, Genra::LOCK_PARAM + 0, Genra::LOCK_LIGHT + 0));
		addParam(createLightParamCentered<VCVLightBezelLatch<PinkLight>>(P(lock[1]), module, Genra::LOCK_PARAM + 1, Genra::LOCK_LIGHT + 1));
		addParam(createLightParamCentered<VCVLightBezelLatch<CyanLight>>(P(lock[2]), module, Genra::LOCK_PARAM + 2, Genra::LOCK_LIGHT + 2));
		addParam(createLightParamCentered<VCVLightBezelLatch<PurpleLight>>(P(lock[3]), module, Genra::LOCK_PARAM + 3, Genra::LOCK_LIGHT + 3));

		// outputs
		const float* drumOut[8] = {layout::DRUM_0, layout::DRUM_1, layout::DRUM_2, layout::DRUM_3,
		                           layout::DRUM_4, layout::DRUM_5, layout::DRUM_6, layout::DRUM_7};
		for (int v = 0; v < 8; v++)
			out(drumOut[v], Genra::DRUM_OUTPUT + v);
		out(layout::BASS_VOCT, Genra::BASS_VOCT_OUTPUT);
		out(layout::BASS_GATE, Genra::BASS_GATE_OUTPUT);
		out(layout::BASS_VEL, Genra::BASS_VEL_OUTPUT);
		out(layout::BASS_ACC, Genra::BASS_ACC_OUTPUT);
		out(layout::BASS_SLIDE, Genra::BASS_SLIDE_OUTPUT);
		out(layout::MEL_VOCT, Genra::MEL_VOCT_OUTPUT);
		out(layout::MEL_GATE, Genra::MEL_GATE_OUTPUT);
		out(layout::MEL_VEL, Genra::MEL_VEL_OUTPUT);
		out(layout::MEL_ACC, Genra::MEL_ACC_OUTPUT);
		out(layout::MEL_SLIDE, Genra::MEL_SLIDE_OUTPUT);
		out(layout::PAD_VOCT, Genra::PAD_VOCT_OUTPUT);
		out(layout::PAD_GATE, Genra::PAD_GATE_OUTPUT);
		out(layout::PAD_VEL, Genra::PAD_VEL_OUTPUT);
		out(layout::PAD_MOD, Genra::PAD_MOD_OUTPUT);
	}

	void appendContextMenu(Menu* menu) override {
		Genra* m = dynamic_cast<Genra*>(module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("GENRA"));
		menu->addChild(createMenuItem("Als MIDI-Datei exportieren…", "", [=]() { m->exportMidi(); }));
		menu->addChild(createMenuItem("MIDI-Datei importieren…", "", [=]() { m->importMidi(); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Akkord-Modus", {CHORD_MODES[0], CHORD_MODES[1], CHORD_MODES[2]},
		                                      [=]() { return (size_t)m->chordMode; },
		                                      [=](size_t v) { m->chordMode = (int)v; }));
		menu->addChild(createSubmenuItem("Akkord-Vorlage", "", [=](Menu* sub) {
			const Genre& G = prof(m->curGenre);
			sub->addChild(createCheckMenuItem("Zufällig", "", [=]() { return m->templateSel < 0; }, [=]() {
				m->templateSel = -1;
				m->chordRegenRequested = true;
			}));
			for (int t = 0; t < G.nTemplates; t++) {
				sub->addChild(createCheckMenuItem(G.templates[t], "", [=]() { return m->templateSel == t; }, [=]() {
					m->templateSel = t;
					m->chordRegenRequested = true;
				}));
			}
		}));
		menu->addChild(createMenuItem("Nur Akkorde neu würfeln", "", [=]() { m->chordRegenRequested = true; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Genre-CV-Eingänge", {GENRE_CV_MODES[0], GENRE_CV_MODES[1]},
		                                      [=]() { return (size_t)m->genreCvMode; },
		                                      [=](size_t v) { m->genreCvMode = (int)v; }));
		menu->addChild(createMenuItem("Alle Spuren auf Global (folgen GENRE)", "", [=]() { m->resetTracksRequested = true; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Clock-Eingang", {CLOCK_MODES[0], CLOCK_MODES[1], CLOCK_MODES[2], CLOCK_MODES[3]},
		                                      [=]() { return (size_t)m->clockMode; },
		                                      [=](size_t v) { m->clockMode = (int)v; }));
	}
};

Model* modelGenra = createModel<Genra, GenraWidget>("Genra");
