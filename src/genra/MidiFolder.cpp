#include "MidiFolder.hpp"
#include <osdialog.h>
#include <ctime>
#include <cstdlib>
#if defined ARCH_WIN
#include <windows.h>
#endif

namespace midifolder {

static std::string settingsPath() { return asset::user("Genra/plugin-settings.json"); }

static std::string documents() {
#if defined ARCH_WIN
	// the Documents folder as the shell knows it (it may be moved, e.g. to OneDrive); %USERPROFILE%\Documents else
	typedef HRESULT(WINAPI * GetPathW)(HWND, int, HANDLE, DWORD, LPWSTR);
	if (HMODULE sh = LoadLibraryW(L"shell32.dll")) {
		GetPathW get = (GetPathW)(void*)GetProcAddress(sh, "SHGetFolderPathW");
		wchar_t buf[MAX_PATH] = {};
		bool ok = get && get(NULL, 0x0005 /* CSIDL_PERSONAL */, NULL, 0, buf) == 0 && buf[0];
		FreeLibrary(sh);
		if (ok)
			return string::UTF16toUTF8(buf);
	}
	if (const char* up = std::getenv("USERPROFILE"))
		return system::join(up, "Documents");
	return asset::user("");
#else
	if (const char* home = std::getenv("HOME"))
		return system::join(home, "Documents");
	return asset::user("");
#endif
}

static std::string chosen() {
	std::string dir;
	if (json_t* root = json_load_file(settingsPath().c_str(), 0, NULL)) {
		if (json_t* j = json_object_get(root, "midiExportFolder"))
			if (json_is_string(j))
				dir = json_string_value(j);
		json_decref(root);
	}
	return dir;
}

static void setChosen(const std::string& dir) {
	json_t* root = json_load_file(settingsPath().c_str(), 0, NULL);
	if (!root || !json_is_object(root)) {
		if (root)
			json_decref(root);
		root = json_object();
	}
	json_object_set_new(root, "midiExportFolder", json_string(dir.c_str()));
	system::createDirectories(system::getDirectory(settingsPath()));
	json_dump_file(root, settingsPath().c_str(), JSON_INDENT(2));
	json_decref(root);
}

std::string folder() {
	std::string dir = chosen();
	return dir.empty() ? system::join(documents(), "Midi Files from Genra") : dir;
}

std::string save(const std::string& name, const std::vector<uint8_t>& data, std::string* err) {
	std::string base;
	for (char c : name) {
		if (c == ' ' || c == '&' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
		    c == '>' || c == '|')
			c = '-';
		base += c;
	}
	if (system::getExtension(base) == ".mid")
		base = base.substr(0, base.size() - 4);
	std::time_t t = std::time(nullptr);
	char day[16] = "";
	if (std::tm* lt = std::localtime(&t))
		std::strftime(day, sizeof(day), "%Y-%m-%d", lt);
	std::string dir = folder();
	try {
		system::createDirectories(dir);
		std::string file = string::f("%s_%s.mid", day, base.c_str());
		for (int n = 2; system::exists(system::join(dir, file)) && n < 1000; n++)
			file = string::f("%s_%s-%d.mid", day, base.c_str(), n);
		system::writeFile(system::join(dir, file), data);
		return file;
	}
	catch (Exception& e) {
		if (err)
			*err = e.what();
	}
	return "";
}

void appendMenu(ui::Menu* menu) {
	menu->addChild(createMenuItem("MIDI export folder…", "", []() {
		std::string cur = folder();
		char* p = osdialog_file(OSDIALOG_OPEN_DIR, cur.c_str(), NULL, NULL);
		if (p) {
			setChosen(p);
			std::free(p);
		}
	}));
	menu->addChild(createMenuItem("Open MIDI export folder", "", []() {
		std::string dir = folder();
		system::createDirectories(dir);
		system::openDirectory(dir);
	}));
	menu->addChild(createMenuLabel("Saves to: " + folder()));
}

}  // namespace midifolder
