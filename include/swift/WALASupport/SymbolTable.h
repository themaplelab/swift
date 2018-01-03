#ifndef SWIFT_SYMBOLTABLE_H
#define SWIFT_SYMBOLTABLE_H

#include <string>
#include <unordered_map>

using std::string;
using std::unordered_map;

namespace swift {

class SymbolTable {
public:
	bool has(void* key);
	string get(void* key);
	void insert(void* key, const string& name);
	void duplicate(void* key, const string& name);
private:
	unordered_map<void*, string> table;
};

} // end namespace swift

#endif
