#pragma once
#include <string>
#include <set>
#include <map>

namespace slljit
{
	using namespace std;

	//===----------------------------------------------------------------------===//
	// Lexer
	//===----------------------------------------------------------------------===//

	/**
	 * @enum slljit::Token
	 * @brief Token representation. All values < 0.
	 */
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

		static const set<char> identifier_start_charset;
		static const set<char> identifier_charset;
		static const map<string, Token> reserved_identifier_set;

		inline int _getchar();

		static inline pair<bool, Token> is_reserved_command_id(string c);

		static inline bool is_id_start_char(int c);

		static inline bool is_id_char(int c);

		int LastChar = ' ';

	public:
		void set_source(std::string_view source);

		/**
		 * @brief get next token from source code.
		 * @return an integer representation of a token. Either a Token or a character.
		 * @ref slljit::Token
		 */
		int gettok();

		std::string get_identifier();

		std::string get_type_identifier();

		double get_double_val();
	};
}; // namespace slljit
