#include "pch.h"
#include "Tokenizer.h"
#include "Types.h"

namespace slljit
{
	using namespace llvm;
	using namespace llvm::orc;
	using namespace slljit;
	using namespace std;

	const set<char> Tokenizer::identifier_start_charset = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C',
	    'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '_'};

	const set<char> Tokenizer::identifier_charset = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D',
	    'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '_'};

	const map<string, Token> Tokenizer::reserved_identifier_set = {{"extern", tok_extern}, {"if", tok_if}, {"else", tok_else}, {"for", tok_for}};

	inline int Tokenizer::_getchar()
	{
		if (source_idx < source_code.size())
		{
			const auto char_ = source_code[source_idx++];

			if (char_ != '\n')
			{
				line_char_count += 1;
			}
			else
			{
				line_count += 1;
				line_char_count = 1;
			}

			return char_;
		}
		else
		{
			return EOF;
		}
	}

	inline void Tokenizer::set_lok_location()
	{
		tok_location = {line_count, line_char_count - 1};
	}

	std::pair<size_t, size_t> Tokenizer::get_source_location() const
	{
		return tok_location;
	}

	inline pair<bool, Token> Tokenizer::is_reserved_command_id(string c)
	{
		auto it = reserved_identifier_set.find(c);
		if (it != reserved_identifier_set.end())
		{
			return {true, it->second};
		}
		return {false, tok_eof};
	}

	inline bool Tokenizer::is_id_start_char(int c)
	{
		return identifier_start_charset.contains(c);
	}

	inline bool Tokenizer::is_id_char(int c)
	{
		return identifier_charset.find(c) != identifier_charset.end();
	}

	void Tokenizer::set_source(std::string_view source)
	{
		this->source_code = source;
	}

	int Tokenizer::gettok()
	{
		// Skip any whitespace.
		while (isspace(LastChar))
			LastChar = _getchar();

		if (is_id_start_char(LastChar))
		{ // identifier: [a-zA-Z_][a-zA-Z0-9_]*
			set_lok_location();
			IdentifierStr = LastChar;
			while (is_id_char((LastChar = _getchar())))
				IdentifierStr += LastChar;

			// BASIC TYPES
			if (basic_types_identifier_set.contains(IdentifierStr))
			{
				TypeIdentifierStr = IdentifierStr;
				return tok_type;
			}

			// COMMANDS
			{
				auto [is_command, cmd_tok] = is_reserved_command_id(IdentifierStr);
				if (is_command)
					return cmd_tok;
			}

			return tok_identifier;
		}

		if (isdigit(LastChar) || LastChar == '.')
		{ // Number: [0-9.]+
			set_lok_location();
			std::string NumStr;
			do
			{
				NumStr += LastChar;
				LastChar = _getchar();
			} while (isdigit(LastChar) || LastChar == '.');

			//	NumVal = strtod(NumStr.c_str(), nullptr);
			NumberStr = NumStr;
			//std::from_chars(NumStr.c_str(), NumStr.c_str() + NumStr.size(), NumVal);
			return tok_number;
		}

		if (LastChar == '#')
		{
			// Comment until end of line.
			do
				LastChar = _getchar();
			while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

			if (LastChar != EOF)
				return gettok();
		}

		// Check for end of file.  Don't eat the EOF.
		if (LastChar == EOF)
			return tok_eof;

		// Otherwise, just return the character as its ascii value.
		int ThisChar = LastChar;
		set_lok_location();
		LastChar = _getchar();
		return ThisChar;
	}

	std::string Tokenizer::get_identifier()
	{
		return IdentifierStr;
	}

	std::string Tokenizer::get_type_identifier()
	{
		return TypeIdentifierStr;
	}

	std::string Tokenizer::get_number_string()
	{
		return NumberStr;
	}
}; // namespace slljit
