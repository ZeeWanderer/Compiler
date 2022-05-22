// Compiler.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
// TODO: implement variable scopes for codegen
// TODO: ForExprAST implement types
//
// TODO: nothrow error handling
// TODO: parse funnction arg types
// TODO: More operators
// TODO: 1+ char length operators
// TODO: Branching: Broaden if-else, switch statement;
// TODO: Broaden types

#include "pch.h"
#include "Tokenizer.h"
#include "Context.h"
#include "Layout.h"
#include "Program.hpp"

#include <taskflow/taskflow.hpp>

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
};

const std::string source_code = R"(
uint64 main()
{
	double left = start_0;
	double right = start_1;

	for(int64 idx = 0; idx < iter; idx = idx + 1)
	{
		double tmp = right + left;
		left = right;
		right = tmp;
	}

	uint64 result = right;

	return result;
}
)";

//double tmp = right;
//right      = right + left;
//left       = tmp;

int main(int argc, char** argv)
{

	Context m_context;

	Layout m_layout;
	m_layout.addMember("iter", ::Kdouble, offsetof(Data, iter));
	//m_layout.addConsatant("v", 5.0, ::Kdouble);
	//m_layout.addConsatant("vi", 14.0, ::Kint64);
	m_layout.addMember("start_0", ::Kdouble, offsetof(Data, start_0));
	m_layout.addMember("start_1", ::Kdouble, offsetof(Data, start_1));

	Program<Data, uint64_t> m_program(m_context, m_layout, source_code);

	auto err   = m_program.compile(CompileOptions(), true);
	if (err)
	{
		logAllUnhandledErrors(std::move(err), errs());
		return 0;
	}

	std::cout << std::endl; // new line after IR dump

	Data data{3.0, 0, 1 /*, 10.0, 10.0*/};

	auto retval = m_program.run(&data);

	std::cout << "retval = " << retval << std::endl;

#if _DEBUG
	return 0; // cut off testing code
#endif
	{
		auto lambda = [&m_context, &m_layout]()
		{
			Program<Data, uint64_t> m_program(m_context, m_layout, source_code);
			auto err = m_program.compile();
			if (err)
				return;
		};

		auto begin = std::chrono::steady_clock::now();
		lambda();
		auto end = std::chrono::steady_clock::now();
		std::cout << std::endl; // new line after IR dump

		auto compile_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

		std::cout << "sync 1 compile_time = " << compile_time.count() << "[ms]" << std::endl;

		begin       = std::chrono::steady_clock::now();
		auto retval = m_program.run(&data);
		end         = std::chrono::steady_clock::now();

		auto run_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
		std::cout << "run_time 1 = " << run_time.count() << "[ns]" << std::endl;

		begin = std::chrono::steady_clock::now();
		for (uint64_t idx = 0; idx < 1000; idx++)
		{
			auto retval = m_program.run(&data);
		}
		end = std::chrono::steady_clock::now();

		run_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
		std::cout << "run_time 1000 = " << run_time.count() << "[ns]" << std::endl;

		const auto hw_c = std::thread::hardware_concurrency();
		std::cout << "CPUtm: " << hw_c / 2 << "C" << hw_c << "T" << std::endl;

		begin = std::chrono::steady_clock::now();
		for (int idx = 0; idx < 100; idx++)
		{
			lambda();
		}
		end = std::chrono::steady_clock::now();

		compile_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

		std::cout << "sync 100 compile_time = " << compile_time.count() << "[ms]" << std::endl;

		tf::Executor executor;
		tf::Taskflow taskflow;

		for (int idx = 0; idx < 100; idx++)
		{
			taskflow.emplace(lambda);
		}

		begin = std::chrono::steady_clock::now();
		executor.run(taskflow).wait();
		end = std::chrono::steady_clock::now();

		compile_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

		std::cout << "async 100 compile_time = " << compile_time.count() << "[ms]" << std::endl;
	}

	//	unsigned char* ptr = reinterpret_cast<unsigned char*>(m_program.main_func);

	//	auto tmp_str = hexStr(ptr, 150);
	//	printf("%s",tmp_str.c_str());

	return 0;
}
