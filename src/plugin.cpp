#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelGenra);
	p->addModel(modelGenraGrid);
	p->addModel(modelVoice);
	p->addModel(modelDrumX);
	p->addModel(modelAnimal);
}
