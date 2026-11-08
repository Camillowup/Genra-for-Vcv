// Diagnosis: does a fresh GENRA module advance its steps with the internal clock?
#include "GenraModule.hpp"
#include <cstdio>
Plugin* pluginInstance = nullptr;
static void run(Genra* g, double secs, const char* what) {
	Module::ProcessArgs a;
	a.sampleRate = 48000.f;
	a.sampleTime = 1.f / 48000.f;
	int last = -99, changes = 0;
	long n = (long)(48000 * secs);
	for (long i = 0; i < n; i++) {
		a.frame = i;
		g->process(a);
		if (g->step != last) {
			last = g->step;
			changes++;
		}
	}
	printf("%-28s running=%d step=%d stepChanges=%d bpm=%.1f\n", what, (int)g->running, g->step, changes,
	       g->params[Genra::BPM_PARAM].getValue());
	fflush(stdout);
}
int main() {
	Genra* g = new Genra;
	run(g, 2.0, "fresh");
	g->params[Genra::RUN_PARAM].setValue(1.f);
	run(g, 0.01, "run pressed");
	g->params[Genra::RUN_PARAM].setValue(0.f);
	run(g, 2.0, "after run toggle");
	g->params[Genra::RUN_PARAM].setValue(1.f);
	run(g, 0.01, "run pressed again");
	g->params[Genra::RUN_PARAM].setValue(0.f);
	run(g, 2.0, "after second toggle");
	json_t* j = g->dataToJson();
	Genra* h = new Genra;
	h->dataFromJson(j);
	run(h, 2.0, "after json reload");
	printf("done\n");
	return 0;
}
