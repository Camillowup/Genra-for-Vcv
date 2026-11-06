#include "plugin.hpp"

Plugin* pluginInstance;

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
}
