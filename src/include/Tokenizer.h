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

	//! Token enum.
	/*! Token representation. All values < 0. */
	enum Token : int
	{
		tok_eof = -1, /*!< wnd of file token */

		// commands
		tok_type   = -2, /*!< type token */
		tok_extern = -3, /*!< extern token */

		// primary
		tok_identifier = -4, /*!< inedtifier token */
		tok_literal    = -5, /*!< literal value token */

		// control
		tok_if   = -6, /*!< 'if' control command token */
		tok_else = -8, /*!< 'else' control command token */
		tok_for  = -9, /*!< 'for' control command token */
	};

	/// Used to transform source into token stream
	/**
	 * Keeps immediate token location and defining data.
	 */
	class Tokenizer
	{
	protected:
		std::string IdentifierStr;     // Filled in if tok_identifier
		std::string TypeIdentifierStr; // Filled in if tok_type
		std::string LiteralStr;        // Filled in if tok_literal

		std::string_view source_code;

		size_t source_idx = 0;

		size_t line_count      = 1;
		size_t line_char_count = 0;

		pair<size_t, size_t> tok_location = {-1, -1};

		static const set<char> identifier_start_charset;
		static const set<char> identifier_charset;
		static const map<string, Token> reserved_identifier_set;

		inline int _getchar();

		inline void set_lok_location();

		static inline bool is_special_literal(string c);

		static inline pair<bool, Token> is_reserved_command_id(string c);

		static inline bool is_id_start_char(int c);

		static inline bool is_id_char(int c);

		int LastChar = ' ';

	public:
		/**
		 * Sets source to process.
		 * @param source Source program string.
		 */
		void set_source(std::string_view source);

		/**
		 * Get next token from source code.
		 * @return an integer representation of a token. Either a Token or a character.
		 * @ref slljit::Token
		 */
		int gettok();

		/**
		 * Return current token location in source.
		 * @return Line and character number in the line. 1 based counting.
		 */
		std::pair<size_t, size_t> get_source_location() const;

		std::string get_identifier();

		std::string get_type_identifier();

		std::string get_literal_string();
	};
}; // namespace slljit
