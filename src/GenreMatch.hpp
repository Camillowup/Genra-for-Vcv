// SPDX-License-Identifier: GPL-3.0-only
// Which preset fits a genre (2.31): a preset's style ("HOUSE · GARAGE", "AFROBEAT · AFROBEATS · AMAPIANO · GQOM")
// against a genre's name. Plain C++ without Rack (tests, GENRA BOX, GENRA's NEW with presets).
//   2  the genre's whole name is one of the style's items ("LO-FI", "2-STEP", "WITCH HOUSE")
//   1  a word of the name (3 letters or more) is in the style ("Deep House" in "HOUSE · GARAGE")
//   0  nothing
// A genre takes the presets of its best score: whole names first, so Witch House gets its own presets and not the
// house ones, and Lo-Fi (no word of 3 letters) finds "LO-FI".
#pragma once
#include <string>
#include <vector>

namespace genrematch {

inline std::string upr(std::string s) {
	for (char& c : s)
		if (c >= 'a' && c <= 'z')
			c = (char) (c - 'a' + 'A');
	return s;
}
inline std::string trim(const std::string& s) {
	size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
/** The style's items: split at the middle dot (UTF-8 "\xc2\xb7") or a comma. */
inline std::vector<std::string> items(const std::string& style) {
	std::vector<std::string> out;
	std::string st = upr(style), cur;
	for (size_t i = 0; i < st.size(); i++) {
		if ((unsigned char) st[i] == 0xc2 && i + 1 < st.size() && (unsigned char) st[i + 1] == 0xb7) {
			out.push_back(trim(cur));
			cur.clear();
			i++;
			continue;
		}
		if (st[i] == ',') {
			out.push_back(trim(cur));
			cur.clear();
			continue;
		}
		cur += st[i];
	}
	out.push_back(trim(cur));
	return out;
}
/** The words of a text (letters and digits, 1 or more). */
inline std::vector<std::string> words(const std::string& s) {
	std::vector<std::string> out;
	std::string w;
	for (size_t i = 0; i <= s.size(); i++) {
		char c = i < s.size() ? s[i] : ' ';
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
			w += c;
			continue;
		}
		if (!w.empty())
			out.push_back(upr(w));
		w.clear();
	}
	return out;
}
/** The genres' names in capitals, for the word rule (GENRA's genre table sets them: initGenres; empty elsewhere). */
inline std::vector<std::string>& genreNames() {
	static std::vector<std::string> names;
	return names;
}
inline void setGenreNames(const std::vector<std::string>& v) {
	std::vector<std::string>& n = genreNames();
	n.clear();
	for (const std::string& x : v)
		n.push_back(upr(x));
}
inline int score(const char* style, const std::string& genreName) {
	if (!style || !*style)
		return 0;
	std::string g = upr(genreName);
	std::vector<std::string> its = items(style);
	for (const std::string& it : its)
		if (!it.empty() && it == g)
			return 2;
	// a word of the name, as a whole word of an item; an item that is the name of another genre of two words or more
	// ("WITCH HOUSE", "DRUM & BASS") is that genre's, not a word for the others
	std::vector<std::string> gw = words(g);
	for (const std::string& it : its) {
		std::vector<std::string> iw = words(it);
		if (iw.size() >= 2) {
			bool other = false;
			for (const std::string& n : genreNames())
				other |= n == it && n != g;
			if (other)
				continue;
		}
		for (const std::string& w : gw)
			if (w.size() >= 3)
				for (const std::string& x : iw)
					if (x == w)
						return 1;
	}
	return 0;
}
/** The candidates for a genre among n presets (style(i)): those of the best score; `skip` (the one loaded) only when it
 * is the only one. Empty: none fits. */
template <typename StyleOf>
std::vector<int> candidates(int n, StyleOf style, const std::string& genreName, int skip = -1) {
	std::vector<int> best;
	int top = 0;
	for (int i = 0; i < n; i++) {
		int s = score(style(i), genreName);
		if (s == 0)
			continue;
		if (s > top) {
			top = s;
			best.clear();
		}
		if (s == top)
			best.push_back(i);
	}
	if (best.size() > 1)
		for (size_t k = 0; k < best.size(); k++)
			if (best[k] == skip) {
				best.erase(best.begin() + k);
				break;
			}
	return best;
}

} // namespace genrematch
