#include "../include/type_checker.h"

bool TypeChecker::compare_type_stacks(std::vector<RambutanType> type_stack_1, std::vector<RambutanType> type_stack_2) {
	if (type_stack_1.size() != type_stack_2.size())
		return false;

	for (long unsigned int i = 0; i < type_stack_1.size(); i++)
		if (!types_equal(type_stack_1.at(i), type_stack_2.at(i)))
			return false;

	return true;
}
