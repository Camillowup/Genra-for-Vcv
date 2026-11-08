#include "plugin.hpp"
#include "Engine.hpp"

Plugin* pluginInstance;

void GenraKnob::load(const char* n) {
	setSvg(Svg::load(asset::plugin(pluginInstance, std::string("res/knob/") + n + ".svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, std::string("res/knob/") + n + "_bg.svg")));
}

void init(Plugin* p) {
	pluginInstance = p;
	genra::initGenres(); // before any module loads: ABLAUF's stored slots need the genre list, even ahead of GENRA
	p->addModel(modelGenra);
	p->addModel(modelGenraGrid);
	p->addModel(modelAblauf);
	p->addModel(modelMix);
	p->addModel(modelGenraFx);
	p->addModel(modelStotter);
	p->addModel(modelSchlag);
	p->addModel(modelDrumX);
	p->addModel(modelGrund);
	p->addModel(modelGrundFx);
	p->addModel(modelGenraMono);
	p->addModel(modelKwasub);
	p->addModel(modelKwasubFx);
	p->addModel(modelFunke);
	p->addModel(modelUmlauf);
	p->addModel(modelUmlaufFx);
	p->addModel(modelMonk);
	p->addModel(modelVoice);
	p->addModel(modelAnimal);
	p->addModel(modelSchmelz);
}
