#pragma once

#include "llvm/Support/Error.h"
#include <system_error>

namespace slljit
{
	using namespace llvm;

	/// Used to represent Parser errors.
	class ParserError : public ErrorInfo<ParserError>
	{

	public:
		static char ID;

		ParserError(const Twine& S, std::errc EC = std::errc::interrupted);

		ParserError(const Twine& S, std::pair<size_t, size_t> location, std::errc EC = std::errc::interrupted);

		void log(raw_ostream& OS) const override;

		std::error_code convertToErrorCode() const override;

	private:
		// line, character
		std::pair<size_t, size_t> location;
		std::string Msg;
		std::error_code EC;
	};

	/// Used to repseresnt CodeGen and compile() errors.
	class CompileError : public ErrorInfo<CompileError>
	{

	public:
		static char ID;

		CompileError(const Twine& S, std::errc EC = std::errc::interrupted);

		void log(raw_ostream& OS) const override;

		std::error_code convertToErrorCode() const override;

	private:
		// line, character
		std::string Msg;
		std::error_code EC;
	};

}; // namespace slljit