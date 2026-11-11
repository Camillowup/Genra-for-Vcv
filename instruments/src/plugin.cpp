// SPDX-License-Identifier: GPL-3.0-only
#include "plugin.hpp"

Plugin* pluginInstance;

void GenraKnob::load(const char* n) {
	setSvg(Svg::load(asset::plugin(pluginInstance, std::string("res/knob/") + n + ".svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, std::string("res/knob/") + n + "_bg.svg")));
}

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelKwasub);
	p->addModel(modelKwasubFx);
	p->addModel(modelGolana);
	p->addModel(modelGolanaFx);
	p->addModel(modelUmlauf);
	p->addModel(modelUmlaufFx);
	p->addModel(modelMonk);
	p->addModel(modelVoice);
	p->addModel(modelVoiceBend);
	p->addModel(modelAnimal);
}
