// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/string.h"
#include "mordor/string_conv.h"
#include "mordor/test/test.h"
#include <stddef.h>

using namespace Mordor;

MORDOR_UNITTEST(String, dataFromHexstring)
{
    MORDOR_TEST_ASSERT_EQUAL(dataFromHexstring(""), "");
    MORDOR_TEST_ASSERT_EQUAL(dataFromHexstring("00"), std::string("\0", 1));
    MORDOR_TEST_ASSERT_EQUAL(dataFromHexstring("abcd"), "\xab\xcd");
    MORDOR_TEST_ASSERT_EQUAL(dataFromHexstring("01eF"), "\x01\xef");
    MORDOR_TEST_ASSERT_EXCEPTION(dataFromHexstring("0"),
        std::invalid_argument);
    MORDOR_TEST_ASSERT_EXCEPTION(dataFromHexstring("fg"),
        std::invalid_argument);
    MORDOR_TEST_ASSERT_EXCEPTION(dataFromHexstring("fG"),
        std::invalid_argument);
    MORDOR_TEST_ASSERT_EXCEPTION(dataFromHexstring(std::string("\0\0", 2)),
        std::invalid_argument);
}

MORDOR_UNITTEST(String, sha0sum)
{
    MORDOR_TEST_ASSERT_EQUAL(hexstringFromData(sha0sum("")), "f96cea198ad1dd5617ac084a3d92c6107708c0ef");
    MORDOR_TEST_ASSERT_EQUAL(hexstringFromData(sha0sum("1234567890")), "786abc00fc4c0ab7ea5f0f2bd85fb9ab00c2ad82");
    MORDOR_TEST_ASSERT_EQUAL(hexstringFromData(sha0sum((const void *)"\x7e\x54\xe4\xbc\x27\x00\x40\xab", 8)), "ea1d7982eb4c6201498ece16539ce174735b6a21");
}

MORDOR_UNITTEST(StringConv, toString)
{
    MORDOR_TEST_ASSERT_EQUAL(StringNativeConv<int>::toString(1234567), "1234567");
    MORDOR_TEST_ASSERT_EQUAL(StringNativeConv<int>::toString(-1234567), "-1234567");
    MORDOR_TEST_ASSERT_EQUAL(StringNativeConv<bool>::toString(true), "true");
    MORDOR_TEST_ASSERT_EQUAL(StringNativeConv<bool>::toString(false), "false");
    MORDOR_TEST_ASSERT_EQUAL(StringNativeConv<size_t>::toString(9999999), "9999999");
    MORDOR_TEST_ASSERT_EQUAL(StringNativeConv<std::string>::toString("hello,world"), "hello,world");

}


MORDOR_UNITTEST(StringConv, fromString)
{
    MORDOR_TEST_ASSERT_EQUAL(StringNativeConv<int>::fromString("1234567"), 1234567);
    MORDOR_TEST_ASSERT_EQUAL(StringNativeConv<int>::fromString("-1234567"), -1234567);
    MORDOR_TEST_ASSERT_EQUAL(StringNativeConv<bool>::fromString("true"), true);
    MORDOR_TEST_ASSERT_EQUAL(StringNativeConv<bool>::fromString("false"), false);
    MORDOR_TEST_ASSERT_EQUAL(StringNativeConv<size_t>::fromString("9999999"), (size_t)9999999);
    MORDOR_TEST_ASSERT_EQUAL(StringNativeConv<std::string>::fromString("hello,world"), "hello,world");

    MORDOR_TEST_ASSERT_EXCEPTION(StringNativeConv<int>::fromString("bool"), std::invalid_argument);
    MORDOR_TEST_ASSERT_EXCEPTION(StringNativeConv<unsigned long int>::fromString("hello,world"), std::invalid_argument);
}
