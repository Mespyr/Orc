#pragma once

#include "location.h"
#include "token.h"
#include "program.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

void print_error(std::string message);
void print_error_at_loc(Location loc, std::string message);
void print_note_at_loc(Location loc, std::string message);
void print_not_enough_arguments_error(Location loc, int expected, int actual, std::string name, std::string fullname = "", bool is_func = false);
void print_invalid_combination_of_types_error(Location loc, std::vector<LangType> types, std::string name, std::string fullname = "", bool is_func = false);
// expected and actual should be the string version of RambutanType 
void print_invalid_type_error(Location loc, std::string expected, std::string actual, std::string name, std::string fullname = "", bool is_func = false);
