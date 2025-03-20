#ifndef MENUENTRY_H
#define MENUENTRY_H

#include <vector>
#include <string>
using std::string;

enum class category_t { none, shapes, utilities, lighting, settings };
struct menuEntry {
	menuEntry ( string l, category_t c ) : label( l ), category( c ) {}
	string label;
	category_t category;
};

struct menuContainer {
	menuContainer () {}
	std::vector< menuEntry > entries;
};

#endif
