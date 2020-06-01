#pragma once
#include <string>

namespace slljit
{
	using namespace std;

	//===----------------------------------------------------------------------===//
	// Lexer
	//===----------------------------------------------------------------------===//

	// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
	// of these for known things.
	enum Token : int
	{
		tok_eof = -1,

		// commands
		tok_type   = -2,
		tok_extern = -3,

		// primary
		tok_identifier = -4,
		tok_number     = -5,

		// control
		tok_if   = -6,
		tok_else = -8,
		tok_for  = -9,
	};

	class Tokenizer
	{
	protected:
		std::string IdentifierStr;     // Filled in if tok_identifier
		std::string TypeIdentifierStr; // Filled in if tok_type
		double NumVal;                 // Filled in if tok_number
		std::string_view source_code;

		int _getchar();

		int LastChar = ' ';
		/// gettok - Return the next token from standard input.
	public:
		void set_source(std::string_view source);

		int gettok();

		std::string get_identifier();

		std::string get_type_identifier();

		double get_double_val();
	};

}; // namespace slljit