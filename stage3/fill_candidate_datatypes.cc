/*
 *  matiec - a compiler for the programming languages defined in IEC 61131-3
 *
 *  Copyright (C) 2009-2012  Mario de Sousa (msousa@fe.up.pt)
 *  Copyright (C) 2012       Manuele Conti (manuele.conti@sirius-es.it)
 *  Copyright (C) 2012       Matteo Facchinetti (matteo.facchinetti@sirius-es.it)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * This code is made available on the understanding that it will not be
 * used in safety-critical situations without a full and competent review.
 */

/*
 * An IEC 61131-3 compiler.
 *
 * Based on the
 * FINAL DRAFT - IEC 61131-3, 2nd Ed. (2001-12-10)
 *
 */


/*
 *  Fill candidate list of data types for all symbols
 */

#include "fill_candidate_datatypes.hh"
#include "datatype_functions.hh"
#include <typeinfo>
#include <list>
#include <string>
#include <string.h>
#include <strings.h>

/* set to 1 to see debug info during execution */
static int debug = 0;

fill_candidate_datatypes_c::fill_candidate_datatypes_c(symbol_c *ignore) {

}

fill_candidate_datatypes_c::~fill_candidate_datatypes_c(void) {
}

symbol_c *fill_candidate_datatypes_c::widening_conversion(symbol_c *left_type, symbol_c *right_type, const struct widen_entry widen_table[]) {
	int k;
	/* find a widening table entry compatible */
	for (k = 0; NULL != widen_table[k].left;  k++)
		if ((typeid(*left_type) == typeid(*widen_table[k].left)) && (typeid(*right_type) == typeid(*widen_table[k].right)))
			return widen_table[k].result;
	return NULL;
}

void fill_candidate_datatypes_c::match_nonformal_call(symbol_c *f_call, symbol_c *f_decl, int *error_count) {
	symbol_c *call_param_value,  *param_type;
	identifier_c *param_name;
	function_param_iterator_c       fp_iterator(f_decl);
	function_call_param_iterator_c fcp_iterator(f_call);
	int extensible_parameter_highest_index = -1;
	unsigned int i;

	/* reset error counter */
	if (error_count != NULL) *error_count = 0;
	/* Iterating through the non-formal parameters of the function call */
	while((call_param_value = fcp_iterator.next_nf()) != NULL) {
		/* Obtaining the type of the value being passed in the function call */
		std::vector <symbol_c *>&call_param_types = call_param_value->candidate_datatypes;
		/* Iterate to the next parameter of the function being called.
		 * Get the name of that parameter, and ignore if EN or ENO.
		 */
		do {
			param_name = fp_iterator.next();
			/* If there is no other parameter declared, then we are passing too many parameters... */
			if(param_name == NULL) {
				(*error_count)++;
				return;
			}
		} while ((strcmp(param_name->value, "EN") == 0) || (strcmp(param_name->value, "ENO") == 0));

		/* Get the parameter type */
		param_type = base_type(fp_iterator.param_type());
		for(i = 0; i < call_param_types.size(); i++) {
			/* If the declared parameter and the parameter from the function call do not have the same type */
			if(is_type_equal(param_type, call_param_types[i])) {
				break;
			}
		}
		if (i >= call_param_types.size()) (*error_count)++;
	}
}

void fill_candidate_datatypes_c::match_formal_call(symbol_c *f_call, symbol_c *f_decl, int *error_count) {
	symbol_c *call_param_value, *call_param_name, *param_type;
	symbol_c *verify_duplicate_param;
	identifier_c *param_name;
	function_param_iterator_c       fp_iterator(f_decl);
	function_call_param_iterator_c fcp_iterator(f_call);
	int extensible_parameter_highest_index = -1;
	identifier_c *extensible_parameter_name;
	unsigned int i;

	/* reset error counter */
	if (error_count != NULL) *error_count = 0;

	/* Iterating through the formal parameters of the function call */
	while((call_param_name = fcp_iterator.next_f()) != NULL) {

		/* Obtaining the value being passed in the function call */
		call_param_value = fcp_iterator.get_current_value();
		/* the following should never occur. If it does, then we have a bug in our code... */
		if (NULL == call_param_value) ERROR;

		/* Checking if there are duplicated parameter values */
		verify_duplicate_param = fcp_iterator.search_f(call_param_name);
		if(verify_duplicate_param != call_param_value)
			(*error_count)++;

		/* Obtaining the type of the value being passed in the function call */
		std::vector <symbol_c *>&call_param_types = call_param_value->candidate_datatypes;


		/* Find the corresponding parameter in function declaration */
		param_name = fp_iterator.search(call_param_name);
		if(param_name == NULL) {
			(*error_count)++;
		} else {
			/* Get the parameter type */
			param_type = base_type(fp_iterator.param_type());
			for (i = 0; i < call_param_types.size(); i++) {
				/* If the declared parameter and the parameter from the function call have the same type */
				if(is_type_equal(param_type, call_param_types[i]))
					break;
			}
			if (i >= call_param_types.size()) (*error_count)++;
		}
	}

}

/* a helper function... */
symbol_c *fill_candidate_datatypes_c::base_type(symbol_c *symbol) {
	/* NOTE: symbol == NULL is valid. It will occur when, for e.g., an undefined/undeclared symbolic_variable is used
	 *       in the code.
	 */
	if (symbol == NULL) return NULL;
	return (symbol_c *)symbol->accept(search_base_type);
}

/*********************/
/* B 1.2 - Constants */
/*********************/
/******************************/
/* B 1.2.1 - Numeric Literals */
/******************************/
void *fill_candidate_datatypes_c::visit(real_c *symbol) {
	int calc_size;

	calc_size = sizeoftype(symbol);
	if (calc_size <= sizeoftype(&search_constant_type_c::real_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::real_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::real_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::lreal_type_name);
	if (debug) std::cout << "ANY_REAL [" << symbol->candidate_datatypes.size() << "]" << std::endl;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(integer_c *symbol) {
	int calc_size;

	calc_size = sizeoftype(symbol);
	if (calc_size <= sizeoftype(&search_constant_type_c::bool_type_name))
	        symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::byte_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::byte_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::word_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::word_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::dword_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::dword_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::lword_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::lword_type_name);

	if (calc_size < sizeoftype(&search_constant_type_c::sint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::sint_type_name);
	if (calc_size < sizeoftype(&search_constant_type_c::int_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::int_type_name);
	if (calc_size < sizeoftype(&search_constant_type_c::dint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::dint_type_name);
	if (calc_size < sizeoftype(&search_constant_type_c::lint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::lint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::usint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::usint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::uint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::uint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::udint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::udint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::ulint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::ulint_type_name);
	if (debug) std::cout << "ANY_INT [" << symbol->candidate_datatypes.size()<< "]" << std::endl;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(neg_real_c *symbol) {
	if (sizeoftype(symbol) <= sizeoftype(&search_constant_type_c::real_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::real_type_name);
	symbol->candidate_datatypes.push_back(&search_constant_type_c::lreal_type_name);
	if (debug) std::cout << "neg ANY_REAL [" << symbol->candidate_datatypes.size() << "]" << std::endl;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(neg_integer_c *symbol) {
	int calc_size;

	calc_size = sizeoftype(symbol);
	if (calc_size <= sizeoftype(&search_constant_type_c::int_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::int_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::sint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::sint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::dint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::dint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::lint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::lint_type_name);
	if (debug) std::cout << "neg ANY_INT [" << symbol->candidate_datatypes.size() << "]" << std::endl;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(binary_integer_c *symbol) {
	int calc_size;

	calc_size = sizeoftype(symbol);
	if (calc_size <= sizeoftype(&search_constant_type_c::bool_type_name))
	        symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::byte_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::byte_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::word_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::word_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::dword_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::dword_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::lword_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::lword_type_name);
	
	if (calc_size < sizeoftype(&search_constant_type_c::sint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::sint_type_name);
	if (calc_size < sizeoftype(&search_constant_type_c::int_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::int_type_name);
	if (calc_size < sizeoftype(&search_constant_type_c::dint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::dint_type_name);
	if (calc_size < sizeoftype(&search_constant_type_c::lint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::lint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::usint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::usint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::uint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::uint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::udint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::udint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::ulint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::ulint_type_name);
	if (debug) std::cout << "ANY_INT [" << symbol->candidate_datatypes.size()<< "]" << std::endl;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(octal_integer_c *symbol) {
	int calc_size;

	calc_size = sizeoftype(symbol);
	if (calc_size <= sizeoftype(&search_constant_type_c::bool_type_name))
	        symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::byte_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::byte_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::word_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::word_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::dword_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::dword_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::lword_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::lword_type_name);

	if (calc_size < sizeoftype(&search_constant_type_c::sint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::sint_type_name);
	if (calc_size < sizeoftype(&search_constant_type_c::int_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::int_type_name);
	if (calc_size < sizeoftype(&search_constant_type_c::dint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::dint_type_name);
	if (calc_size < sizeoftype(&search_constant_type_c::lint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::lint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::usint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::usint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::uint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::uint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::udint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::udint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::ulint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::ulint_type_name);
	if (debug) std::cout << "ANY_INT [" << symbol->candidate_datatypes.size()<< "]" << std::endl;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(hex_integer_c *symbol) {
	int calc_size;

	calc_size = sizeoftype(symbol);
	if (calc_size <= sizeoftype(&search_constant_type_c::bool_type_name))
	        symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::byte_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::byte_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::word_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::word_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::dword_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::dword_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::lword_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::lword_type_name);

	if (calc_size < sizeoftype(&search_constant_type_c::sint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::sint_type_name);
	if (calc_size < sizeoftype(&search_constant_type_c::int_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::int_type_name);
	if (calc_size < sizeoftype(&search_constant_type_c::dint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::dint_type_name);
	if (calc_size < sizeoftype(&search_constant_type_c::lint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::lint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::usint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::usint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::uint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::uint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::udint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::udint_type_name);
	if (calc_size <= sizeoftype(&search_constant_type_c::ulint_type_name))
		symbol->candidate_datatypes.push_back(&search_constant_type_c::ulint_type_name);
	if (debug) std::cout << "ANY_INT [" << symbol->candidate_datatypes.size()<< "]" << std::endl;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(integer_literal_c *symbol) {
	symbol->candidate_datatypes.push_back(symbol->type);
	if (debug) std::cout << "INT_LITERAL [" << symbol->candidate_datatypes.size() << "]\n";
	return NULL;
}

void *fill_candidate_datatypes_c::visit(real_literal_c *symbol) {
	symbol->candidate_datatypes.push_back(symbol->type);
	if (debug) std::cout << "REAL_LITERAL [" << symbol->candidate_datatypes.size() << "]\n";
	return NULL;
}

void *fill_candidate_datatypes_c::visit(bit_string_literal_c *symbol) {
	symbol->candidate_datatypes.push_back(symbol->type);
	return NULL;
}

void *fill_candidate_datatypes_c::visit(boolean_literal_c *symbol) {
	symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	return NULL;
}

void *fill_candidate_datatypes_c::visit(boolean_true_c *symbol) {
	symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	return NULL;
}

void *fill_candidate_datatypes_c::visit(boolean_false_c *symbol) {
	symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	return NULL;
}

/*******************************/
/* B.1.2.2   Character Strings */
/*******************************/
void *fill_candidate_datatypes_c::visit(double_byte_character_string_c *symbol) {
	symbol->candidate_datatypes.push_back(&search_constant_type_c::wstring_type_name);
	return NULL;
}

void *fill_candidate_datatypes_c::visit(single_byte_character_string_c *symbol) {
	symbol->candidate_datatypes.push_back(&search_constant_type_c::string_type_name);
	return NULL;
}

/***************************/
/* B 1.2.3 - Time Literals */
/***************************/
/************************/
/* B 1.2.3.1 - Duration */
/************************/
void *fill_candidate_datatypes_c::visit(duration_c *symbol) {
	symbol->candidate_datatypes.push_back(symbol->type_name);
	if (debug) std::cout << "TIME_LITERAL [" << symbol->candidate_datatypes.size() << "]\n";
	return NULL;
}

/************************************/
/* B 1.2.3.2 - Time of day and Date */
/************************************/
void *fill_candidate_datatypes_c::visit(time_of_day_c *symbol) {
	symbol->candidate_datatypes.push_back(symbol->type_name);
	return NULL;
}

void *fill_candidate_datatypes_c::visit(date_c *symbol) {
	symbol->candidate_datatypes.push_back(symbol->type_name);
	return NULL;
}

void *fill_candidate_datatypes_c::visit(date_and_time_c *symbol) {
	symbol->candidate_datatypes.push_back(symbol->type_name);
	return NULL;
}

/**********************/
/* B 1.3 - Data types */
/**********************/
/********************************/
/* B 1.3.3 - Derived data types */
/********************************/
/*  signed_integer DOTDOT signed_integer */
// SYM_REF2(subrange_c, lower_limit, upper_limit)
void *fill_candidate_datatypes_c::visit(subrange_c *symbol) {
	symbol->lower_limit->accept(*this);
	symbol->upper_limit->accept(*this);
	
	for (unsigned int u = 0; u < symbol->upper_limit->candidate_datatypes.size(); u++) {
		for(unsigned int l = 0; l < symbol->lower_limit->candidate_datatypes.size(); l++) {
			if (is_type_equal(symbol->upper_limit->candidate_datatypes[u], symbol->lower_limit->candidate_datatypes[l]))
				symbol->candidate_datatypes.push_back(symbol->lower_limit->candidate_datatypes[l]);
		}
	}
	return NULL;
}


void *fill_candidate_datatypes_c::visit(data_type_declaration_c *symbol) {
	// TODO !!!
	/* for the moment we must return NULL so semantic analysis of remaining code is not interrupted! */
	return NULL;
}

void *fill_candidate_datatypes_c::visit(enumerated_value_c *symbol) {
	symbol_c *enumerated_type;

	if (NULL != symbol->type)
		enumerated_type = symbol->type;
	else {
		enumerated_type = enumerated_value_symtable.find_value(symbol->value);
		if (enumerated_type == enumerated_value_symtable.end_value())
			enumerated_type = NULL;
	}
	enumerated_type = base_type(enumerated_type);
	if (NULL != enumerated_type)
		symbol->candidate_datatypes.push_back(enumerated_type);

	if (debug) std::cout << "ENUMERATE [" << symbol->candidate_datatypes.size() << "]\n";
	return NULL;
}


/*********************/
/* B 1.4 - Variables */
/*********************/
void *fill_candidate_datatypes_c::visit(symbolic_variable_c *symbol) {
	symbol_c *result = search_varfb_instance_type->get_basetype_decl(symbol);
	if (NULL != result)
		symbol->candidate_datatypes.push_back(result);
	if (debug) std::cout << "VAR [" << symbol->candidate_datatypes.size() << "]\n";
	return NULL;
}

/********************************************/
/* B 1.4.1 - Directly Represented Variables */
/********************************************/
void *fill_candidate_datatypes_c::visit(direct_variable_c *symbol) {
	/* Comment added by mario:
	 * The following code is safe, actually, as the lexical parser guarantees the correct IEC61131-3 syntax was used.
	 */
	/* However, we should probably add an assertion in case we later change the lexical parser! */
	/* if (symbol->value == NULL) ERROR;
	 * if (symbol->value[0] == '\0') ERROR;
	 * if (symbol->value[1] == '\0') ERROR;
	 */
	switch (symbol->value[2]) {
	case 'X': // bit - 1 bit
		symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
		break;

	case 'B': // byte - 8 bits
		symbol->candidate_datatypes.push_back(&search_constant_type_c::byte_type_name);
		break;

	case 'W': // word - 16 bits
		symbol->candidate_datatypes.push_back(&search_constant_type_c::word_type_name);
		break;

	case 'D': // double word - 32 bits
		symbol->candidate_datatypes.push_back(&search_constant_type_c::dword_type_name);
		break;

	case 'L': // long word - 64 bits
		symbol->candidate_datatypes.push_back(&search_constant_type_c::lword_type_name);
		break;

	default:  // if none of the above, then the empty string was used <=> boolean
		symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
		break;
	}
	return NULL;
}

/*************************************/
/* B 1.4.2 - Multi-element variables */
/*************************************/
/*  subscripted_variable '[' subscript_list ']' */
// SYM_REF2(array_variable_c, subscripted_variable, subscript_list)
void *fill_candidate_datatypes_c::visit(array_variable_c *symbol) {
	/* get the declaration of the data type __stored__ in the array... */
	/* if we were to want the data type of the array itself, then we should call_param_name
	 * search_varfb_instance_type->get_basetype_decl(symbol->subscripted_variable)
	 */
	symbol_c *result = search_varfb_instance_type->get_basetype_decl(symbol);
	if (NULL != result) symbol->candidate_datatypes.push_back(result);
	
	/* recursively call the subscript list, so we can check the data types of the expressions used for the subscripts */
if (debug) std::cout << "ARRAY_VAR XXX\n";		
	symbol->subscript_list->accept(*this);
if (debug) std::cout << "ARRAY_VAR YYY\n";		

	if (debug) std::cout << "ARRAY_VAR [" << symbol->candidate_datatypes.size() << "]\n";	
	return NULL;
}


/* subscript_list ',' subscript */
// SYM_LIST(subscript_list_c)
/* NOTE: we inherit from iterator visitor, so we do not need to implement this method... */
#if 0
void *fill_candidate_datatypes_c::visit(subscript_list_c *symbol) {
}
#endif


/*  record_variable '.' field_selector */
/*  WARNING: input and/or output variables of function blocks
 *           may be accessed as fields of a structured variable!
 *           Code handling a structured_variable_c must take
 *           this into account!
 */
// SYM_REF2(structured_variable_c, record_variable, field_selector)
/* NOTE: We do not need to recursively determine the data types of each field_selector, as the search_varfb_instance_type
 * will do that for us. So we determine the candidate datatypes only for the full structured_variable.
 */
void *fill_candidate_datatypes_c::visit(structured_variable_c *symbol) {
	symbol_c *result = search_varfb_instance_type->get_basetype_decl(symbol);
	if (NULL != result) symbol->candidate_datatypes.push_back(result);
	return NULL;
}

/************************************/
/* B 1.5 Program organization units */
/************************************/
/*********************/
/* B 1.5.1 Functions */
/*********************/
void *fill_candidate_datatypes_c::visit(function_declaration_c *symbol) {
	search_varfb_instance_type = new search_varfb_instance_type_c(symbol);
	symbol->var_declarations_list->accept(*this);
	if (debug) printf("Filling candidate data types list in body of function %s\n", ((token_c *)(symbol->derived_function_name))->value);
	il_parenthesis_level = 0;
	prev_il_instruction = NULL;
	symbol->function_body->accept(*this);
	prev_il_instruction = NULL;
	delete search_varfb_instance_type;
	search_varfb_instance_type = NULL;
	return NULL;
}

/***************************/
/* B 1.5.2 Function blocks */
/***************************/
void *fill_candidate_datatypes_c::visit(function_block_declaration_c *symbol) {
	search_varfb_instance_type = new search_varfb_instance_type_c(symbol);
	symbol->var_declarations->accept(*this);
	if (debug) printf("Filling candidate data types list in body of FB %s\n", ((token_c *)(symbol->fblock_name))->value);
	il_parenthesis_level = 0;
	prev_il_instruction = NULL;
	symbol->fblock_body->accept(*this);
	prev_il_instruction = NULL;
	delete search_varfb_instance_type;
	search_varfb_instance_type = NULL;
	return NULL;
}

/**********************/
/* B 1.5.3 - Programs */
/**********************/
void *fill_candidate_datatypes_c::visit(program_declaration_c *symbol) {
	search_varfb_instance_type = new search_varfb_instance_type_c(symbol);
	symbol->var_declarations->accept(*this);
	if (debug) printf("Filling candidate data types list in body of program %s\n", ((token_c *)(symbol->program_type_name))->value);
	il_parenthesis_level = 0;
	prev_il_instruction = NULL;
	symbol->function_block_body->accept(*this);
	prev_il_instruction = NULL;
	delete search_varfb_instance_type;
	search_varfb_instance_type = NULL;
	return NULL;
}



/********************************/
/* B 1.7 Configuration elements */
/********************************/
void *fill_candidate_datatypes_c::visit(configuration_declaration_c *symbol) {
#if 0
	// TODO !!!
	/* for the moment we must return NULL so semantic analysis of remaining code is not interrupted! */
#endif
	return NULL;
}

/****************************************/
/* B.2 - Language IL (Instruction List) */
/****************************************/
/***********************************/
/* B 2.1 Instructions and Operands */
/***********************************/
// void *visit(instruction_list_c *symbol);
void *fill_candidate_datatypes_c::visit(il_simple_operation_c *symbol) {
	/* determine the data type of the operand */
	if (NULL != symbol->il_operand) {
		symbol->il_operand->accept(*this);
	}
	/* recursive call to fill the candidate data types list */
	il_operand = symbol->il_operand;
	symbol->il_simple_operator->accept(*this);
	il_operand = NULL;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(il_function_call_c *symbol) {
}

/* MJS: Manuele, could you please not delete the following 2 lines of comments. They help me understand where this class is used
 *     and when it is created by bison - syntax parse, and how it can show up in the abstract syntax tree.
 *
 *       Actually, it could be helpful if we could have all the similar comments already present in visit_expression_type_c
 *       in the 3 new classes fill/narrow/print candidate datatype 
 */
/* | il_expr_operator '(' [il_operand] eol_list [simple_instr_list] ')' */
// SYM_REF3(il_expression_c, il_expr_operator, il_operand, simple_instr_list);
void *fill_candidate_datatypes_c::visit(il_expression_c *symbol) {
  if (NULL != symbol->il_operand)
    symbol->il_operand->accept(*this);
  
  il_parenthesis_level++;

  /* Note that prev_il_instruction will actually be used to get the current value store in the il_default_variable */
  /* If a symbol->il_operand is provided, then that will be the result before executing the simple_instr_list.
   * If this symbol is NULL, then the current result is also NULL, which is correct for what we want to do!
   */
  symbol_c *prev_il_instruction_backup = prev_il_instruction;
  prev_il_instruction = symbol->il_operand;
  
  if(symbol->simple_instr_list != NULL) {
    symbol->simple_instr_list->accept(*this);
  }

  il_parenthesis_level--;
  if (il_parenthesis_level < 0) ERROR;

  /* Now check the if the data type semantics of operation are correct,  */
  il_operand = prev_il_instruction;
  prev_il_instruction = prev_il_instruction_backup;
  symbol->il_expr_operator->accept(*this);
  il_operand = NULL;
  return NULL;
}

void *fill_candidate_datatypes_c::visit(il_jump_operation_c *symbol) {
  /* recursive call to fill the candidate data types list */
  il_operand = NULL;
  symbol->il_jump_operator->accept(*this);
  il_operand = NULL;
  return NULL;
}

void *fill_candidate_datatypes_c::visit(il_fb_call_c *symbol) {
}

void *fill_candidate_datatypes_c::visit(il_formal_funct_call_c *symbol) {

}

/*
    void *visit(il_operand_list_c *symbol);
    void *visit(simple_instr_list_c *symbol);
    void *visit(il_param_list_c *symbol);
    void *visit(il_param_assignment_c *symbol);
    void *visit(il_param_out_assignment_c *symbol);
*/

/*******************/
/* B 2.2 Operators */
/*******************/
void *fill_candidate_datatypes_c::visit(LD_operator_c *symbol) {
	for(unsigned int i = 0; i < il_operand->candidate_datatypes.size(); i++) {
		symbol->candidate_datatypes.push_back(il_operand->candidate_datatypes[i]);
	}
	if (debug) std::cout << "LD [" <<  il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(LDN_operator_c *symbol) {
	for(unsigned int i = 0; i < il_operand->candidate_datatypes.size(); i++) {
		if      (is_ANY_BIT_compatible(il_operand->candidate_datatypes[i]))
			symbol->candidate_datatypes.push_back(il_operand->candidate_datatypes[i]);
	}
	if (debug) std::cout << "LDN [" << il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(ST_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type,operand_type))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	if (debug) std::cout << "ST [" << prev_il_instruction->candidate_datatypes.size() << "," << il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(STN_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type,operand_type) && is_ANY_BIT_compatible(il_operand->candidate_datatypes[i]))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	if (debug) std::cout << "STN [" << prev_il_instruction->candidate_datatypes.size() << "," << il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(NOT_operator_c *symbol) {
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(S_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type,operand_type) && is_ANY_BOOL_compatible(il_operand->candidate_datatypes[i]))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	if (debug) std::cout << "S [" << prev_il_instruction->candidate_datatypes.size() << "," << il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(R_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type,operand_type) && is_ANY_BOOL_compatible(il_operand->candidate_datatypes[i]))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	if (debug) std::cout << "R [" << prev_il_instruction->candidate_datatypes.size() << "," << il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(S1_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type,operand_type) && is_ANY_BOOL_compatible(il_operand->candidate_datatypes[i]))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	if (debug) std::cout << "S1 [" << prev_il_instruction->candidate_datatypes.size() << "," << il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(R1_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type,operand_type) && is_ANY_BOOL_compatible(il_operand->candidate_datatypes[i]))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	if (debug) std::cout << "R1 [" << prev_il_instruction->candidate_datatypes.size() << "," << il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(CLK_operator_c *symbol) {
	/* MANU:
	 * How it works? I(MANU) don't know this function
	 */
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(CU_operator_c *symbol) {
	/* MANU:
	 * How it works? I(MANU) don't know this function
	 */
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(CD_operator_c *symbol) {
	/* MANU:
	 * How it works? I(MANU) don't know this function
	 */

	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(PV_operator_c *symbol) {
	/* MANU:
	 * How it works? I(MANU) don't know this function
	 */

	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(IN_operator_c *symbol) {
	/* MANU:
	 * How it works? I(MANU) don't know this function
	 */

	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(PT_operator_c *symbol) {
	/* MANU:
	 * How it works? I(MANU) don't know this function
	 */

	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(AND_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type, operand_type) &&
					is_ANY_BIT_compatible(operand_type))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(OR_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type, operand_type) &&
					is_ANY_BIT_compatible(operand_type))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(XOR_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type, operand_type) &&
					is_ANY_BIT_compatible(operand_type))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(ANDN_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type, operand_type) &&
					is_ANY_BIT_compatible(operand_type))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(ORN_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type, operand_type) &&
					is_ANY_BIT_compatible(operand_type))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(XORN_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type, operand_type) &&
					is_ANY_BIT_compatible(operand_type))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(ADD_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for(unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type, operand_type) &&
					is_ANY_NUM_compatible(prev_instruction_type))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
			else {
				symbol_c *result = widening_conversion(prev_instruction_type, operand_type, widen_ADD_table);
				if (result)
					symbol->candidate_datatypes.push_back(result);

			}
		}
	}
	if (debug) std::cout <<  "ADD [" << prev_il_instruction->candidate_datatypes.size() << "," << il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(SUB_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for(unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type, operand_type) &&
					is_ANY_NUM_compatible(prev_instruction_type))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
			else {
				symbol_c *result = widening_conversion(prev_instruction_type, operand_type, widen_SUB_table);
				if (result)
					symbol->candidate_datatypes.push_back(result);
			}
		}
	}
	if (debug) std::cout <<  "SUB [" << prev_il_instruction->candidate_datatypes.size() << "," << il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(MUL_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for(unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type, operand_type) &&
					is_ANY_NUM_compatible(prev_instruction_type))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
			else {
				symbol_c *result = widening_conversion(prev_instruction_type, operand_type, widen_MUL_table);
				if (result)
					symbol->candidate_datatypes.push_back(result);
			}
		}
	}
	if (debug) std::cout <<  "MUL [" << prev_il_instruction->candidate_datatypes.size() << "," << il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(DIV_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for(unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type, operand_type) &&
					is_ANY_NUM_compatible(prev_instruction_type))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
			else {
				symbol_c *result = widening_conversion(prev_instruction_type, operand_type, widen_DIV_table);
				if (result)
					symbol->candidate_datatypes.push_back(result);
			}
		}
	}
	if (debug) std::cout <<  "DIV [" << prev_il_instruction->candidate_datatypes.size() << "," << il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(MOD_operator_c *symbol) {
	symbol_c *prev_instruction_type, *operand_type;

	if (NULL == prev_il_instruction) return NULL;
	for(unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			prev_instruction_type = prev_il_instruction->candidate_datatypes[i];
			operand_type = il_operand->candidate_datatypes[j];
			if (is_type_equal(prev_instruction_type, operand_type) &&
					is_ANY_INT_compatible(prev_instruction_type))
				symbol->candidate_datatypes.push_back(prev_instruction_type);
		}
	}
	if (debug) std::cout <<  "MOD [" << prev_il_instruction->candidate_datatypes.size() << "," << il_operand->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(GT_operator_c *symbol) {
	bool found = false;

	if (NULL == prev_il_instruction) return NULL;
	for(unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			if (is_type_equal(prev_il_instruction->candidate_datatypes[i], il_operand->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_compatible(prev_il_instruction->candidate_datatypes[i])) {
				found = true;
				break;
			}
		}
	}
	if (found) symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(GE_operator_c *symbol) {
	bool found = false;

	if (NULL == prev_il_instruction) return NULL;
	for(unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			if (is_type_equal(prev_il_instruction->candidate_datatypes[i], il_operand->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_compatible(prev_il_instruction->candidate_datatypes[i])) {
				found = true;
				break;
			}
		}
	}
	if (found) symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(EQ_operator_c *symbol) {
	bool found = false;

	if (NULL == prev_il_instruction) return NULL;
	for(unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			if (is_type_equal(prev_il_instruction->candidate_datatypes[i], il_operand->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_compatible(prev_il_instruction->candidate_datatypes[i])) {
				found = true;
				break;
			}
		}
	}
	if (found) symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(LT_operator_c *symbol) {
	bool found = false;

	if (NULL == prev_il_instruction) return NULL;
	for(unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			if (is_type_equal(prev_il_instruction->candidate_datatypes[i], il_operand->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_compatible(prev_il_instruction->candidate_datatypes[i])) {
				found = true;
				break;
			}
		}
	}
	if (found) symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(LE_operator_c *symbol) {
	bool found = false;

	if (NULL == prev_il_instruction) return NULL;
	for(unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			if (is_type_equal(prev_il_instruction->candidate_datatypes[i], il_operand->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_compatible(prev_il_instruction->candidate_datatypes[i])) {
				found = true;
				break;
			}
		}
	}
	if (found) symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(NE_operator_c *symbol) {
	bool found = false;

	if (NULL == prev_il_instruction) return NULL;
	for(unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < il_operand->candidate_datatypes.size(); j++) {
			if (is_type_equal(prev_il_instruction->candidate_datatypes[i], il_operand->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_compatible(prev_il_instruction->candidate_datatypes[i])) {
				found = true;
				break;
			}
		}
	}
	if (found) symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(CAL_operator_c *symbol) {
	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
	        /* does not need to be bool type !! */
		symbol->candidate_datatypes.push_back(prev_il_instruction->candidate_datatypes[i]);
	}
	if (debug) std::cout <<  "CAL [" << prev_il_instruction->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(CALC_operator_c *symbol) {
	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		if (is_type(prev_il_instruction->candidate_datatypes[i], bool_type_name_c))
			symbol->candidate_datatypes.push_back(prev_il_instruction->candidate_datatypes[i]);
	}
	if (debug) std::cout <<  "CALC [" << prev_il_instruction->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(CALCN_operator_c *symbol) {
	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		if (is_type(prev_il_instruction->candidate_datatypes[i], bool_type_name_c))
			symbol->candidate_datatypes.push_back(prev_il_instruction->candidate_datatypes[i]);
	}
	if (debug) std::cout <<  "CALCN [" << prev_il_instruction->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(RET_operator_c *symbol) {
	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
	        /* does not need to be bool type !! */
		symbol->candidate_datatypes.push_back(prev_il_instruction->candidate_datatypes[i]);
	}
	if (debug) std::cout <<  "RET [" << prev_il_instruction->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(RETC_operator_c *symbol) {
	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		if (is_type(prev_il_instruction->candidate_datatypes[i], bool_type_name_c))
			symbol->candidate_datatypes.push_back(prev_il_instruction->candidate_datatypes[i]);
	}
	if (debug) std::cout <<  "RETC [" << prev_il_instruction->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(RETCN_operator_c *symbol) {
	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		if (is_type(prev_il_instruction->candidate_datatypes[i], bool_type_name_c))
			symbol->candidate_datatypes.push_back(prev_il_instruction->candidate_datatypes[i]);
	}
	if (debug) std::cout <<  "RETCN [" << prev_il_instruction->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(JMP_operator_c *symbol) {
	if (NULL == prev_il_instruction) return NULL;
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
	        /* does not need to be bool type !! */
		symbol->candidate_datatypes.push_back(prev_il_instruction->candidate_datatypes[i]);
	}
	if (debug) std::cout <<  "JMP [" << prev_il_instruction->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(JMPC_operator_c *symbol) {
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		if (is_type(prev_il_instruction->candidate_datatypes[i], bool_type_name_c))
			symbol->candidate_datatypes.push_back(prev_il_instruction->candidate_datatypes[i]);
	}
	if (debug) std::cout <<  "JMPC [" << prev_il_instruction->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}

void *fill_candidate_datatypes_c::visit(JMPCN_operator_c *symbol) {
	for (unsigned int i = 0; i < prev_il_instruction->candidate_datatypes.size(); i++) {
		if (is_type(prev_il_instruction->candidate_datatypes[i], bool_type_name_c))
			symbol->candidate_datatypes.push_back(prev_il_instruction->candidate_datatypes[i]);
	}
	if (debug) std::cout <<  "JMPCN [" << prev_il_instruction->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	prev_il_instruction = symbol;
	return NULL;
}
/* Symbol class handled together with function call checks */
// void *visit(il_assign_operator_c *symbol, variable_name);
/* Symbol class handled together with function call checks */
// void *visit(il_assign_operator_c *symbol, option, variable_name);

/***************************************/
/* B.3 - Language ST (Structured Text) */
/***************************************/
/***********************/
/* B 3.1 - Expressions */
/***********************/

void *fill_candidate_datatypes_c::visit(or_expression_c *symbol) {
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	for (unsigned  int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for (unsigned  int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (is_type_equal(symbol->l_exp->candidate_datatypes[i], symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_BIT_compatible(symbol->l_exp->candidate_datatypes[i]))
				symbol->candidate_datatypes.push_back(symbol->l_exp->candidate_datatypes[i]);
		}
	}
	return NULL;
}


void *fill_candidate_datatypes_c::visit(xor_expression_c *symbol) {
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);

	for (unsigned  int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for (unsigned  int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (is_type_equal(symbol->l_exp->candidate_datatypes[i], symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_BIT_compatible(symbol->l_exp->candidate_datatypes[i]))
				symbol->candidate_datatypes.push_back(symbol->l_exp->candidate_datatypes[i]);
		}
	}
	return NULL;
}


void *fill_candidate_datatypes_c::visit(and_expression_c *symbol) {
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);

	for (unsigned  int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for (unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (is_type_equal(symbol->l_exp->candidate_datatypes[i], symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_BIT_compatible(symbol->l_exp->candidate_datatypes[i]))
				symbol->candidate_datatypes.push_back(symbol->l_exp->candidate_datatypes[i]);
		}
	}
	return NULL;
}


void *fill_candidate_datatypes_c::visit(equ_expression_c *symbol) {
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	bool found = false;

	for (unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for (unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (is_type_equal(symbol->l_exp->candidate_datatypes[i], symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_compatible(symbol->l_exp->candidate_datatypes[i])) {
				found = true;
				break;
			}
		}
	}
	if (found) symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	return NULL;
}


void *fill_candidate_datatypes_c::visit(notequ_expression_c *symbol)  {
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	bool found = false;

	for (unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for (unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (is_type_equal(symbol->l_exp->candidate_datatypes[i], symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_compatible(symbol->l_exp->candidate_datatypes[i])) {
				found = true;
				break;
			}
		}
	}
	if (found)
		symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	return NULL;
}


void *fill_candidate_datatypes_c::visit(lt_expression_c *symbol) {
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	bool found = false;

	for (unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for (unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (is_type_equal(symbol->l_exp->candidate_datatypes[i], symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_compatible(symbol->l_exp->candidate_datatypes[i])) {
				found = true;
				break;
			}
		}
	}
	if (found)
		symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	return NULL;
}


void *fill_candidate_datatypes_c::visit(gt_expression_c *symbol) {
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	bool found = false;

	for (unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for (unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (is_type_equal(symbol->l_exp->candidate_datatypes[i], symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_compatible(symbol->l_exp->candidate_datatypes[i])) {
				found = true;
				break;
			}
		}
	}
	if (found)
		symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	return NULL;
}

void *fill_candidate_datatypes_c::visit(le_expression_c *symbol) {
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	bool found = false;

	for (unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for (unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (is_type_equal(symbol->l_exp->candidate_datatypes[i], symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_compatible(symbol->l_exp->candidate_datatypes[i])) {
				found = true;
				break;
			}
		}
	}
	if (found)
		symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	return NULL;
}

void *fill_candidate_datatypes_c::visit(ge_expression_c *symbol) {
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	bool found = false;

	for (unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for (unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (is_type_equal(symbol->l_exp->candidate_datatypes[i], symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_compatible(symbol->l_exp->candidate_datatypes[i])) {
				found = true;
				break;
			}
		}
	}
	if (found)
		symbol->candidate_datatypes.push_back(&search_constant_type_c::bool_type_name);
	return NULL;
}

void *fill_candidate_datatypes_c::visit(add_expression_c *symbol) {
	/* The following code is correct when handling the addition of 2 symbolic_variables
	 * In this case, adding two variables (e.g. USINT_var1 + USINT_var2) will always yield
	 * the same data type, even if the result of the adition could not fit inside the same
	 * data type (due to overflowing)
	 *
	 * However, when adding two literals (e.g. USINT#42 + USINT#3)
	 * we should be able to detect overflows of the result, and therefore not consider
	 * that the result may be of type USINT.
	 * Currently we do not yet detect these overflows, and allow handling the sum of two USINTs
	 * as always resulting in an USINT, even in the following expression
	 * (USINT#65535 + USINT#2).
	 *
	 * In the future we can add some code to reduce
	 * all the expressions that are based on literals into the resulting literal
	 * value (maybe some visitor class that will run before or after data type
	 * checking). Since this class will have to be very careful to make sure it implements the same mathematical
	 * details (e.g. how to round and truncate numbers) as defined in IEC 61131-3, we will leave this to the future.
	 * Also, the question will arise if we should also replace calls to standard
	 * functions if the input parameters are all literals (e.g. ADD(42, 42)). This
	 * means this class will be more difficult than it appears at first.
	 */
	symbol_c *left_type, *right_type;

	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			left_type = symbol->l_exp->candidate_datatypes[i];
			right_type = symbol->r_exp->candidate_datatypes[j];
			if (is_type_equal(left_type, right_type) && is_ANY_NUM_compatible(left_type))
				symbol->candidate_datatypes.push_back(left_type);
			else {
				symbol_c *result = widening_conversion(left_type, right_type, widen_ADD_table);
				if (result)
					symbol->candidate_datatypes.push_back(result);
			}
		}
	}
	if (debug) std::cout <<  "+ [" << symbol->l_exp->candidate_datatypes.size() << "," << symbol->r_exp->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	return NULL;
}


void *fill_candidate_datatypes_c::visit(sub_expression_c *symbol) {
	symbol_c *left_type, *right_type;

	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			left_type = symbol->l_exp->candidate_datatypes[i];
			right_type = symbol->r_exp->candidate_datatypes[j];
			if (is_type_equal(left_type, right_type) && is_ANY_NUM_compatible(left_type))
				symbol->candidate_datatypes.push_back(left_type);
			else {
				symbol_c *result = widening_conversion(left_type, right_type, widen_SUB_table);
				if (result)
					symbol->candidate_datatypes.push_back(result);
			}
		}
	}
	if (debug) std::cout <<  "- [" << symbol->l_exp->candidate_datatypes.size() << "," << symbol->r_exp->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	return NULL;
}


void *fill_candidate_datatypes_c::visit(mul_expression_c *symbol) {
	symbol_c *left_type, *right_type;

	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			left_type = symbol->l_exp->candidate_datatypes[i];
			right_type = symbol->r_exp->candidate_datatypes[j];
			if      (is_type_equal(left_type, right_type) && is_ANY_NUM_compatible(left_type))
				symbol->candidate_datatypes.push_back(left_type);
			else {
				symbol_c *result = widening_conversion(left_type, right_type, widen_MUL_table);
				if (result)
					symbol->candidate_datatypes.push_back(result);
			}

		}
	}
	if (debug) std::cout << "* [" << symbol->l_exp->candidate_datatypes.size() << "," << symbol->r_exp->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";

	return NULL;
}

void *fill_candidate_datatypes_c::visit(div_expression_c *symbol) {
	symbol_c *left_type, *right_type;

	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			left_type = symbol->l_exp->candidate_datatypes[i];
			right_type = symbol->r_exp->candidate_datatypes[j];
			if      (is_type_equal(left_type, right_type) && is_ANY_NUM_type(left_type))
				symbol->candidate_datatypes.push_back(left_type);
			else {
				symbol_c *result = widening_conversion(left_type, right_type, widen_DIV_table);
				if (result)
					symbol->candidate_datatypes.push_back(result);
			}

		}
	}
	if (debug) std::cout << "/ [" << symbol->l_exp->candidate_datatypes.size() << "," << symbol->r_exp->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	return NULL;
}


void *fill_candidate_datatypes_c::visit(mod_expression_c *symbol) {
	symbol_c *left_type, *right_type;

	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	for (unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			left_type = symbol->l_exp->candidate_datatypes[i];
			right_type = symbol->r_exp->candidate_datatypes[j];
			if (is_type_equal(left_type, right_type) && is_ANY_INT_compatible(left_type))
				symbol->candidate_datatypes.push_back(left_type);
		}
	}
	if (debug) std::cout << "mod [" << symbol->l_exp->candidate_datatypes.size() << "," << symbol->r_exp->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	return NULL;
}


void *fill_candidate_datatypes_c::visit(power_expression_c *symbol) {
	symbol_c *left_type, *right_type;
	bool check_ok;

	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	check_ok = false;
	for (unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		left_type = symbol->l_exp->candidate_datatypes[i];
		if (is_ANY_REAL_compatible(left_type)) {
			check_ok = true;
			break;
		}
	}
	if (! check_ok) return NULL;
	check_ok = false;
	for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
		right_type = symbol->r_exp->candidate_datatypes[j];
		if (is_ANY_NUM_compatible(right_type)) {
			check_ok = true;
			break;
		}
	}
	if (! check_ok) return NULL;
	for (unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		symbol->candidate_datatypes.push_back(symbol->l_exp->candidate_datatypes[i]);
	}
	if (debug) std::cout << "** [" << symbol->l_exp->candidate_datatypes.size() << "," << symbol->r_exp->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	return NULL;
}


void *fill_candidate_datatypes_c::visit(neg_expression_c *symbol) {
	symbol->exp->accept(*this);
	for (unsigned int i = 0; i < symbol->exp->candidate_datatypes.size(); i++) {
		if (is_ANY_MAGNITUDE_compatible(symbol->exp->candidate_datatypes[i]))
			symbol->candidate_datatypes.push_back(symbol->exp->candidate_datatypes[i]);
	}
	if (debug) std::cout << "neg [" << symbol->exp->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	return NULL;
}


void *fill_candidate_datatypes_c::visit(not_expression_c *symbol) {
	symbol->exp->accept(*this);
	for (unsigned int i = 0; i < symbol->exp->candidate_datatypes.size(); i++) {
		if      (is_ANY_BIT_compatible(symbol->exp->candidate_datatypes[i]))
			symbol->candidate_datatypes.push_back(symbol->exp->candidate_datatypes[i]);
	}
	if (debug) std::cout << "not [" << symbol->exp->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	return NULL;
}


void *fill_candidate_datatypes_c::visit(function_invocation_c *symbol) {
	function_declaration_c *f_decl;
	list_c *parameter_list;
	list_c *parameter_candidate_datatypes;
	symbol_c *parameter_type;
	int error_count;
	function_symtable_t::iterator lower = function_symtable.lower_bound(symbol->function_name);
	function_symtable_t::iterator upper = function_symtable.upper_bound(symbol->function_name);

	if (NULL != symbol->formal_param_list)
		parameter_list = (list_c *)symbol->formal_param_list;
	else if (NULL != symbol->nonformal_param_list)
		parameter_list = (list_c *)symbol->nonformal_param_list;
	else ERROR;
	if (debug) std::cout << "function()\n";
	parameter_list->accept(*this);
	for(; lower != upper; lower++) {
		f_decl = function_symtable.get_value(lower);
		error_count = 0;
		/* Check if function declaration in symbol_table is compatible with parameters */
		if (NULL != symbol->nonformal_param_list)
			/* nonformal parameter function call */
			match_nonformal_call(symbol, f_decl, &error_count);
		else
			/* formal parameter function call */
			match_formal_call (symbol, f_decl, &error_count);
		if (0 == error_count) {
			/* Add basetype matching function only if not present */
			unsigned int k;
			parameter_type = base_type(f_decl->type_name);
			for(k = 0; k < symbol->candidate_datatypes.size(); k++) {
				if (is_type_equal(parameter_type, symbol->candidate_datatypes[k]))
					break;
			}
			if (k >= symbol->candidate_datatypes.size())
				symbol->candidate_datatypes.push_back(parameter_type);
		}
	}
	if (debug) std::cout << "end_function() [" << symbol->candidate_datatypes.size() << "] result.\n";
	return NULL;
}

/********************/
/* B 3.2 Statements */
/********************/
// SYM_LIST(statement_list_c)
/* The visitor of the base class search_visitor_c will handle calling each instruction in the list.
 * We do not need to do anything here...
 */
// void *fill_candidate_datatypes_c::visit(statement_list_c *symbol)


/*********************************/
/* B 3.2.1 Assignment Statements */
/*********************************/
void *fill_candidate_datatypes_c::visit(assignment_statement_c *symbol) {
	symbol_c *left_type, *right_type;

	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	for (unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			left_type = symbol->l_exp->candidate_datatypes[i];
			right_type = symbol->r_exp->candidate_datatypes[j];
			if (is_type_equal(left_type, right_type))
				symbol->candidate_datatypes.push_back(left_type);
		}
	}
	if (debug) std::cout << ":= [" << symbol->l_exp->candidate_datatypes.size() << "," << symbol->r_exp->candidate_datatypes.size() << "] ==> "  << symbol->candidate_datatypes.size() << " result.\n";
	return NULL;
}



/********************************/
/* B 3.2.3 Selection Statements */
/********************************/
void *fill_candidate_datatypes_c::visit(if_statement_c *symbol) {
	/* MANU:
	 * IF statement accept only BOOL type. We intersect with BOOL type to validate current if condition
	 * Example:
	 * IF 1 THEN 		---> 	 ok
	 * IF 5 THEN 		---> not ok
	 * IF 1 OR 1 THEN	--->     ok
	 * IF 1 OR 5 THEN   ---> not ok
	 * IF SHL() THEN	---> 	 ok if shl return BOOL
	 * IF INT_TO_REAL() ---> not ok
	 */
	symbol->expression->accept(*this);
	if (NULL != symbol->statement_list)
		symbol->statement_list->accept(*this);
	if (NULL != symbol->elseif_statement_list)
		symbol->elseif_statement_list->accept(*this);
	if (NULL != symbol->else_statement_list)
		symbol->else_statement_list->accept(*this);
	return NULL;
}


void *fill_candidate_datatypes_c::visit(elseif_statement_c *symbol) {
	symbol->expression->accept(*this);
	if (NULL != symbol->statement_list)
		symbol->statement_list->accept(*this);
	return NULL;
}

/* CASE expression OF case_element_list ELSE statement_list END_CASE */
// SYM_REF3(case_statement_c, expression, case_element_list, statement_list)
void *fill_candidate_datatypes_c::visit(case_statement_c *symbol) {
	symbol->expression->accept(*this);
	if (NULL != symbol->case_element_list)
		symbol->case_element_list->accept(*this);
	if (NULL != symbol->statement_list)
		symbol->statement_list->accept(*this);
	return NULL;
}


/* helper symbol for case_statement */
// SYM_LIST(case_element_list_c)
/* NOTE: visitor method for case_element_list_c is not required since we inherit from iterator_visitor_c */

/*  case_list ':' statement_list */
// SYM_REF2(case_element_c, case_list, statement_list)
/* NOTE: visitor method for case_element_c is not required since we inherit from iterator_visitor_c */

// SYM_LIST(case_list_c)
/* NOTE: visitor method for case_list_c is not required since we inherit from iterator_visitor_c */

/********************************/
/* B 3.2.4 Iteration Statements */
/********************************/

void *fill_candidate_datatypes_c::visit(for_statement_c *symbol) {
	symbol->control_variable->accept(*this);
	symbol->beg_expression->accept(*this);
	symbol->end_expression->accept(*this);
	if (NULL != symbol->by_expression)
		symbol->by_expression->accept(*this);
	if (NULL != symbol->statement_list)
		symbol->statement_list->accept(*this);
	return NULL;
}


void *fill_candidate_datatypes_c::visit(while_statement_c *symbol) {
	symbol->expression->accept(*this);
	if (NULL != symbol->statement_list)
		symbol->statement_list->accept(*this);
	return NULL;
}


void *fill_candidate_datatypes_c::visit(repeat_statement_c *symbol) {
	symbol->expression->accept(*this);
	if (NULL != symbol->statement_list)
		symbol->statement_list->accept(*this);
	return NULL;
}





