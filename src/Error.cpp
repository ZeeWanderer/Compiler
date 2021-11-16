#include "pch.h"
#include "Error.h"

namespace slljit
{
	char ParserError::ID;

	ParserError::ParserError(const Twine& S, std::errc EC)
	    : Msg(S.str()), location(-1, -1), EC(std::make_error_code(EC))
	{
	}

	ParserError::ParserError(const Twine& S, std::pair<size_t, size_t> location, std::errc EC)
	    : Msg(S.str()), location(location), EC(std::make_error_code(EC))
	{
	}

	void ParserError::log(raw_ostream& OS) const
	{
		if (location == std::pair<size_t, size_t>{-1, -1})
		{
			OS << "error: " << Msg;
		}
		else
		{
			OS << "(" << location.first << "," << location.second << "): error: " << Msg;
		}

		if (EC != std::errc::interrupted)
		{
			OS << " error_code: " << EC.message();
		}
	}

	std::error_code ParserError::convertToErrorCode() const
	{
		return EC;
	}
}; // namespace slljit