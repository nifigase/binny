//============================================================================
// Name        : test.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <assert.h>
#include <execinfo.h>
#include <iostream>

#include "binny.hpp"

using std::cout;
using std::endl;

void print_trace() {
	void *array[50];
	size_t size;

	// get void*'s for all entries on the stack
	size = ::backtrace(array, 50);

	// print out all the frames to stderr
	std::cerr << "Stacktrace:" << endl;
	::backtrace_symbols_fd(array, size, 2);
}

void test_binny_simple() {
	unsigned int acc1 = 0;

//	11101000
	unsigned char buf[1] = {0xe8};
	try {
		binny::parse(
			binny::c_bin_data<11>() |
			binny::nonconst_data<unsigned int>(acc1, 3) |
			binny::zeros<3>(),
			buf
		);
	} catch (const binny::bin_match_error& e) {
		std::cerr << e.what();
		print_trace();
	}
	std::cout << "acc1 = " << acc1 << std::endl; //must be 5
	assert(acc1 == 5);
}

#ifdef TEST_CPP0X

void test_binny_2el() {
	unsigned int acc1 = 0;

	unsigned char buf[2] = {0x1, 0xe8};

	auto format = binny::zeros<7>() |
			binny::c_bin_data<111>() |
			binny::nonconst(acc1, 3) |
			binny::c_bin_data<000>();

	try {
		binny::parse(format, buf);
	} catch (const binny::bin_match_error& e) {
		std::cerr << e.what();
		print_trace();
	}

	std::cout << "acc1 = " << acc1 << std::endl; //must be 101 = 5
	assert(acc1 == 5);
}

#else

void test_binny_2el() {
	unsigned int acc1 = 0;

	unsigned char buf[2] = {0x1, 0xe8};
	try {
		binny::parse(
			binny::zeros<7>() |
			binny::c_bin_data<111>() |
//			binny::nonconst_data<unsigned int>(acc1, 3) |
			binny::nonconst(acc1, 3) |
			binny::c_bin_data<000>(),
			buf
		);
	} catch (const binny::bin_match_error& e) {
		std::cerr << e.what();
		print_trace();
	}

	std::cout << "acc1 = " << acc1 << std::endl; //must be 101 = 5
	assert(acc1 == 5);
}
#endif

int main() {
	test_binny_simple();
	test_binny_2el();

    return 0;
}
