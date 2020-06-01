#include "pch.h"
#include "Tokenizer.h"

using namespace llvm;
using namespace llvm::orc;
using namespace slljit;
using namespace std;

namespace slljit
{
	int Tokenizer::_getchar()
	{
		static size_t source_idx = 0;
		if (source_idx < source_code.size())
		{
			return source_code[source_idx++];
		}
		else
		{
			return EOF;
		}
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

		if (isalpha(LastChar))
		{ // identifier: [a-zA-Z][a-zA-Z0-9]*
			IdentifierStr = LastChar;
			while (isalnum((LastChar = _getchar())))
				IdentifierStr += LastChar;

			// BASIC TYPES
			if (IdentifierStr == "double")
			{
				TypeIdentifierStr = IdentifierStr;
				return tok_type;
			}

			// COMMANDS
			if (IdentifierStr == "extern")
				return tok_extern;
			if (IdentifierStr == "if")
				return tok_if;
			if (IdentifierStr == "else")
				return tok_else;
			if (IdentifierStr == "for")
				return tok_for;
			return tok_identifier;
		}

		if (isdigit(LastChar) || LastChar == '.')
		{ // Number: [0-9.]+
			std::string NumStr;
			do
			{
				NumStr += LastChar;
				LastChar = _getchar();
			} while (isdigit(LastChar) || LastChar == '.');

			//	NumVal = strtod(NumStr.c_str(), nullptr);
			std::from_chars(NumStr.c_str(), NumStr.c_str() + NumStr.size(), NumVal);
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
		LastChar     = _getchar();
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

	double Tokenizer::get_double_val()
	{
		return NumVal;
	}
}; // namespace slljit