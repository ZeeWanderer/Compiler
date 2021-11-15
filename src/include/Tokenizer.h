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

	//TODO: fix line_count and line_char_count counting
	class Tokenizer
	{
	protected:
		std::string IdentifierStr;     // Filled in if tok_identifier
		std::string TypeIdentifierStr; // Filled in if tok_type
		std::string NumberStr;         // Filled in if tok_number

		double NumVal;                 // Filled in if tok_number
		std::string_view source_code;

		size_t source_idx = 0;

		size_t line_count = 1;
		size_t line_char_count = 0;

		pair<size_t, size_t> tok_location = {-1,-1};

		static const set<char> identifier_start_charset;
		static const set<char> identifier_charset;
		static const map<string, Token> reserved_identifier_set;

		inline int _getchar();

		inline void set_lok_location();

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

		// line, character
		std::pair<size_t, size_t> get_source_location() const;

		std::string get_identifier();

		std::string get_type_identifier();

		std::string get_number_string();
	};
}; // namespace slljit
