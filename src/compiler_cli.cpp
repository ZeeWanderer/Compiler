﻿// Compiler.cpp : This file contains the 'main' function. Program execution begins and ends there.
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
	{
		auto tmp = (int)data[i];
		ss << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
	}

	return ss.str();
}

struct Data
{
	double iter;
	double start_0;
	double start_1;
	int64_t Ni;
};

int main(int argc, char** argv)
{
	Context m_context;
	Program<Data> m_program(m_context);
	Layout m_layout;
	m_layout.addMember("N", ::Kdouble, offsetof(Data, iter));
	m_layout.addMember("Ni", ::Kint64, offsetof(Data, Ni));
	m_layout.addConsatant("v", 5.0, ::Kdouble);
	m_layout.addConsatant("vi", 14.0, ::Kint64);
	//	m_layout.addMember("start_0", ::Kdouble, offsetof(Data, start_0));
	//	m_layout.addMember("start_1", ::Kdouble, offsetof(Data, start_1));
	//	m_layout.addConsatant("v", 5, ::Kdouble);

	const std::string source_code = R"(
	double main()
	{
		int64 test = Ni;
		double left = 0;
		double right = 1;

		test = right;

		int64 test_1 = test + vi;
		int64 test_2 = test + test_1 + v;

		if(N < 2)
		{
			return N;
		}

		for(double idx = 0; idx < N - 1; idx = idx + 1)
		{
			double tmp = right + left;
			left = right;
			right = tmp;
		}

		return right;
	}
)";

	auto begin = std::chrono::steady_clock::now();
	m_program.compile(source_code, m_layout);
	auto end = std::chrono::steady_clock::now();

	const auto compile_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

	std::cout << "compile_time = " << compile_time.count() << "[ms]" << std::endl;

	Data data{10.0, 10.0, 10.0, 15};

	begin       = std::chrono::steady_clock::now();
	auto retval = m_program.run(&data);
	end         = std::chrono::steady_clock::now();

	const auto run_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);

	std::cout << "run_time = " << run_time.count() << "[ns]" << std::endl;

	//	unsigned char* ptr = reinterpret_cast<unsigned char*>(m_program.main_func);

	//	auto tmp_str = hexStr(ptr, 150);
	//	printf("%s",tmp_str.c_str());

	return 0;
}
