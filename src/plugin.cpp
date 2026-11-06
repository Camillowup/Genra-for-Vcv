#include "plugin.hpp"

Plugin* pluginInstance;

void GenraKnob::load(const char* n) {
	setSvg(Svg::load(asset::plugin(pluginInstance, std::string("res/knob/") + n + ".svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, std::string("res/knob/") + n + "_bg.svg")));
}

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelGenra);
	p->addModel(modelGenraGrid);
	p->addModel(modelVoice);
	p->addModel(modelDrumX);
	p->addModel(modelAnimal);
	p->addModel(modelMonk);
	p->addModel(modelGenraMono);
	p->addModel(modelSchmelz);
	p->addModel(modelFunke);
	p->addModel(modelMix);
	p->addModel(modelKwasub);
	p->addModel(modelKwasubFx);
	p->addModel(modelUmlauf);
	p->addModel(modelUmlaufFx);
	p->addModel(modelStotter);
	p->addModel(modelAblauf);
}
