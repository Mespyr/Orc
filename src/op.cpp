#include "include/op.h"

bool Op::is_prim_type_mode() {
	if (mode == MODE_8BIT || mode == MODE_64BIT)
		return true;
	return false;
}

bool is_builtin_word(std::string word) {
	static_assert(OP_COUNT == 57, "unhandled op types in is_builtin_word()");
	std::vector<std::string> builtin_words = {
		"dump",
		"+", "-", "*", "/",
		"=", ">", "<", ">=", "<=", "!=", "and", "or",
		"pop", "dup", "swp", "rot", "over",
		"call0", "call1", "call2", "call3", "call4", "call5", "call6",
		"fun", "const", "end", "struct", "import", "jmp", "cjmpt", "cjmpf", "jmpe", "cjmpet", "cjmpef", "delete"
	};
	for (std::string w : builtin_words)
		if (w == word) return true;
	return false;
};
