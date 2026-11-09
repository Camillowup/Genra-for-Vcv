// SPDX-License-Identifier: GPL-3.0-only
#include "GlCore.hpp"
#include <atomic>

#if defined(ARCH_WIN)
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
#else
	#include <dlfcn.h>
	#include <unistd.h>
#endif

#if defined(ARCH_WIN)
static const char* CORE_FILE = "glcore.dll";
#elif defined(ARCH_MAC)
static const char* CORE_FILE = "glcore.dylib";
#else
static const char* CORE_FILE = "glcore.so";
#endif

static std::atomic<uint32_t> copyCounter{0};

static std::string runDir() { return asset::user("Genra/run/glcore"); }

/** Copies left behind by a crash are removed once per session, before the first copy is made. */
static void cleanRunDir() {
	static bool cleaned = false;
	if (cleaned)
		return;
	cleaned = true;
	std::string dir = runDir();
	if (!system::isDirectory(dir))
		return;
	for (const std::string& f : system::getEntries(dir))
		system::remove(f);  // a copy still loaded by another Rack (Windows) refuses: it stays
}

bool GlCore::load() {
	unload();
	error.clear();
	std::string src = asset::plugin(pluginInstance, std::string("res/") + CORE_FILE);
	if (!system::isFile(src)) {
		error = "GOLANA core not found: " + src;
		return false;
	}
	cleanRunDir();
	std::string dir = runDir();
	system::createDirectories(dir);
#if defined(ARCH_WIN)
	uint32_t pid = (uint32_t) GetCurrentProcessId();
#else
	uint32_t pid = (uint32_t) getpid();
#endif
	copyPath = system::join(dir, string::f("glcore-%u-%u-%s", pid, (unsigned) copyCounter++, CORE_FILE));
	if (!system::copy(src, copyPath)) {
		error = "could not copy the GOLANA core to " + copyPath;
		copyPath.clear();
		return false;
	}
	void* sym = nullptr;
#if defined(ARCH_WIN)
	std::wstring w = string::UTF8toUTF16(copyPath);
	HMODULE h = LoadLibraryW(w.c_str());
	if (!h) {
		error = string::f("LoadLibrary failed (%lu)", (unsigned long) GetLastError());
		system::remove(copyPath);
		copyPath.clear();
		return false;
	}
	handle = (void*) h;
	sym = (void*) GetProcAddress(h, "gl_get");
#else
	handle = dlopen(copyPath.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (!handle) {
		const char* e = dlerror();
		error = std::string("dlopen failed: ") + (e ? e : "?");
		system::remove(copyPath);
		copyPath.clear();
		return false;
	}
	sym = dlsym(handle, "gl_get");
#endif
	const gl_api* a = sym ? ((gl_get_fn) sym)() : nullptr;
	if (!a || a->api != GL_API_VERSION) {
		error = "GOLANA core has the wrong interface";
		unload();
		return false;
	}
	api = a;
	return true;
}

void GlCore::unload() {
	api = nullptr;
	if (handle) {
#if defined(ARCH_WIN)
		FreeLibrary((HMODULE) handle);
#else
		dlclose(handle);
#endif
		handle = nullptr;
	}
	if (!copyPath.empty()) {
		system::remove(copyPath);
		copyPath.clear();
	}
}
