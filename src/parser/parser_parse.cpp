#include "../include/parser.h"

std::vector<Op> Parser::link_ops(std::vector<Op> ops, std::map<std::string, std::pair<int, int>> labels) {
	static_assert(OP_COUNT == 58, "unhandled op types in link_ops()");

	for (long unsigned int i = 0; i < ops.size(); i++) {
		Op current_op = ops.at(i);

		if (current_op.type == OP_JMP || current_op.type == OP_CJMPF || current_op.type == OP_CJMPT || current_op.type == OP_JMPE || current_op.type == OP_CJMPET || current_op.type == OP_CJMPEF) {
			if (labels.count(current_op.str_operand)) {
				if (current_op.type == OP_JMP || current_op.type == OP_CJMPF || current_op.type == OP_CJMPT)
					current_op.int_operand = labels.at(current_op.str_operand).first;
				else if (current_op.type == OP_JMPE || current_op.type == OP_CJMPET || current_op.type == OP_CJMPEF)
					current_op.int_operand = labels.at(current_op.str_operand).second;
				ops.at(i) = current_op;
			}
			else {
				print_error_at_loc(current_op.loc, "label '" + current_op.str_operand + "' not found");
				exit(1);
			}
		}
	}

	return ops;
}

void Parser::parse() {
	static_assert(OP_COUNT == 58, "unhandled op types in parse_tokens()");

	int function_addr = 0;
	std::vector<std::string> include_paths;

	while (i < tokens.size()) {
		Op current_op = convert_token_to_op(tokens.at(i));
		
		if (current_op.type == OP_FUN) {
			i++;
			// check if function name is in the token stream
			if (i >= tokens.size()) {
				print_error_at_loc(current_op.loc, "unexpected EOF found while parsing function definition");
				exit(1);
			}
		
			// check function name
			Token name_token = tokens.at(i);
			std::string function_name = name_token.value;

			if (!is_legal_name(name_token)) {
				print_error_at_loc(name_token.loc, "illegal name for function");
				exit(1);
			}
			else if (program.functions.count(function_name)) {
				print_error_at_loc(name_token.loc, "function '" + function_name + "' already exists");
				exit(1);
			}
			else if (program.structs.count(function_name)) {
				print_error_at_loc(name_token.loc, "struct '" + function_name + "' already exists");
				exit(1);
			}
			i++;

			// check if eof before argument parsing
			if (i >= tokens.size()) {
				print_error_at_loc(current_op.loc, "unexpected EOF found while parsing function definition");
				exit(1);
			}

			if (tokens.at(i).value != "(") {
				print_error_at_loc(tokens.at(i).loc, "unexpected '" + tokens.at(i).value + "' found while parsing function definition");
				exit(1);
			}
			i++;

			std::vector<RambutanType> arg_stack;
			std::vector<RambutanType> ret_stack;

			// parse arguments of function
			while (tokens.at(i).value != ")") {
				Token tok = tokens.at(i);
				std::string tok_base_value = parse_type_str(tok.value).first;
				// if not a primitive type or a struct
				if (!is_prim_type(tok_base_value) && !program.structs.count(tok_base_value)) {
					print_error_at_loc(tok.loc, "unknown argument type '" + tok.value + "'");
					exit(1);
				}
				arg_stack.push_back(RambutanType(tok.loc, tok.value));

				i++;
				if (i > tokens.size() - 1) {
					print_error_at_loc(tok.loc, "unexpected EOF found while parsing function definition");
					exit(1);
				}
			}

			// check if there is code after function declaration
			i++;
			if (i >= tokens.size()) {
				print_error_at_loc(tokens.back().loc, "unexpected EOF found while parsing function arguments");
				exit(1);
			}
			
			if (tokens.at(i).value == "[") {
				i++;
				// parse return stack of function
				while (tokens.at(i).value != "]") {
					Token tok = tokens.at(i);
					std::string tok_base_value = parse_type_str(tok.value).first;
					// if isn't a primitive type or struct
					if (!is_prim_type(tok_base_value) && !program.structs.count(tok_base_value)) {
						print_error_at_loc(tok.loc, "unknown return type '" + tok.value + "'");
						exit(1);
					}
					ret_stack.push_back(RambutanType(tok.loc, tok.value));

					i++;
					if (i > tokens.size() - 1) {
						print_error_at_loc(tok.loc, "unexpected EOF found while parsing function return stack");
						exit(1);
					}
				}
				i++;
			}

			program.functions.insert({function_name, Function(
				name_token.loc, function_addr, arg_stack, ret_stack
			)});
			function_addr++;

			std::vector<Op> function_ops;
			std::map<std::string, std::pair<int, int>> labels;
			std::map<std::string, std::pair<RambutanType, int>> var_offsets;
			int offset = 0;
			bool found_function_end = false;
			// recursion
			int recursion_level = 0;
			std::vector<std::string> recursion_stack;
		
			// parse tokens in function
			while (i < tokens.size()) {
				Op f_op = convert_token_to_op(tokens.at(i), var_offsets);
				
				if (f_op.type == OP_FUN) {
					print_error_at_loc(f_op.loc, "unexpected 'fun' keyword found inside function definition");
					exit(1);
				}
				else if (f_op.type == OP_STRUCT) {
					print_error_at_loc(f_op.loc, "unexpected 'struct' keyword found inside function definition");
					exit(1);
				}
				else if (f_op.type == OP_END) {
					if (recursion_level == 0) {
						found_function_end = true;
						break;
					}
					else {
						recursion_level--;
						std::string ended_label = recursion_stack.back();
						recursion_stack.pop_back();

						// set second idx to end of label	
						std::pair<int, int> label_pair = labels.at(ended_label);
						label_pair.second = i;
						labels.at(ended_label) = label_pair;

						f_op.type = OP_LABEL_END;
						f_op.int_operand = label_pair.second;
						f_op.str_operand = ended_label;
						function_ops.push_back(f_op);
					}
				}
				else if (f_op.type == OP_LABEL) {
					if (labels.count(f_op.str_operand)) {
						print_error_at_loc(f_op.loc, "redefinition of label '" + f_op.str_operand + "'");
						exit(1);
					}
					std::string label_name = f_op.str_operand;
					std::pair<int, int> start_and_end_idxs;
					start_and_end_idxs.first = i;
					labels.insert({f_op.str_operand, start_and_end_idxs});

					// increase recursion
					recursion_level++;
					recursion_stack.push_back(f_op.str_operand);

					f_op.int_operand = i;
					function_ops.push_back(f_op);
				}
				else if (f_op.type == OP_DEFINE_VAR) {
					i++;
					if (i >= tokens.size()) break;
					Token var_name_tok = tokens.at(i);
					if (!is_legal_name(var_name_tok)) {
						print_error_at_loc(var_name_tok.loc, "illegal name for variable");
						exit(1);
					}
					else if (program.functions.count(var_name_tok.value)) {
						print_error_at_loc(var_name_tok.loc, "name '" + var_name_tok.value + "' already exists as a function");
						exit(1);
					}
					else if (program.structs.count(var_name_tok.value)) {
						print_error_at_loc(var_name_tok.loc, "name '" + var_name_tok.value + "' already exists as a struct");
						exit(1);
					}
					else if (var_offsets.count(var_name_tok.value)) {
						print_error_at_loc(var_name_tok.loc, "variable '" + var_name_tok.value + "' already exists");
						exit(1);
					}

					var_offsets.insert({var_name_tok.value,
						{RambutanType(var_name_tok.loc, f_op.str_operand), offset}
					});

					offset += sizeof_type(f_op.str_operand, program.structs);
				}
				else if (f_op.type == OP_SET) {
					// if setting a variable
					if (var_offsets.count(f_op.str_operand)) {
						static_assert(MODE_COUNT == 3, "unhandled OpCodeModes in parse_tokens()");
						RambutanType type = var_offsets.at(f_op.str_operand).first;
						if (is_prim_type(type.base_type) || is_pointer(type)) {
							// 8bit values
							if (sizeof_type(type) == 1)
								f_op.mode = MODE_8BIT;
							// 64bit values
							else if (sizeof_type(type) == 8)
								f_op.mode = MODE_64BIT;
						}
						else f_op.mode = MODE_STRUCT;
						f_op.type = OP_SET_VAR;
						f_op.int_operand = var_offsets.at(f_op.str_operand).second; // offset to location of variable
						f_op.int_operand_2 = sizeof_type(type, program.structs); // size of type (amount of data needed to move it around)
						function_ops.push_back(f_op);
					}
					else if (program.structs.count(f_op.str_operand) || is_prim_type(f_op.str_operand) || is_pointer(f_op.str_operand)) {
						static_assert(MODE_COUNT == 3, "unhandled OpCodeModes in parse_tokens()");
						// if setting value of pointer
						if (is_pointer(f_op.str_operand)) {
							std::pair<std::string, int> type_pair = parse_type_str(f_op.str_operand);
							if (program.structs.count(type_pair.first) || is_prim_type(type_pair.first))
								f_op.mode = MODE_64BIT;
							else {
								print_error_at_loc(f_op.loc, "unknown type or variable name '" + f_op.str_operand + "'");
								exit(1);
							}
						}
						// if setting primitive type
						else if (is_prim_type(f_op.str_operand)) {
							// 8bit values
							if (sizeof_type(f_op.str_operand) == 1)
								f_op.mode = MODE_8BIT;
							// 64bit values
							else if (sizeof_type(f_op.str_operand) == 8)
								f_op.mode = MODE_64BIT;
						}
						else f_op.mode = MODE_STRUCT;
						f_op.type = OP_SET_PTR;
						// since setting variable relatively via pointers, there is no global offset to provide.
						f_op.int_operand = sizeof_type(f_op.str_operand, program.structs); // size of type (amount of data needed to move around)
						function_ops.push_back(f_op);
					}
					// if setting value of struct member
					else {
						static_assert(MODE_COUNT == 3, "unhandled OpCodeModes in parse_tokens()");
						std::vector<std::string> split_cmd = split_by_dot(f_op.str_operand);

						// if there is only one value in the command ie: String, meaning that a variable String doesn't exist and a type String doesnt exist
						// because we check if it is one of those in the previous if-statements
						if (split_cmd.size() == 1) {
							print_error_at_loc(f_op.loc, "unknown type or variable name '" + split_cmd.front() + "'");
							exit(1);
						}
						
						// if setting pointer member
						if (program.structs.count(split_cmd.front())) {
							std::pair<RambutanType, int> member_type_offset = struct_member_offset(f_op, program.structs);
							f_op.type = OP_SET_PTR_STRUCT_MEMBER;
							if (member_type_offset.first.ptr_to_trace > 0) // if it is a pointer, whether to a primitive or a struct
								f_op.mode = MODE_64BIT;
							else if (program.structs.count(member_type_offset.first.base_type))
								f_op.mode = MODE_STRUCT;
							// 8bit values
							else if (sizeof_type(member_type_offset.first, program.structs) == 1)
								f_op.mode = MODE_8BIT;
							// 64bit values
							else if (sizeof_type(member_type_offset.first, program.structs) == 8)
								f_op.mode = MODE_64BIT;

							f_op.int_operand = member_type_offset.second; // relative offset of member in pointer
							f_op.int_operand_2 = sizeof_type(member_type_offset.first, program.structs); // size of member
							function_ops.push_back(f_op);
						}
						// if setting variable member
						else
						{
							std::pair<RambutanType, int> member_type_offset = variable_member_offset(f_op, var_offsets, program.structs);
							if (member_type_offset.first.ptr_to_trace > 0) // if it is a pointer, whether to a primitive or a struct
								f_op.mode = MODE_64BIT;
							else if (program.structs.count(member_type_offset.first.base_type))
								f_op.mode = MODE_STRUCT;
							// 8bit values
							else if (sizeof_type(member_type_offset.first, program.structs) == 1)
								f_op.mode = MODE_8BIT;
							// 64bit values
							else if (sizeof_type(member_type_offset.first, program.structs) == 8)
								f_op.mode = MODE_64BIT;
							f_op.type = OP_SET_VAR_STRUCT_MEMBER;
							f_op.int_operand = member_type_offset.second; // offset to where member is located
							f_op.int_operand_2 = sizeof_type(member_type_offset.first, program.structs); // size of member
							function_ops.push_back(f_op);
						}
					}
				}
				else if (f_op.type == OP_READ) {
					// if reading a variable
					if (var_offsets.count(f_op.str_operand)) {
						RambutanType type = var_offsets.at(f_op.str_operand).first;
						if (!is_prim_type(type) && !is_pointer(type)) {
							print_error_at_loc(f_op.loc, "Can't read whole variable for a non-primitive type (type " + human_readable_type(type) + ")");
							exit(1);
						}

						static_assert(MODE_COUNT == 3, "unhandled OpCodeModes in parse_tokens()");
						// 8bit values
						if (sizeof_type(type) == 1)
							f_op.mode = MODE_8BIT;
						// 64bit values
						else if (sizeof_type(type) == 8)
							f_op.mode = MODE_64BIT;

						f_op.type = OP_READ_VAR;
						f_op.int_operand = var_offsets.at(f_op.str_operand).second;
						function_ops.push_back(f_op);
					}
					// if reading value of pointer
					else if (program.structs.count(f_op.str_operand) || is_prim_type(f_op.str_operand)) {
						if (!is_prim_type(f_op.str_operand) && !is_pointer(f_op.str_operand)) {
							print_error_at_loc(f_op.loc, "Can't read and push struct onto the stack from a pointer to it (type " + f_op.str_operand + ")");
							exit(1);
						}

						static_assert(MODE_COUNT == 3, "unhandled OpCodeModes in parse_tokens()");
						// 8bit values
						if (sizeof_type(f_op.str_operand) == 1)
							f_op.mode = MODE_8BIT;
						// 64bit values
						else if (sizeof_type(f_op.str_operand) == 8)
							f_op.mode = MODE_64BIT;

						f_op.type = OP_READ_PTR;
						f_op.int_operand = sizeof_type(f_op.str_operand, program.structs);
						function_ops.push_back(f_op);
					}
					// if reading value of struct member
					else {
						static_assert(PRIM_TYPES_COUNT == 2, "unhandled prim types in parse_tokens()");
						std::vector<std::string> split_cmd = split_by_dot(f_op.str_operand);
						
						// if reading pointer member
						if (program.structs.count(split_cmd.front())) {
							std::pair<RambutanType, int> member_type_offset = struct_member_offset(f_op, program.structs);
							f_op.type = OP_READ_PTR_STRUCT_MEMBER;

							if (member_type_offset.first.ptr_to_trace > 0) // if it is a pointer, whether to a primitive or a struct
								f_op.mode = MODE_64BIT;
							else if (program.structs.count(member_type_offset.first.base_type))
								f_op.mode = MODE_STRUCT;
							// 8bit values
							else if (sizeof_type(member_type_offset.first, program.structs) == 1)
								f_op.mode = MODE_8BIT;
							// 64bit values
							else if (sizeof_type(member_type_offset.first, program.structs) == 8)
								f_op.mode = MODE_64BIT;

							f_op.int_operand = member_type_offset.second; // relative offset of member in pointer
							f_op.int_operand_2 = sizeof_type(member_type_offset.first, program.structs);
							function_ops.push_back(f_op);
						}
						// if reading variable member
						else {
							std::pair<RambutanType, int> member_type_offset = variable_member_offset(f_op, var_offsets, program.structs);
							if (member_type_offset.first.ptr_to_trace > 0) // if it is a pointer, whether to a primitive or a struct
								f_op.mode = MODE_64BIT;
							else if (program.structs.count(member_type_offset.first.base_type))
								f_op.mode = MODE_STRUCT;
							// 8bit values
							else if (sizeof_type(member_type_offset.first, program.structs) == 1)
								f_op.mode = MODE_8BIT;
							// 64bit values
							else if (sizeof_type(member_type_offset.first, program.structs) == 8)
								f_op.mode = MODE_64BIT;

							f_op.type = OP_READ_VAR_STRUCT_MEMBER;
							f_op.int_operand = member_type_offset.second; // offset to where member is located
							f_op.int_operand_2 = sizeof_type(member_type_offset.first, program.structs);
							function_ops.push_back(f_op);
						}
					}
				}
				else if (f_op.type == OP_JMP || f_op.type == OP_CJMPT || f_op.type == OP_CJMPF || f_op.type == OP_JMPE || f_op.type == OP_CJMPET || f_op.type == OP_CJMPEF) {
					i++;
					if (i >= tokens.size()) break;
					Token label_jumpto_tok = tokens.at(i);
					f_op.str_operand = label_jumpto_tok.value;
					function_ops.push_back(f_op);
				}
				else if (f_op.type == OP_PUSH_VAR) {
					f_op.int_operand = var_offsets.at(f_op.str_operand).second;
					function_ops.push_back(f_op);
				}
				else if (f_op.type == OP_PUSH_TYPE_INSTANCE) {
					f_op.int_operand = sizeof_type(f_op.str_operand, program.structs);
					function_ops.push_back(f_op);
				}
				else function_ops.push_back(f_op);
				i++;
			}
			if (found_function_end) {
				program.functions.at(function_name).ops = link_ops(function_ops, labels);
				program.functions.at(function_name).var_offsets = var_offsets;
				program.functions.at(function_name).memory_capacity = offset;
			}
			else {
				print_error_at_loc(tokens.back().loc, "Unexpected EOF while parsing function body");
				exit(1);
			}
		}
		else if (current_op.type == OP_STRUCT) {
			i++;
			// check if struct name is in the tokens stream
			if (i >= tokens.size()) {
				print_error_at_loc(current_op.loc, "unexpected EOF found while parsing struct definition");
				exit(1);
			}
		
			// check struct name
			Token name_token = tokens.at(i);
			std::string struct_name = name_token.value;

			if (!is_legal_name(name_token)) {
				print_error_at_loc(name_token.loc, "illegal name for struct");
				exit(1);
			}
			else if (program.functions.count(struct_name)) {
				print_error_at_loc(name_token.loc, "name '" + struct_name + "' already exists as a function");
				exit(1);
			}
			else if (program.structs.count(struct_name)) {
				print_error_at_loc(name_token.loc, "struct '" + struct_name + "' already exists");
				exit(1);
			}
			i++;

			// check if eof before parsing members of struct
			if (i >= tokens.size()) {
				print_error_at_loc(current_op.loc, "unexpected EOF found while parsing struct definition");
				exit(1);
			}

			std::map<std::string, std::pair<RambutanType, int>> members;
			int offset = 0;
			while (tokens.at(i).value != "end") {
				// get type of next member
				Token type_tok = tokens.at(i);
				std::pair<std::string, int> type_info = parse_type_str(type_tok.value);
				std::string base_type = type_info.first;
				int pointer_count = type_info.second;
			
				// if type is an existing type (primitive or struct)
				// or if the type is a pointer to the currently defined type
				if (!is_prim_type(base_type) && !program.structs.count(base_type)) {
					if (base_type == struct_name && pointer_count > 0);
					else {
						print_error_at_loc(type_tok.loc, "unknown type '" + type_tok.value + "' found while parsing struct definition");
						exit(1);
					}
				}
				// get member name
				i++;
				if (i > tokens.size() - 1) {
					print_error_at_loc(type_tok.loc, "unexpected EOF found while parsing function definition");
					exit(1);
				}
				Token member_name_tok = tokens.at(i);
				if (!is_legal_name(member_name_tok)) {
					print_error_at_loc(member_name_tok.loc, "illegal name for struct member");
					exit(1);
				}

				// set variable type and relative offset
				members.insert({member_name_tok.value, {
					RambutanType(member_name_tok.loc, type_tok.value), offset
				}});

				// increase offset by size of type
				if (pointer_count > 0) offset += 8;
				else offset += sizeof_type(base_type, program.structs);

				i++;
				if (i > tokens.size() - 1) {
					print_error_at_loc(member_name_tok.loc, "unexpected EOF found while parsing function definition");
					exit(1);
				}
			}
			// align the offset into sections of 8
			program.structs.insert({struct_name, 
				Struct(current_op.loc, members, (offset + 7) / 8 * 8)
			});
		}
		else if (current_op.type == OP_IMPORT) {
			if (i++ == tokens.size()) {
				print_error_at_loc(current_op.loc, "Unexpected 'import' keyword found while parsing");
				exit(1);
			}

			Token include_file_token = tokens.at(i);
			if (include_file_token.type != TOKEN_STRING) {
				print_error_at_loc(include_file_token.loc, "Was expecting token of type string after 'import' statement");
				exit(1);
			}

			std::string file_path = include_file_token.value.substr(1,include_file_token.value.length() - 2);
			File file(file_path, FILE_READ);
			if (!file.exists()) {
				print_error_at_loc(include_file_token.loc, "No such file or directory, '" + file_path + "'");
				exit(1);
			}

			if (std::find(include_paths.begin(), include_paths.end(), file_path) == include_paths.end())
			{
				lexer->set_file(file_path);
				lexer->tokenize();
				tokens.insert(tokens.begin() + i + 1, lexer->tokens.begin(), lexer->tokens.end());
				include_paths.push_back(file_path);
			}
		}
		else if (current_op.type == OP_CONST) {
			i++;
			// check if function name is in the token stream
			if (i >= tokens.size()) {
				print_error_at_loc(current_op.loc, "unexpected EOF found while parsing function definition");
				exit(1);
			}
		
			// check function name
			Token name_token = tokens.at(i);
			std::string const_name = name_token.value;

			if (!is_legal_name(name_token)) {
				print_error_at_loc(name_token.loc, "illegal name for function");
				exit(1);
			}
			else if (program.functions.count(const_name)) {
				print_error_at_loc(name_token.loc, "function '" + const_name + "' already exists");
				exit(1);
			}
			else if (program.structs.count(const_name)) {
				print_error_at_loc(name_token.loc, "struct '" + const_name + "' already exists");
				exit(1);
			}
			i++;

			// check if eof before parsing expression
			if (i >= tokens.size()) {
				print_error_at_loc(current_op.loc, "unexpected EOF found while parsing function definition");
				exit(1);
			}

			long long eval = eval_const_expression(name_token.loc);
			program.consts.insert({const_name, Const(name_token.loc, eval)});
		}
		else {
			print_error_at_loc(current_op.loc, "Unexpected token found while parsing");
			exit(1);
		}

		i++;
	}
}
