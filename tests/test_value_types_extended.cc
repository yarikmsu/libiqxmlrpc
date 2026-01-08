#define BOOST_TEST_MODULE value_types_extended_test
#include <memory>
#include <ctime>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/value.h"

using namespace boost::unit_test;
using namespace iqxmlrpc;

BOOST_AUTO_TEST_SUITE(binary_data_tests)

BOOST_AUTO_TEST_CASE(binary_from_data_simple)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("Hello"));
    BOOST_CHECK_EQUAL(bin->get_data(), "Hello");
}

BOOST_AUTO_TEST_CASE(binary_from_data_with_size)
{
    const char data[] = "Hello\0World";
    std::unique_ptr<Binary_data> bin(Binary_data::from_data(data, 11));
    BOOST_CHECK_EQUAL(bin->get_data().size(), 11u);
}

BOOST_AUTO_TEST_CASE(binary_encode_simple)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("Man"));
    BOOST_CHECK_EQUAL(bin->get_base64(), "TWFu");
}

BOOST_AUTO_TEST_CASE(binary_encode_with_padding_1)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("Ma"));
    BOOST_CHECK_EQUAL(bin->get_base64(), "TWE=");
}

BOOST_AUTO_TEST_CASE(binary_encode_with_padding_2)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("M"));
    BOOST_CHECK_EQUAL(bin->get_base64(), "TQ==");
}

BOOST_AUTO_TEST_CASE(binary_decode_simple)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("TWFu"));
    BOOST_CHECK_EQUAL(bin->get_data(), "Man");
}

BOOST_AUTO_TEST_CASE(binary_decode_with_padding_1)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("TWE="));
    BOOST_CHECK_EQUAL(bin->get_data(), "Ma");
}

BOOST_AUTO_TEST_CASE(binary_decode_with_padding_2)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("TQ=="));
    BOOST_CHECK_EQUAL(bin->get_data(), "M");
}

BOOST_AUTO_TEST_CASE(binary_roundtrip)
{
    std::string original = "Hello, World! This is a test of base64 encoding.";
    std::unique_ptr<Binary_data> bin1(Binary_data::from_data(original));
    std::string encoded = bin1->get_base64();

    std::unique_ptr<Binary_data> bin2(Binary_data::from_base64(encoded));
    BOOST_CHECK_EQUAL(bin2->get_data(), original);
}

BOOST_AUTO_TEST_CASE(binary_decode_with_whitespace)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("TW Fu\r\n"));
    BOOST_CHECK_EQUAL(bin->get_data(), "Man");
}

BOOST_AUTO_TEST_CASE(binary_malformed_base64_throws)
{
    BOOST_CHECK_THROW(Binary_data::from_base64("!!!"), Binary_data::Malformed_base64);
}

BOOST_AUTO_TEST_CASE(binary_malformed_incomplete_throws)
{
    BOOST_CHECK_THROW(Binary_data::from_base64("TWF"), Binary_data::Malformed_base64);
}

BOOST_AUTO_TEST_CASE(binary_malformed_padding_at_start_throws)
{
    BOOST_CHECK_THROW(Binary_data::from_base64("=TWF"), Binary_data::Malformed_base64);
}

BOOST_AUTO_TEST_CASE(binary_empty_data)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_data(""));
    BOOST_CHECK(bin->get_data().empty());
    BOOST_CHECK(bin->get_base64().empty());
}

BOOST_AUTO_TEST_CASE(binary_clone)
{
    std::unique_ptr<Binary_data> original(Binary_data::from_data("test data"));
    std::unique_ptr<Value_type> clone(original->clone());
    Binary_data* bin_clone = dynamic_cast<Binary_data*>(clone.get());
    BOOST_REQUIRE(bin_clone != nullptr);
    BOOST_CHECK_EQUAL(bin_clone->get_data(), original->get_data());
}

BOOST_AUTO_TEST_CASE(binary_type_name)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("test"));
    BOOST_CHECK_EQUAL(bin->type_name(), "base64");
}

BOOST_AUTO_TEST_CASE(binary_value_wrapper)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("test data"));
    Value v(*bin);
    BOOST_CHECK(v.is_binary());
    BOOST_CHECK_EQUAL(v.get_binary().get_data(), "test data");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(date_time_tests)

BOOST_AUTO_TEST_CASE(datetime_parse_valid)
{
    Date_time dt(std::string("20231225T12:30:45"));
    const struct tm& t = dt.get_tm();
    BOOST_CHECK_EQUAL(t.tm_year, 2023 - 1900);
    BOOST_CHECK_EQUAL(t.tm_mon, 11);  // December = 11
    BOOST_CHECK_EQUAL(t.tm_mday, 25);
    BOOST_CHECK_EQUAL(t.tm_hour, 12);
    BOOST_CHECK_EQUAL(t.tm_min, 30);
    BOOST_CHECK_EQUAL(t.tm_sec, 45);
}

BOOST_AUTO_TEST_CASE(datetime_parse_midnight)
{
    Date_time dt(std::string("20230101T00:00:00"));
    const struct tm& t = dt.get_tm();
    BOOST_CHECK_EQUAL(t.tm_hour, 0);
    BOOST_CHECK_EQUAL(t.tm_min, 0);
    BOOST_CHECK_EQUAL(t.tm_sec, 0);
}

BOOST_AUTO_TEST_CASE(datetime_parse_end_of_day)
{
    Date_time dt(std::string("20231231T23:59:59"));
    const struct tm& t = dt.get_tm();
    BOOST_CHECK_EQUAL(t.tm_hour, 23);
    BOOST_CHECK_EQUAL(t.tm_min, 59);
    BOOST_CHECK_EQUAL(t.tm_sec, 59);
}

BOOST_AUTO_TEST_CASE(datetime_to_string)
{
    Date_time dt(std::string("20231225T12:30:45"));
    BOOST_CHECK_EQUAL(dt.to_string(), "20231225T12:30:45");
}

BOOST_AUTO_TEST_CASE(datetime_from_tm)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = 2023 - 1900;
    t.tm_mon = 5;  // June
    t.tm_mday = 15;
    t.tm_hour = 10;
    t.tm_min = 20;
    t.tm_sec = 30;

    Date_time dt(&t);
    BOOST_CHECK_EQUAL(dt.to_string(), "20230615T10:20:30");
}

BOOST_AUTO_TEST_CASE(datetime_malformed_wrong_length_throws)
{
    BOOST_CHECK_THROW(Date_time(std::string("2023-12-25")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_malformed_no_t_separator_throws)
{
    BOOST_CHECK_THROW(Date_time(std::string("20231225 12:30:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_malformed_invalid_chars_throws)
{
    BOOST_CHECK_THROW(Date_time(std::string("2023ABCDT12:30:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_malformed_invalid_month_throws)
{
    BOOST_CHECK_THROW(Date_time(std::string("20231325T12:30:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_malformed_invalid_day_throws)
{
    BOOST_CHECK_THROW(Date_time(std::string("20231232T12:30:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_malformed_invalid_hour_throws)
{
    BOOST_CHECK_THROW(Date_time(std::string("20231225T25:30:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_malformed_invalid_minute_throws)
{
    BOOST_CHECK_THROW(Date_time(std::string("20231225T12:61:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_clone)
{
    Date_time original(std::string("20231225T12:30:45"));
    std::unique_ptr<Value_type> clone(original.clone());
    Date_time* dt_clone = dynamic_cast<Date_time*>(clone.get());
    BOOST_REQUIRE(dt_clone != nullptr);
    BOOST_CHECK_EQUAL(dt_clone->to_string(), original.to_string());
}

BOOST_AUTO_TEST_CASE(datetime_type_name)
{
    Date_time dt(std::string("20231225T12:30:45"));
    BOOST_CHECK_EQUAL(dt.type_name(), "dateTime.iso8601");
}

BOOST_AUTO_TEST_CASE(datetime_local_time)
{
    Date_time dt(true);  // local time
    const struct tm& t = dt.get_tm();
    BOOST_CHECK(t.tm_year >= 100);  // Year >= 2000
}

BOOST_AUTO_TEST_CASE(datetime_utc_time)
{
    Date_time dt(false);  // UTC time
    const struct tm& t = dt.get_tm();
    BOOST_CHECK(t.tm_year >= 100);  // Year >= 2000
}

BOOST_AUTO_TEST_CASE(datetime_value_wrapper)
{
    Date_time dt(std::string("20231225T12:30:45"));
    Value v(dt);
    BOOST_CHECK(v.is_datetime());
    BOOST_CHECK_EQUAL(v.get_datetime().to_string(), "20231225T12:30:45");
}

BOOST_AUTO_TEST_CASE(datetime_value_from_tm)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = 2023 - 1900;
    t.tm_mon = 5;
    t.tm_mday = 15;
    t.tm_hour = 10;
    t.tm_min = 20;
    t.tm_sec = 30;

    Value v(&t);
    BOOST_CHECK(v.is_datetime());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(nil_type_tests)

BOOST_AUTO_TEST_CASE(nil_construction)
{
    Nil nil;
    BOOST_CHECK_EQUAL(nil.type_name(), "nil");
}

BOOST_AUTO_TEST_CASE(nil_clone)
{
    Nil original;
    std::unique_ptr<Value_type> clone(original.clone());
    Nil* nil_clone = dynamic_cast<Nil*>(clone.get());
    BOOST_REQUIRE(nil_clone != nullptr);
}

BOOST_AUTO_TEST_CASE(nil_value_wrapper)
{
    Value v = Nil();
    BOOST_CHECK(v.is_nil());
    BOOST_CHECK_EQUAL(v.type_name(), "nil");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(value_edge_cases)

BOOST_AUTO_TEST_CASE(value_empty_string)
{
    Value v = "";
    BOOST_CHECK(v.is_string());
    BOOST_CHECK(v.get_string().empty());
}

BOOST_AUTO_TEST_CASE(value_max_int)
{
    Value v = 2147483647;
    BOOST_CHECK(v.is_int());
    BOOST_CHECK_EQUAL(v.get_int(), 2147483647);
}

BOOST_AUTO_TEST_CASE(value_min_int)
{
    Value v = static_cast<int>(-2147483647 - 1);
    BOOST_CHECK(v.is_int());
    BOOST_CHECK_EQUAL(v.get_int(), -2147483647 - 1);
}

BOOST_AUTO_TEST_CASE(value_max_int64)
{
    Value v = static_cast<int64_t>(9223372036854775807LL);
    BOOST_CHECK(v.is_int64());
    BOOST_CHECK_EQUAL(v.get_int64(), 9223372036854775807LL);
}

BOOST_AUTO_TEST_CASE(value_min_int64)
{
    int64_t min_val = -9223372036854775807LL - 1;
    Value v = min_val;
    BOOST_CHECK(v.is_int64());
    BOOST_CHECK_EQUAL(v.get_int64(), min_val);
}

BOOST_AUTO_TEST_CASE(value_double_zero)
{
    Value v = 0.0;
    BOOST_CHECK(v.is_double());
    BOOST_CHECK_EQUAL(v.get_double(), 0.0);
}

BOOST_AUTO_TEST_CASE(value_double_negative)
{
    Value v = -123.456;
    BOOST_CHECK(v.is_double());
    BOOST_CHECK_CLOSE(v.get_double(), -123.456, 0.001);
}

BOOST_AUTO_TEST_CASE(value_bool_false)
{
    Value v = false;
    BOOST_CHECK(v.is_bool());
    BOOST_CHECK_EQUAL(v.get_bool(), false);
}

BOOST_AUTO_TEST_CASE(value_bool_true)
{
    Value v = true;
    BOOST_CHECK(v.is_bool());
    BOOST_CHECK_EQUAL(v.get_bool(), true);
}

BOOST_AUTO_TEST_CASE(value_copy_construction)
{
    Value original = 42;
    Value copy(original);
    BOOST_CHECK_EQUAL(copy.get_int(), 42);
}

BOOST_AUTO_TEST_CASE(value_assignment)
{
    Value v1 = 42;
    Value v2 = "string";
    v2 = v1;
    BOOST_CHECK(v2.is_int());
    BOOST_CHECK_EQUAL(v2.get_int(), 42);
}

BOOST_AUTO_TEST_CASE(value_bad_cast_int_to_string)
{
    Value v = 42;
    BOOST_CHECK_THROW(v.get_string(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(value_bad_cast_string_to_int)
{
    Value v = "not a number";
    BOOST_CHECK_THROW(v.get_int(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(array_out_of_range)
{
    Array a;
    a.push_back(Value(1));
    a.push_back(Value(2));
    Value v = a;
    BOOST_CHECK_THROW(v[10], Array::Out_of_range);
}

BOOST_AUTO_TEST_CASE(struct_no_field)
{
    Struct s;
    s.insert("exists", Value(1));
    Value v = s;
    BOOST_CHECK_THROW(v["nonexistent"], Struct::No_field);
}

BOOST_AUTO_TEST_CASE(array_clear)
{
    Array a;
    a.push_back(Value(1));
    a.push_back(Value(2));
    a.push_back(Value(3));
    BOOST_CHECK_EQUAL(a.size(), 3u);
    a.clear();
    BOOST_CHECK_EQUAL(a.size(), 0u);
}

BOOST_AUTO_TEST_CASE(struct_clear)
{
    Struct s;
    s.insert("a", Value(1));
    s.insert("b", Value(2));
    BOOST_CHECK_EQUAL(s.size(), 2u);
    s.clear();
    BOOST_CHECK_EQUAL(s.size(), 0u);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(value_type_conversions)

BOOST_AUTO_TEST_CASE(int_operator_conversion)
{
    Value v = 42;
    int i = v;
    BOOST_CHECK_EQUAL(i, 42);
}

BOOST_AUTO_TEST_CASE(int64_operator_conversion)
{
    Value v = static_cast<int64_t>(5000000000LL);
    int64_t i = v;
    BOOST_CHECK_EQUAL(i, 5000000000LL);
}

BOOST_AUTO_TEST_CASE(bool_operator_conversion)
{
    Value v = true;
    bool b = v;
    BOOST_CHECK_EQUAL(b, true);
}

BOOST_AUTO_TEST_CASE(double_operator_conversion)
{
    Value v = 3.14159;
    double d = v;
    BOOST_CHECK_CLOSE(d, 3.14159, 0.00001);
}

BOOST_AUTO_TEST_CASE(string_operator_conversion)
{
    Value v = "test string";
    std::string s = v;
    BOOST_CHECK_EQUAL(s, "test string");
}

BOOST_AUTO_TEST_CASE(binary_operator_conversion)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("test data"));
    Value v(*bin);
    Binary_data result = v;
    BOOST_CHECK_EQUAL(result.get_data(), "test data");
}

BOOST_AUTO_TEST_CASE(tm_operator_conversion)
{
    struct tm t_in;
    memset(&t_in, 0, sizeof(t_in));
    t_in.tm_year = 2023 - 1900;
    t_in.tm_mon = 5;
    t_in.tm_mday = 15;
    t_in.tm_hour = 10;
    t_in.tm_min = 20;
    t_in.tm_sec = 30;

    Value v(&t_in);
    struct tm t_out = v;
    BOOST_CHECK_EQUAL(t_out.tm_year, t_in.tm_year);
    BOOST_CHECK_EQUAL(t_out.tm_mon, t_in.tm_mon);
    BOOST_CHECK_EQUAL(t_out.tm_mday, t_in.tm_mday);
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et
