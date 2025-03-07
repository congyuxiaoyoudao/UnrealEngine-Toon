// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PreprocessorHelpers.h"

#include "Tests/TestHarnessAdapter.h"

#if WITH_TESTS

TEST_CASE_NAMED(FPreprocessorHelperArgCountTest, "System::Core::HAL::UE_VA_ARG_COUNT", "[ApplicationContextMask][EngineFilter]")
{
	STATIC_CHECK(UE_VA_ARG_COUNT() == 0);
	STATIC_CHECK(UE_VA_ARG_COUNT(a) == 1);
	STATIC_CHECK(UE_VA_ARG_COUNT(a, b) == 2);
	STATIC_CHECK(UE_VA_ARG_COUNT(a, b, c) == 3);
	STATIC_CHECK(UE_VA_ARG_COUNT(a, b, c, d) == 4);
	STATIC_CHECK(UE_VA_ARG_COUNT(a, b, c, d, e) == 5);
	STATIC_CHECK(UE_VA_ARG_COUNT(a, b, c, d, e, f) == 6);
	STATIC_CHECK(UE_VA_ARG_COUNT(a, b, c, d, e, f, g) == 7);
	STATIC_CHECK(UE_VA_ARG_COUNT(a, b, c, d, e, f, g, h) == 8);
	STATIC_CHECK(UE_VA_ARG_COUNT(a, b, c, d, e, f, g, h, i) == 9);
	STATIC_CHECK(UE_VA_ARG_COUNT(a, b, c, d, e, f, g, h, i, j) == 10);
}

TEST_CASE_NAMED(FPreprocessorHelperArgAppendCountTest, "System::Core::HAL::UE_APPEND_VA_ARG_COUNT", "[ApplicationContextMask][EngineFilter]")
{
	#define TESTPREFIX_0 22
	#define TESTPREFIX_1 32
	#define TESTPREFIX_2 41
	#define TESTPREFIX_3 52
	#define TESTPREFIX_4 61
	#define TESTPREFIX_5 70
	#define TESTPREFIX_6 77
	#define TESTPREFIX_7 82
	#define TESTPREFIX_8 89
	#define TESTPREFIX_9 90
	#define TESTPREFIX_10 96

	STATIC_CHECK(UE_APPEND_VA_ARG_COUNT(TESTPREFIX_) == TESTPREFIX_0);
	STATIC_CHECK(UE_APPEND_VA_ARG_COUNT(TESTPREFIX_, a) == TESTPREFIX_1);
	STATIC_CHECK(UE_APPEND_VA_ARG_COUNT(TESTPREFIX_, a, b) == TESTPREFIX_2);
	STATIC_CHECK(UE_APPEND_VA_ARG_COUNT(TESTPREFIX_, a, b, c) == TESTPREFIX_3);
	STATIC_CHECK(UE_APPEND_VA_ARG_COUNT(TESTPREFIX_, a, b, c, d) == TESTPREFIX_4);
	STATIC_CHECK(UE_APPEND_VA_ARG_COUNT(TESTPREFIX_, a, b, c, d, e) == TESTPREFIX_5);
	STATIC_CHECK(UE_APPEND_VA_ARG_COUNT(TESTPREFIX_, a, b, c, d, e, f) == TESTPREFIX_6);
	STATIC_CHECK(UE_APPEND_VA_ARG_COUNT(TESTPREFIX_, a, b, c, d, e, f, g) == TESTPREFIX_7);
	STATIC_CHECK(UE_APPEND_VA_ARG_COUNT(TESTPREFIX_, a, b, c, d, e, f, g, h) == TESTPREFIX_8);
	STATIC_CHECK(UE_APPEND_VA_ARG_COUNT(TESTPREFIX_, a, b, c, d, e, f, g, h, i) == TESTPREFIX_9);
	STATIC_CHECK(UE_APPEND_VA_ARG_COUNT(TESTPREFIX_, a, b, c, d, e, f, g, h, i, j) == TESTPREFIX_10);
}

#endif //WITH_TESTS
