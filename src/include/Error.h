#include "llvm/Support/Error.h"
#include <system_error>

namespace slljit
{
	using namespace llvm;

	class ParserError : public ErrorInfo<ParserError>
	{

	public:
		static char ID;

		ParserError(const Twine& S)
		    : Msg(S.str()), location(-1,-1)
		{
			EC = std::make_error_code(std::errc::interrupted);
		}

		ParserError(const Twine& S, std::pair<size_t, size_t> location)
		    : Msg(S.str()), location(location)
		{
			EC = std::make_error_code(std::errc::interrupted);
		}

		void log(raw_ostream& OS) const override
		{
			if (location == std::pair<size_t, size_t>{-1, -1})
			{
				OS << "error: " << Msg;
			}
			else
			{
				OS << "(" << location.first << "," << location.second << "): error: " << Msg;
			}
			
		}

		std::error_code convertToErrorCode() const override
		{
			return EC;
		}

	private:
		// line, character
		std::pair<size_t, size_t> location;
		std::string Msg;
		std::error_code EC;
	};

}; // namespace slljit