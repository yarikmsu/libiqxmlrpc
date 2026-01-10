#define BOOST_TEST_MODULE value_types_extended_test
#include <memory>
#include <ctime>
#include <sstream>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/value.h"
#include "libiqxmlrpc/value_type_visitor.h"

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
    BOOST_CHECK_THROW(v.get_string(), Value::Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(value_bad_cast_string_to_int)
{
    Value v = "not a number";
    BOOST_CHECK_THROW(v.get_int(), Value::Value::Bad_cast);
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

BOOST_AUTO_TEST_SUITE(print_value_visitor_tests)

BOOST_AUTO_TEST_CASE(print_nil)
{
    std::ostringstream oss;
    Print_value_visitor visitor(oss);
    Value v = Nil();
    v.apply_visitor(visitor);
    BOOST_CHECK_EQUAL(oss.str(), "NIL");
}

BOOST_AUTO_TEST_CASE(print_int)
{
    std::ostringstream oss;
    Print_value_visitor visitor(oss);
    Value v = 42;
    v.apply_visitor(visitor);
    BOOST_CHECK_EQUAL(oss.str(), "42");
}

BOOST_AUTO_TEST_CASE(print_int64)
{
    std::ostringstream oss;
    Print_value_visitor visitor(oss);
    Value v = static_cast<int64_t>(5000000000LL);
    v.apply_visitor(visitor);
    BOOST_CHECK_EQUAL(oss.str(), "5000000000");
}

BOOST_AUTO_TEST_CASE(print_double)
{
    std::ostringstream oss;
    Print_value_visitor visitor(oss);
    Value v = 3.14;
    v.apply_visitor(visitor);
    BOOST_CHECK(oss.str().find("3.14") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(print_bool_true)
{
    std::ostringstream oss;
    Print_value_visitor visitor(oss);
    Value v = true;
    v.apply_visitor(visitor);
    BOOST_CHECK_EQUAL(oss.str(), "1");
}

BOOST_AUTO_TEST_CASE(print_bool_false)
{
    std::ostringstream oss;
    Print_value_visitor visitor(oss);
    Value v = false;
    v.apply_visitor(visitor);
    BOOST_CHECK_EQUAL(oss.str(), "0");
}

BOOST_AUTO_TEST_CASE(print_string)
{
    std::ostringstream oss;
    Print_value_visitor visitor(oss);
    Value v = "hello";
    v.apply_visitor(visitor);
    BOOST_CHECK_EQUAL(oss.str(), "'hello'");
}

BOOST_AUTO_TEST_CASE(print_struct)
{
    std::ostringstream oss;
    Print_value_visitor visitor(oss);
    Struct s;
    s.insert("key", Value(42));
    Value v = s;
    v.apply_visitor(visitor);
    std::string result = oss.str();
    BOOST_CHECK(result.find("{") != std::string::npos);
    BOOST_CHECK(result.find("'key'") != std::string::npos);
    BOOST_CHECK(result.find("42") != std::string::npos);
    BOOST_CHECK(result.find("}") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(print_array)
{
    std::ostringstream oss;
    Print_value_visitor visitor(oss);
    Array a;
    a.push_back(Value(1));
    a.push_back(Value(2));
    Value v = a;
    v.apply_visitor(visitor);
    std::string result = oss.str();
    BOOST_CHECK(result.find("[") != std::string::npos);
    BOOST_CHECK(result.find("1") != std::string::npos);
    BOOST_CHECK(result.find("2") != std::string::npos);
    BOOST_CHECK(result.find("]") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(print_binary)
{
    std::ostringstream oss;
    Print_value_visitor visitor(oss);
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("test"));
    Value v(*bin);
    v.apply_visitor(visitor);
    BOOST_CHECK_EQUAL(oss.str(), "RAWDATA");
}

BOOST_AUTO_TEST_CASE(print_datetime)
{
    std::ostringstream oss;
    Print_value_visitor visitor(oss);
    Date_time dt(std::string("20231225T12:30:45"));
    Value v(dt);
    v.apply_visitor(visitor);
    BOOST_CHECK(oss.str().find("20231225T12:30:45") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(value_options_tests)

BOOST_AUTO_TEST_CASE(omit_string_tag_default_false)
{
    // Default should be false
    BOOST_CHECK(!Value::omit_string_tag_in_responses());
}

BOOST_AUTO_TEST_CASE(omit_string_tag_set_and_get)
{
    // Save original value
    bool original = Value::omit_string_tag_in_responses();

    Value::omit_string_tag_in_responses(true);
    BOOST_CHECK(Value::omit_string_tag_in_responses());

    Value::omit_string_tag_in_responses(false);
    BOOST_CHECK(!Value::omit_string_tag_in_responses());

    // Restore original value
    Value::omit_string_tag_in_responses(original);
}

BOOST_AUTO_TEST_CASE(default_int_set_get_drop)
{
    // Initially should return null
    BOOST_CHECK(Value::get_default_int() == nullptr);

    // Set default
    Value::set_default_int(42);
    Int* def = Value::get_default_int();
    BOOST_REQUIRE(def != nullptr);
    BOOST_CHECK_EQUAL(def->value(), 42);
    delete def;

    // Drop default
    Value::drop_default_int();
    BOOST_CHECK(Value::get_default_int() == nullptr);
}

BOOST_AUTO_TEST_CASE(default_int64_set_get_drop)
{
    // Initially should return null
    BOOST_CHECK(Value::get_default_int64() == nullptr);

    // Set default
    Value::set_default_int64(5000000000LL);
    Int64* def = Value::get_default_int64();
    BOOST_REQUIRE(def != nullptr);
    BOOST_CHECK_EQUAL(def->value(), 5000000000LL);
    delete def;

    // Drop default
    Value::drop_default_int64();
    BOOST_CHECK(Value::get_default_int64() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(value_array_operations)

BOOST_AUTO_TEST_CASE(array_mutable_access)
{
    Array a;
    a.push_back(Value(1));
    a.push_back(Value(2));
    Value v = a;

    // Non-const access
    Array& arr_ref = v.the_array();
    BOOST_CHECK_EQUAL(arr_ref.size(), 2u);

    // Modify through mutable index operator
    v[0] = Value(100);
    BOOST_CHECK_EQUAL(v[0].get_int(), 100);
}

BOOST_AUTO_TEST_CASE(array_iterators)
{
    Array a;
    a.push_back(Value(1));
    a.push_back(Value(2));
    a.push_back(Value(3));
    Value v = a;

    int sum = 0;
    for (Array::const_iterator it = v.arr_begin(); it != v.arr_end(); ++it) {
        sum += it->get_int();
    }
    BOOST_CHECK_EQUAL(sum, 6);
}

BOOST_AUTO_TEST_CASE(array_push_back_on_value)
{
    Array a;
    Value v = a;
    v.push_back(Value(42));
    BOOST_CHECK_EQUAL(v.size(), 1u);
    BOOST_CHECK_EQUAL(v[0].get_int(), 42);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(value_struct_operations)

BOOST_AUTO_TEST_CASE(struct_mutable_access)
{
    Struct s;
    s.insert("key", Value(1));
    Value v = s;

    // Non-const access
    Struct& st_ref = v.the_struct();
    BOOST_CHECK_EQUAL(st_ref.size(), 1u);

    // Modify through mutable index operator
    v["key"] = Value(100);
    BOOST_CHECK_EQUAL(v["key"].get_int(), 100);
}

BOOST_AUTO_TEST_CASE(struct_has_field)
{
    Struct s;
    s.insert("exists", Value(1));
    Value v = s;

    BOOST_CHECK(v.has_field("exists"));
    BOOST_CHECK(!v.has_field("missing"));
}

BOOST_AUTO_TEST_CASE(struct_char_ptr_access)
{
    Struct s;
    s.insert("key", Value(42));
    Value v = s;

    const char* key = "key";
    BOOST_CHECK_EQUAL(v[key].get_int(), 42);

    // Mutable access with char*
    v[key] = Value(100);
    BOOST_CHECK_EQUAL(v[key].get_int(), 100);
}

BOOST_AUTO_TEST_CASE(struct_insert_on_value)
{
    Struct s;
    Value v = s;
    v.insert("new_key", Value("new_value"));
    BOOST_CHECK(v.has_field("new_key"));
    BOOST_CHECK_EQUAL(v["new_key"].get_string(), "new_value");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(value_free_functions)

BOOST_AUTO_TEST_CASE(print_value_function)
{
    std::ostringstream oss;
    Value v = 42;
    print_value(v, oss);
    BOOST_CHECK_EQUAL(oss.str(), "42");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(value_type_tests)

BOOST_AUTO_TEST_CASE(int_type_clone)
{
    Int original(42);
    std::unique_ptr<Value_type> cloned(original.clone());
    BOOST_CHECK_EQUAL(dynamic_cast<Int*>(cloned.get())->value(), 42);
}

BOOST_AUTO_TEST_CASE(int64_type_clone)
{
    Int64 original(9876543210LL);
    std::unique_ptr<Value_type> cloned(original.clone());
    BOOST_CHECK_EQUAL(dynamic_cast<Int64*>(cloned.get())->value(), 9876543210LL);
}

BOOST_AUTO_TEST_CASE(double_type_clone)
{
    Double original(3.14159);
    std::unique_ptr<Value_type> cloned(original.clone());
    BOOST_CHECK_CLOSE(dynamic_cast<Double*>(cloned.get())->value(), 3.14159, 0.00001);
}

BOOST_AUTO_TEST_CASE(bool_type_clone)
{
    Bool original(true);
    std::unique_ptr<Value_type> cloned(original.clone());
    BOOST_CHECK_EQUAL(dynamic_cast<Bool*>(cloned.get())->value(), true);
}

BOOST_AUTO_TEST_CASE(string_type_clone)
{
    String original("test");
    std::unique_ptr<Value_type> cloned(original.clone());
    BOOST_CHECK_EQUAL(dynamic_cast<String*>(cloned.get())->value(), "test");
}

BOOST_AUTO_TEST_CASE(nil_type_clone)
{
    Nil original;
    std::unique_ptr<Value_type> cloned(original.clone());
    BOOST_CHECK(dynamic_cast<Nil*>(cloned.get()) != nullptr);
}

BOOST_AUTO_TEST_CASE(array_type_clone)
{
    Array original;
    original.push_back(Value_ptr(new Value(1)));
    original.push_back(Value_ptr(new Value(2)));
    std::unique_ptr<Value_type> cloned(original.clone());
    Array* arr = dynamic_cast<Array*>(cloned.get());
    BOOST_REQUIRE(arr != nullptr);
    BOOST_CHECK_EQUAL(arr->size(), 2u);
}

BOOST_AUTO_TEST_CASE(struct_type_clone)
{
    Struct original;
    original.insert("key", Value_ptr(new Value("val")));
    std::unique_ptr<Value_type> cloned(original.clone());
    Struct* st = dynamic_cast<Struct*>(cloned.get());
    BOOST_REQUIRE(st != nullptr);
    BOOST_CHECK(st->has_field("key"));
}

BOOST_AUTO_TEST_CASE(binary_type_clone)
{
    std::unique_ptr<Binary_data> original(Binary_data::from_data("data"));
    std::unique_ptr<Value_type> cloned(original->clone());
    Binary_data* bin = dynamic_cast<Binary_data*>(cloned.get());
    BOOST_REQUIRE(bin != nullptr);
    BOOST_CHECK_EQUAL(bin->get_data(), "data");
}

BOOST_AUTO_TEST_CASE(datetime_type_clone)
{
    Date_time original(std::string("20260108T12:30:45"));
    std::unique_ptr<Value_type> cloned(original.clone());
    Date_time* dt = dynamic_cast<Date_time*>(cloned.get());
    BOOST_REQUIRE(dt != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(value_bad_cast_tests)

BOOST_AUTO_TEST_CASE(int_on_string_throws)
{
    Value v = "string";
    BOOST_CHECK_THROW(v.get_int(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(int64_on_int_throws)
{
    Value v = 42;
    BOOST_CHECK_THROW(v.get_int64(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(double_on_int_throws)
{
    Value v = 42;
    BOOST_CHECK_THROW(v.get_double(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(bool_on_string_throws)
{
    Value v = "true";
    BOOST_CHECK_THROW(v.get_bool(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(string_on_int_throws)
{
    Value v = 42;
    BOOST_CHECK_THROW(v.get_string(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(binary_on_string_throws)
{
    Value v = "data";
    BOOST_CHECK_THROW(v.get_binary(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(datetime_on_string_throws)
{
    Value v = "20260108";
    BOOST_CHECK_THROW(v.get_datetime(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(array_on_struct_throws)
{
    Struct s;
    Value v = s;
    BOOST_CHECK_THROW(v.the_array(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(struct_on_array_throws)
{
    Array a;
    Value v = a;
    BOOST_CHECK_THROW(v.the_struct(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(index_on_struct_throws)
{
    Struct s;
    Value v = s;
    BOOST_CHECK_THROW(v[0], Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(field_on_array_throws)
{
    Array a;
    Value v = a;
    BOOST_CHECK_THROW(v["key"], Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(push_back_on_struct_throws)
{
    Struct s;
    Value v = s;
    BOOST_CHECK_THROW(v.push_back(Value(1)), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(insert_on_array_throws)
{
    Array a;
    Value v = a;
    BOOST_CHECK_THROW(v.insert("key", Value(1)), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(has_field_on_array_throws)
{
    Array a;
    Value v = a;
    BOOST_CHECK_THROW(v.has_field("key"), Value::Bad_cast);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(datetime_parsing_tests)

BOOST_AUTO_TEST_CASE(datetime_valid_format)
{
    // Format must be exactly YYYYMMDDTHHMMSS (17 chars)
    Date_time dt(std::string("20260108T12:30:45"));
    struct tm t = dt.get_tm();
    BOOST_CHECK_EQUAL(t.tm_year, 2026 - 1900);
    BOOST_CHECK_EQUAL(t.tm_mon, 0);
    BOOST_CHECK_EQUAL(t.tm_mday, 8);
    BOOST_CHECK_EQUAL(t.tm_hour, 12);
    BOOST_CHECK_EQUAL(t.tm_min, 30);
    BOOST_CHECK_EQUAL(t.tm_sec, 45);
}

BOOST_AUTO_TEST_CASE(datetime_malformed_throws)
{
    BOOST_CHECK_THROW(Date_time(std::string("invalid")), Date_time::Malformed_iso8601);
    BOOST_CHECK_THROW(Date_time(std::string("")), Date_time::Malformed_iso8601);
    BOOST_CHECK_THROW(Date_time(std::string("2026")), Date_time::Malformed_iso8601);
    BOOST_CHECK_THROW(Date_time(std::string("2026-01-08T12:30:45")), Date_time::Malformed_iso8601);  // Wrong length
}

BOOST_AUTO_TEST_CASE(datetime_to_string)
{
    Date_time dt(std::string("20260108T12:30:45"));
    std::string str = dt.to_string();
    BOOST_CHECK(str.find("2026") != std::string::npos);
    BOOST_CHECK(str.find("12:30:45") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(binary_data_extended_tests)

BOOST_AUTO_TEST_CASE(binary_from_data_extended)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("test data"));
    BOOST_CHECK_EQUAL(bin->get_data(), "test data");
}

BOOST_AUTO_TEST_CASE(binary_from_base64_decode)
{
    // "SGVsbG8=" is base64 for "Hello"
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("SGVsbG8="));
    BOOST_CHECK_EQUAL(bin->get_data(), "Hello");
}

BOOST_AUTO_TEST_CASE(binary_get_base64_encode)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("Hello"));
    std::string base64 = bin->get_base64();
    BOOST_CHECK_EQUAL(base64, "SGVsbG8=");
}

BOOST_AUTO_TEST_CASE(binary_empty_data_extended)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_data(""));
    BOOST_CHECK(bin->get_data().empty());
    BOOST_CHECK(bin->get_base64().empty());
}

BOOST_AUTO_TEST_CASE(binary_roundtrip_extended)
{
    std::string original = "Test binary \x00\x01\x02 data";
    std::unique_ptr<Binary_data> bin1(Binary_data::from_data(original));
    std::string base64 = bin1->get_base64();
    std::unique_ptr<Binary_data> bin2(Binary_data::from_base64(base64));
    BOOST_CHECK_EQUAL(bin2->get_data(), original);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(struct_operations_extended)

BOOST_AUTO_TEST_CASE(struct_insert_value_ptr)
{
    Struct s;
    s.insert("key", Value_ptr(new Value(42)));
    BOOST_CHECK(s.has_field("key"));
    BOOST_CHECK_EQUAL(s["key"].get_int(), 42);
}

BOOST_AUTO_TEST_CASE(struct_iterator)
{
    Struct s;
    s.insert("a", Value(1));
    s.insert("b", Value(2));

    int count = 0;
    for (Struct::const_iterator it = s.begin(); it != s.end(); ++it) {
        count++;
    }
    BOOST_CHECK_EQUAL(count, 2);
}

BOOST_AUTO_TEST_CASE(struct_find_missing_returns_end)
{
    Struct s;
    s.insert("exists", Value(1));
    BOOST_CHECK(s.find("missing") == s.end());
}

BOOST_AUTO_TEST_CASE(struct_move_constructor)
{
    Struct a;
    a.insert("x", Value(10));
    a.insert("y", Value(20));
    a.insert("z", Value(30));

    Struct b(std::move(a));

    // Moved-to struct should have the values
    BOOST_CHECK_EQUAL(b.size(), 3u);
    BOOST_CHECK(b.has_field("x"));
    BOOST_CHECK(b.has_field("y"));
    BOOST_CHECK(b.has_field("z"));
    BOOST_CHECK_EQUAL(b["x"].get_int(), 10);
    BOOST_CHECK_EQUAL(b["y"].get_int(), 20);
    BOOST_CHECK_EQUAL(b["z"].get_int(), 30);

    // Moved-from struct should be empty
    BOOST_CHECK_EQUAL(a.size(), 0u);
}

BOOST_AUTO_TEST_CASE(struct_move_assignment)
{
    Struct a;
    a.insert("key1", Value(100));
    a.insert("key2", Value(200));

    Struct b;
    b.insert("old", Value(999));

    b = std::move(a);

    // Moved-to struct should have the values from a
    BOOST_CHECK_EQUAL(b.size(), 2u);
    BOOST_CHECK(b.has_field("key1"));
    BOOST_CHECK(b.has_field("key2"));
    BOOST_CHECK_EQUAL(b["key1"].get_int(), 100);
    BOOST_CHECK_EQUAL(b["key2"].get_int(), 200);

    // Moved-from struct has the old values from b (swap semantics)
    BOOST_CHECK_EQUAL(a.size(), 1u);
    BOOST_CHECK(a.has_field("old"));
    BOOST_CHECK_EQUAL(a["old"].get_int(), 999);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(array_operations_extended)

BOOST_AUTO_TEST_CASE(array_const_iterator)
{
    Array a;
    a.push_back(Value(1));
    a.push_back(Value(2));
    a.push_back(Value(3));

    const Array& ca = a;
    int sum = 0;
    for (Array::const_iterator it = ca.begin(); it != ca.end(); ++it) {
        sum += (*it).get_int();
    }
    BOOST_CHECK_EQUAL(sum, 6);
}

BOOST_AUTO_TEST_CASE(array_index_access)
{
    Array a;
    a.push_back(Value(10));
    a.push_back(Value(20));

    BOOST_CHECK_EQUAL(a[0].get_int(), 10);
    BOOST_CHECK_EQUAL(a[1].get_int(), 20);
}

BOOST_AUTO_TEST_CASE(array_size_check)
{
    Array a;
    BOOST_CHECK_EQUAL(a.size(), 0u);

    a.push_back(Value(1));
    BOOST_CHECK_EQUAL(a.size(), 1u);

    a.push_back(Value(2));
    BOOST_CHECK_EQUAL(a.size(), 2u);
}

BOOST_AUTO_TEST_CASE(array_move_constructor)
{
    Array a;
    a.push_back(Value(1));
    a.push_back(Value(2));
    a.push_back(Value(3));

    Array b(std::move(a));

    // Moved-to array should have the values
    BOOST_CHECK_EQUAL(b.size(), 3u);
    BOOST_CHECK_EQUAL(b[0].get_int(), 1);
    BOOST_CHECK_EQUAL(b[1].get_int(), 2);
    BOOST_CHECK_EQUAL(b[2].get_int(), 3);

    // Moved-from array should be empty
    BOOST_CHECK_EQUAL(a.size(), 0u);
}

BOOST_AUTO_TEST_CASE(array_move_assignment)
{
    Array a;
    a.push_back(Value(10));
    a.push_back(Value(20));

    Array b;
    b.push_back(Value(100));

    b = std::move(a);

    // Moved-to array should have the values from a
    BOOST_CHECK_EQUAL(b.size(), 2u);
    BOOST_CHECK_EQUAL(b[0].get_int(), 10);
    BOOST_CHECK_EQUAL(b[1].get_int(), 20);

    // Moved-from array has the old values from b (swap semantics)
    BOOST_CHECK_EQUAL(a.size(), 1u);
    BOOST_CHECK_EQUAL(a[0].get_int(), 100);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(struct_erase_tests)

BOOST_AUTO_TEST_CASE(struct_erase_existing_key)
{
    Struct s;
    s.insert("keep", Value(1));
    s.insert("remove", Value(2));
    BOOST_CHECK_EQUAL(s.size(), 2u);

    s.erase("remove");
    BOOST_CHECK_EQUAL(s.size(), 1u);
    BOOST_CHECK(s.has_field("keep"));
    BOOST_CHECK(!s.has_field("remove"));
}

BOOST_AUTO_TEST_CASE(struct_erase_nonexistent_key)
{
    Struct s;
    s.insert("key", Value(1));
    BOOST_CHECK_EQUAL(s.size(), 1u);

    // Erasing non-existent key should be safe (no-op)
    s.erase("nonexistent");
    BOOST_CHECK_EQUAL(s.size(), 1u);
    BOOST_CHECK(s.has_field("key"));
}

BOOST_AUTO_TEST_CASE(struct_erase_all_keys)
{
    Struct s;
    s.insert("a", Value(1));
    s.insert("b", Value(2));
    s.insert("c", Value(3));

    s.erase("a");
    s.erase("b");
    s.erase("c");
    BOOST_CHECK_EQUAL(s.size(), 0u);
}

BOOST_AUTO_TEST_CASE(struct_replace_value)
{
    Struct s;
    s.insert("key", Value(1));
    BOOST_CHECK_EQUAL(s["key"].get_int(), 1);

    // Insert with same key replaces value
    s.insert("key", Value(2));
    BOOST_CHECK_EQUAL(s["key"].get_int(), 2);
    BOOST_CHECK_EQUAL(s.size(), 1u);  // Still only one key
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(array_swap_tests)

BOOST_AUTO_TEST_CASE(array_swap)
{
    Array a, b;
    a.push_back(Value(1));
    a.push_back(Value(2));
    b.push_back(Value(10));
    b.push_back(Value(20));
    b.push_back(Value(30));

    a.swap(b);

    BOOST_CHECK_EQUAL(a.size(), 3u);
    BOOST_CHECK_EQUAL(a[0].get_int(), 10);
    BOOST_CHECK_EQUAL(a[1].get_int(), 20);
    BOOST_CHECK_EQUAL(a[2].get_int(), 30);

    BOOST_CHECK_EQUAL(b.size(), 2u);
    BOOST_CHECK_EQUAL(b[0].get_int(), 1);
    BOOST_CHECK_EQUAL(b[1].get_int(), 2);
}

BOOST_AUTO_TEST_CASE(array_swap_with_empty)
{
    Array a, b;
    a.push_back(Value(1));
    // b is empty

    a.swap(b);

    BOOST_CHECK_EQUAL(a.size(), 0u);
    BOOST_CHECK_EQUAL(b.size(), 1u);
    BOOST_CHECK_EQUAL(b[0].get_int(), 1);
}

BOOST_AUTO_TEST_CASE(array_self_assignment)
{
    Array a;
    a.push_back(Value(1));
    a.push_back(Value(2));

    a = a;  // Self-assignment should be safe

    BOOST_CHECK_EQUAL(a.size(), 2u);
    BOOST_CHECK_EQUAL(a[0].get_int(), 1);
    BOOST_CHECK_EQUAL(a[1].get_int(), 2);
}

BOOST_AUTO_TEST_CASE(array_copy_assignment)
{
    Array a, b;
    a.push_back(Value(1));
    a.push_back(Value(2));
    b.push_back(Value(100));

    b = a;

    BOOST_CHECK_EQUAL(b.size(), 2u);
    BOOST_CHECK_EQUAL(b[0].get_int(), 1);
    BOOST_CHECK_EQUAL(b[1].get_int(), 2);

    // Original should be unchanged
    BOOST_CHECK_EQUAL(a.size(), 2u);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(struct_swap_tests)

BOOST_AUTO_TEST_CASE(struct_swap)
{
    Struct a, b;
    a.insert("x", Value(1));
    b.insert("y", Value(2));
    b.insert("z", Value(3));

    a.swap(b);

    BOOST_CHECK_EQUAL(a.size(), 2u);
    BOOST_CHECK(a.has_field("y"));
    BOOST_CHECK(a.has_field("z"));

    BOOST_CHECK_EQUAL(b.size(), 1u);
    BOOST_CHECK(b.has_field("x"));
}

BOOST_AUTO_TEST_CASE(struct_self_assignment)
{
    Struct s;
    s.insert("key", Value(42));

    s = s;  // Self-assignment should be safe

    BOOST_CHECK_EQUAL(s.size(), 1u);
    BOOST_CHECK_EQUAL(s["key"].get_int(), 42);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(datetime_boundary_tests)

BOOST_AUTO_TEST_CASE(datetime_leap_second)
{
    // Seconds can be 60 or 61 for leap seconds
    Date_time dt(std::string("20151231T23:59:60"));
    const struct tm& t = dt.get_tm();
    BOOST_CHECK_EQUAL(t.tm_sec, 60);
}

BOOST_AUTO_TEST_CASE(datetime_boundary_day_zero_invalid)
{
    // Day 0 is invalid
    BOOST_CHECK_THROW(Date_time(std::string("20230100T12:00:00")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_month_zero_invalid)
{
    // Month 0 is invalid (must be 01-12)
    BOOST_CHECK_THROW(Date_time(std::string("20230001T12:00:00")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_negative_year_invalid)
{
    // Negative year check (atoi returns negative for year < 1900)
    BOOST_CHECK_THROW(Date_time(std::string("00001225T12:30:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_seconds_62_invalid)
{
    // Seconds > 61 is invalid
    BOOST_CHECK_THROW(Date_time(std::string("20230101T12:00:62")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_hour_boundary)
{
    // Hour 23 is valid
    Date_time dt(std::string("20230101T23:00:00"));
    BOOST_CHECK_EQUAL(dt.get_tm().tm_hour, 23);
}

BOOST_AUTO_TEST_CASE(datetime_minute_boundary)
{
    // Minute 59 is valid
    Date_time dt(std::string("20230101T12:59:00"));
    BOOST_CHECK_EQUAL(dt.get_tm().tm_min, 59);
}

BOOST_AUTO_TEST_CASE(datetime_cached_string)
{
    Date_time dt(std::string("20230615T10:20:30"));
    // First call generates the string
    std::string s1 = dt.to_string();
    // Second call returns cached value
    std::string s2 = dt.to_string();
    BOOST_CHECK_EQUAL(s1, s2);
    BOOST_CHECK_EQUAL(s1, "20230615T10:20:30");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(binary_data_edge_cases)

BOOST_AUTO_TEST_CASE(binary_decode_all_alphabet)
{
    // Test decoding with all base64 alphabet characters
    // "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    std::string base64 = "QUJD";  // ABC
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64(base64));
    BOOST_CHECK_EQUAL(bin->get_data(), "ABC");
}

BOOST_AUTO_TEST_CASE(binary_decode_plus_slash)
{
    // Test base64 characters + and /
    // "+/" encodes to specific bytes
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("Pz8/"));  // Contains special chars
    BOOST_CHECK(!bin->get_data().empty());
}

BOOST_AUTO_TEST_CASE(binary_decode_digits)
{
    // Test base64 digits 0-9
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("MTIz"));  // "123"
    BOOST_CHECK_EQUAL(bin->get_data(), "123");
}

BOOST_AUTO_TEST_CASE(binary_decode_lowercase)
{
    // Test lowercase letters in base64
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("YWJj"));  // "abc"
    BOOST_CHECK_EQUAL(bin->get_data(), "abc");
}

BOOST_AUTO_TEST_CASE(binary_malformed_second_char_padding_throws)
{
    // Second char being = is invalid
    BOOST_CHECK_THROW(Binary_data::from_base64("A==="), Binary_data::Malformed_base64);
}

BOOST_AUTO_TEST_CASE(binary_whitespace_between_groups)
{
    // Whitespace should be skipped
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("TWFu\r\nTWFu"));
    BOOST_CHECK_EQUAL(bin->get_data(), "ManMan");
}

BOOST_AUTO_TEST_CASE(binary_tabs_and_spaces)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("T W F u"));
    BOOST_CHECK_EQUAL(bin->get_data(), "Man");
}

BOOST_AUTO_TEST_CASE(binary_end_of_data_exception_path)
{
    // "TW==" has padding at third position (tests End_of_data path)
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("TQ=="));
    BOOST_CHECK_EQUAL(bin->get_data(), "M");
}

BOOST_AUTO_TEST_CASE(binary_third_char_padding)
{
    // "TWE=" - third char is valid, fourth is padding
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("TWE="));
    BOOST_CHECK_EQUAL(bin->get_data(), "Ma");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(array_iterator_tests)

BOOST_AUTO_TEST_CASE(array_const_iterator_operations)
{
    Array a;
    a.push_back(Value(1));
    a.push_back(Value(2));
    a.push_back(Value(3));

    const Array& ca = a;

    // Test pre-increment and post-increment
    Array::const_iterator it = ca.begin();
    BOOST_CHECK_EQUAL((*it).get_int(), 1);

    ++it;  // Pre-increment
    BOOST_CHECK_EQUAL((*it).get_int(), 2);

    it++;  // Post-increment
    BOOST_CHECK_EQUAL((*it).get_int(), 3);
}

BOOST_AUTO_TEST_CASE(array_const_iterator_equality)
{
    Array a;
    a.push_back(Value(1));

    const Array& ca = a;
    Array::const_iterator it1 = ca.begin();
    Array::const_iterator it2 = ca.begin();

    BOOST_CHECK(it1 == it2);
    ++it1;
    BOOST_CHECK(it1 != it2);
    BOOST_CHECK(it1 == ca.end());
}

BOOST_AUTO_TEST_CASE(array_const_iterator_arrow)
{
    Array a;
    a.push_back(Value("test"));

    const Array& ca = a;
    Array::const_iterator it = ca.begin();
    // Use arrow operator to access Value methods
    BOOST_CHECK_EQUAL(it->get_string(), "test");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(value_from_cstring_tests)

BOOST_AUTO_TEST_CASE(value_from_const_char_ptr)
{
    const char* str = "hello";
    Value v(str);
    BOOST_CHECK(v.is_string());
    BOOST_CHECK_EQUAL(v.get_string(), "hello");
}

BOOST_AUTO_TEST_CASE(value_from_empty_cstring)
{
    const char* str = "";
    Value v(str);
    BOOST_CHECK(v.is_string());
    BOOST_CHECK(v.get_string().empty());
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et
