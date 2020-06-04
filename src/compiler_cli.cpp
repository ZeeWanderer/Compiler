// Compiler.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
// TODO: nothrow error handling
// TODO: parse funnction arg types
// TODO: More operators
// TODO: 1+ char length operators
// TODO: Branching: if-else,for loop;
// TODO: 1+ char length operators
// TODO: Actully use types(Cast, conversion etc.)
// TODO: convert to OOP in mpsl image.
// TODO: layout as in mpsl
// TODO: remove delayed jit

#include "pch.h"
#include "Tokenizer.h"
#include "Context.h"
#include "Layout.h"
#include "Program.hpp"

using namespace slljit;
namespace slljit
{
	using namespace llvm;
	using namespace llvm::orc;
	using namespace std;

	//===----------------------------------------------------------------------===//
	// Code Generation
	//===----------------------------------------------------------------------===//
}; // namespace slljit

	//===----------------------------------------------------------------------===//
	// "Library" functions that can be "extern'd" from user code.
	//===----------------------------------------------------------------------===//

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X)
{
	fputc((char)X, stderr);
	return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X)
{
	fprintf(stderr, "%f\n", X);
	return 0;
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//
#include <sstream>
#include <iomanip>
#include <iostream>

std::string hexStr(unsigned char* data, int len)
{
	std::stringstream ss;
	ss << std::hex;

	for (int i(0); i < len; ++i)
		ss << std::setw(2) << std::setfill('0') << (int)data[i];

	return ss.str();
}

struct Data
{
	double x;
	double y;
};



int main()
{
	Context m_context;
	Program<Data> m_program(m_context);
	Layout m_layout;
	m_layout.addMember("x", ::Kdouble, offsetof(Data, x));
	m_layout.addMember("y", ::Kdouble, offsetof(Data, y));
	m_layout.addConsatant("v", 5, ::Kdouble);

	std::string source_code = R"(
	extern putchard(double x);
	extern printd(double x);
	double max(double left, double right)
	{
		if(left > right)
		{
			return left;
		}
		return right;
	}
	double main()
	{
		double a = x+v*2;
		double b = x*v*2 + a;
		double d = x+v+2 + b;
		double e = x - v + 2 + d;
		for(double idx0 = 0; idx0<400; idx0 = idx0+1)
		{
			e = e + 2;
		}
		for(; idx0<801; idx0 = idx0+2)
		{
			e = e + 2;
		}
		x = 4;
		double h_ = x;
		printd(h_);
		printd(h_);
		printd(h_);
		double t = max(a, e);
		x = 5/2;
		y = 5;
		return max(t, 3);
	}
)";
	m_program.compile(source_code, m_layout);

	Data data{3.0, 1.0};

	auto retval = m_program.run(&data);

	return 0;
}
