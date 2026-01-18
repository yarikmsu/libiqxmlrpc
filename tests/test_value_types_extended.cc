#define BOOST_TEST_MODULE value_types_extended_test
#include <memory>
#include <ctime>
#include <limits>
#include <sstream>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/value.h"
#include "libiqxmlrpc/value_type_visitor.h"
#include "test_utils.h"

using namespace boost::unit_test;
using namespace iqxmlrpc;
using iqxmlrpc::test::OmitStringTagGuard;
using iqxmlrpc::test::DefaultIntGuard;
using iqxmlrpc::test::DefaultInt64Guard;

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
    Value v = std::numeric_limits<int>::max();
    BOOST_CHECK(v.is_int());
    BOOST_CHECK_EQUAL(v.get_int(), std::numeric_limits<int>::max());
}

BOOST_AUTO_TEST_CASE(value_min_int)
{
    Value v = std::numeric_limits<int>::min();
    BOOST_CHECK(v.is_int());
    BOOST_CHECK_EQUAL(v.get_int(), std::numeric_limits<int>::min());
}

BOOST_AUTO_TEST_CASE(value_max_int64)
{
    Value v = std::numeric_limits<int64_t>::max();
    BOOST_CHECK(v.is_int64());
    BOOST_CHECK_EQUAL(v.get_int64(), std::numeric_limits<int64_t>::max());
}

BOOST_AUTO_TEST_CASE(value_min_int64)
{
    Value v = std::numeric_limits<int64_t>::min();
    BOOST_CHECK(v.is_int64());
    BOOST_CHECK_EQUAL(v.get_int64(), std::numeric_limits<int64_t>::min());
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
    OmitStringTagGuard guard;  // RAII ensures restoration even on failure

    Value::omit_string_tag_in_responses(true);
    BOOST_CHECK(Value::omit_string_tag_in_responses());

    Value::omit_string_tag_in_responses(false);
    BOOST_CHECK(!Value::omit_string_tag_in_responses());
}

BOOST_AUTO_TEST_CASE(default_int_set_get_drop)
{
    DefaultIntGuard guard;  // RAII ensures cleanup even on failure

    // Initially should return null
    BOOST_CHECK(Value::get_default_int() == nullptr);

    // Set default
    Value::set_default_int(42);
    std::unique_ptr<Int> def(Value::get_default_int());
    BOOST_REQUIRE(def != nullptr);
    BOOST_CHECK_EQUAL(def->value(), 42);

    // Drop default
    Value::drop_default_int();
    BOOST_CHECK(Value::get_default_int() == nullptr);
}

BOOST_AUTO_TEST_CASE(default_int64_set_get_drop)
{
    DefaultInt64Guard guard;  // RAII ensures cleanup even on failure

    // Initially should return null
    BOOST_CHECK(Value::get_default_int64() == nullptr);

    // Set default
    Value::set_default_int64(5000000000LL);
    std::unique_ptr<Int64> def(Value::get_default_int64());
    BOOST_REQUIRE(def != nullptr);
    BOOST_CHECK_EQUAL(def->value(), 5000000000LL);

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

// Additional datetime boundary tests to cover validation branches (lines 562-568)
BOOST_AUTO_TEST_CASE(datetime_invalid_month_zero)
{
    BOOST_CHECK_THROW(Date_time(std::string("20260008T12:30:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_invalid_month_thirteen)
{
    BOOST_CHECK_THROW(Date_time(std::string("20261308T12:30:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_valid_december)
{
    Date_time dt(std::string("20261231T12:30:45"));
    BOOST_CHECK_EQUAL(dt.get_tm().tm_mon, 11);  // December = 11 (0-indexed)
}

BOOST_AUTO_TEST_CASE(datetime_valid_january)
{
    Date_time dt(std::string("20260101T12:30:45"));
    BOOST_CHECK_EQUAL(dt.get_tm().tm_mon, 0);  // January = 0 (0-indexed)
}

BOOST_AUTO_TEST_CASE(datetime_invalid_day_zero)
{
    BOOST_CHECK_THROW(Date_time(std::string("20260100T12:30:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_valid_day_one)
{
    Date_time dt(std::string("20260101T12:30:45"));
    BOOST_CHECK_EQUAL(dt.get_tm().tm_mday, 1);
}

BOOST_AUTO_TEST_CASE(datetime_valid_day_31)
{
    Date_time dt(std::string("20260131T12:30:45"));
    BOOST_CHECK_EQUAL(dt.get_tm().tm_mday, 31);
}

BOOST_AUTO_TEST_CASE(datetime_invalid_hour_24)
{
    BOOST_CHECK_THROW(Date_time(std::string("20260108T24:30:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_valid_hour_23)
{
    Date_time dt(std::string("20260108T23:30:45"));
    BOOST_CHECK_EQUAL(dt.get_tm().tm_hour, 23);
}

BOOST_AUTO_TEST_CASE(datetime_valid_hour_zero)
{
    Date_time dt(std::string("20260108T00:30:45"));
    BOOST_CHECK_EQUAL(dt.get_tm().tm_hour, 0);
}

BOOST_AUTO_TEST_CASE(datetime_invalid_minute_60)
{
    BOOST_CHECK_THROW(Date_time(std::string("20260108T12:60:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_valid_minute_59)
{
    Date_time dt(std::string("20260108T12:59:45"));
    BOOST_CHECK_EQUAL(dt.get_tm().tm_min, 59);
}

BOOST_AUTO_TEST_CASE(datetime_valid_second_59)
{
    Date_time dt(std::string("20260108T12:30:59"));
    BOOST_CHECK_EQUAL(dt.get_tm().tm_sec, 59);
}

BOOST_AUTO_TEST_CASE(datetime_valid_second_60_leap)
{
    // Seconds 60-61 are allowed for leap seconds (lines 565-566)
    Date_time dt(std::string("20151231T23:59:60"));
    BOOST_CHECK_EQUAL(dt.get_tm().tm_sec, 60);
}

BOOST_AUTO_TEST_CASE(datetime_valid_second_61_leap)
{
    Date_time dt(std::string("20151231T23:59:61"));
    BOOST_CHECK_EQUAL(dt.get_tm().tm_sec, 61);
}

BOOST_AUTO_TEST_CASE(datetime_invalid_second_62)
{
    BOOST_CHECK_THROW(Date_time(std::string("20260108T12:30:62")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_invalid_non_digit_chars)
{
    // Contains non-digit characters (line 552-553)
    BOOST_CHECK_THROW(Date_time(std::string("2026X108T12:30:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_missing_t_separator)
{
    // Missing T separator (line 548)
    BOOST_CHECK_THROW(Date_time(std::string("20260108X12:30:45")), Date_time::Malformed_iso8601);
}

BOOST_AUTO_TEST_CASE(datetime_wrong_length)
{
    // Wrong length (line 548)
    BOOST_CHECK_THROW(Date_time(std::string("20260108T12:30:4")), Date_time::Malformed_iso8601);  // 16 chars
    BOOST_CHECK_THROW(Date_time(std::string("20260108T12:30:450")), Date_time::Malformed_iso8601);  // 18 chars
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
    // Use explicit length to include null byte in the middle
    std::string original("Test binary \x00\x01\x02 data", 20);
    std::unique_ptr<Binary_data> bin1(Binary_data::from_data(original));
    std::string base64 = bin1->get_base64();
    std::unique_ptr<Binary_data> bin2(Binary_data::from_base64(base64));
    BOOST_CHECK_EQUAL(bin2->get_data(), original);
    BOOST_CHECK_EQUAL(bin2->get_data().size(), 20u);  // Verify full length preserved
}

// Tests for bytes >= 128 (covers signed char UB fix in encode())
// On platforms where char is signed, bytes >= 128 are negative values.
// Left-shifting negative values is undefined behavior in C++.
BOOST_AUTO_TEST_CASE(binary_encode_high_bytes)
{
    // Test data with bytes >= 128 (would cause UB before fix on signed char platforms)
    std::string data;
    data.push_back(static_cast<char>(0x80));  // 128
    data.push_back(static_cast<char>(0xFF));  // 255
    data.push_back(static_cast<char>(0xAB));  // 171

    std::unique_ptr<Binary_data> bin(Binary_data::from_data(data));
    std::string base64 = bin->get_base64();

    // Verify roundtrip
    std::unique_ptr<Binary_data> decoded(Binary_data::from_base64(base64));
    BOOST_CHECK_EQUAL(decoded->get_data(), data);

    // Verify specific encoding: 0x80 0xFF 0xAB = gP+r in base64
    BOOST_CHECK_EQUAL(base64, "gP+r");
}

BOOST_AUTO_TEST_CASE(binary_encode_boundary_values)
{
    // Test boundary: 127 (max positive signed char) and 128 (first "negative" as signed)
    std::string data;
    data.push_back(static_cast<char>(127));  // 0x7F - max positive signed char
    data.push_back(static_cast<char>(128));  // 0x80 - would be -128 as signed char
    data.push_back(static_cast<char>(129));  // 0x81 - would be -127 as signed char

    std::unique_ptr<Binary_data> bin(Binary_data::from_data(data));
    std::string base64 = bin->get_base64();

    // Verify roundtrip
    std::unique_ptr<Binary_data> decoded(Binary_data::from_base64(base64));
    BOOST_CHECK_EQUAL(decoded->get_data(), data);

    // Verify specific encoding: 0x7F 0x80 0x81 = f4CB in base64
    BOOST_CHECK_EQUAL(base64, "f4CB");
}

BOOST_AUTO_TEST_CASE(binary_encode_all_high_bytes)
{
    // Test all possible high byte values (128-255)
    std::string data;
    for (int i = 128; i <= 255; ++i) {
        data.push_back(static_cast<char>(i));
    }

    std::unique_ptr<Binary_data> bin(Binary_data::from_data(data));
    std::string base64 = bin->get_base64();

    // Verify roundtrip
    std::unique_ptr<Binary_data> decoded(Binary_data::from_base64(base64));
    BOOST_CHECK_EQUAL(decoded->get_data().size(), 128u);
    BOOST_CHECK_EQUAL(decoded->get_data(), data);
}

// Tests for get_idx branches in value_type.cc lines 433-451
BOOST_AUTO_TEST_CASE(binary_decode_all_uppercase)
{
    // Tests uppercase A-Z branch (lines 436-437)
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("QUJDREVG"));
    BOOST_CHECK_EQUAL(bin->get_data(), "ABCDEF");
}

BOOST_AUTO_TEST_CASE(binary_decode_all_lowercase)
{
    // Tests lowercase a-z branch (lines 439-440)
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("YWJjZGVm"));
    BOOST_CHECK_EQUAL(bin->get_data(), "abcdef");
}

BOOST_AUTO_TEST_CASE(binary_decode_digits)
{
    // Tests digits 0-9 branch (lines 442-443)
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("MDEyMzQ1"));
    BOOST_CHECK_EQUAL(bin->get_data(), "012345");
}

BOOST_AUTO_TEST_CASE(binary_decode_plus_char)
{
    // Tests + character branch (lines 445-446)
    // '+' is character 62 in base64
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("++++"));
    // + at index 62 encodes specific bits
    BOOST_CHECK(!bin->get_data().empty());
}

BOOST_AUTO_TEST_CASE(binary_decode_slash_char)
{
    // Tests / character branch (lines 448-449)
    // '/' is character 63 in base64
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("////"));
    BOOST_CHECK(!bin->get_data().empty());
}

BOOST_AUTO_TEST_CASE(binary_encode_padding_one_byte)
{
    // Tests single byte input which needs == padding (lines 408-413)
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("A"));
    std::string base64 = bin->get_base64();
    BOOST_CHECK_EQUAL(base64.substr(base64.length() - 2), "==");
}

BOOST_AUTO_TEST_CASE(binary_encode_padding_two_bytes)
{
    // Tests two byte input which needs = padding (lines 421-425)
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("AB"));
    std::string base64 = bin->get_base64();
    BOOST_CHECK_EQUAL(base64.back(), '=');
    BOOST_CHECK_NE(base64[base64.length() - 2], '=');
}

BOOST_AUTO_TEST_CASE(binary_encode_no_padding)
{
    // Tests three byte input with no padding (lines 415-420)
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("ABC"));
    std::string base64 = bin->get_base64();
    BOOST_CHECK(base64.find('=') == std::string::npos);
}

BOOST_AUTO_TEST_CASE(binary_decode_with_tabs)
{
    // Tests whitespace handling in decode (line 497)
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("TW\tFu"));
    BOOST_CHECK_EQUAL(bin->get_data(), "Man");
}

BOOST_AUTO_TEST_CASE(binary_decode_with_newlines)
{
    // Tests whitespace handling with different whitespace types
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("TW\n\rFu"));
    BOOST_CHECK_EQUAL(bin->get_data(), "Man");
}

BOOST_AUTO_TEST_CASE(binary_malformed_equals_at_position_one)
{
    // Tests malformed base64 with = at position 1 (line 464)
    BOOST_CHECK_THROW(Binary_data::from_base64("T=Fu"), Binary_data::Malformed_base64);
}

BOOST_AUTO_TEST_CASE(binary_malformed_equals_at_position_zero)
{
    // Tests malformed base64 with = at position 0 (line 464)
    BOOST_CHECK_THROW(Binary_data::from_base64("=WFu"), Binary_data::Malformed_base64);
}

BOOST_AUTO_TEST_CASE(binary_decode_mixed_case)
{
    // Tests mixed uppercase and lowercase
    std::unique_ptr<Binary_data> bin(Binary_data::from_base64("QWJDZEVm"));
    BOOST_CHECK(!bin->get_data().empty());
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

// ============================================================================
// Value Move Semantics Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(value_move_constructor_int)
{
    Value a(42);
    Value b(std::move(a));

    // Moved-to value should have the value
    BOOST_CHECK(b.is_int());
    BOOST_CHECK_EQUAL(b.get_int(), 42);
}

BOOST_AUTO_TEST_CASE(value_move_constructor_string)
{
    Value a(std::string("hello world"));
    Value b(std::move(a));

    // Moved-to value should have the value
    BOOST_CHECK(b.is_string());
    BOOST_CHECK_EQUAL(b.get_string(), "hello world");
}

BOOST_AUTO_TEST_CASE(value_move_constructor_array)
{
    Array arr;
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);
    Value a(arr);
    Value b(std::move(a));

    // Moved-to value should have the array
    BOOST_CHECK(b.is_array());
    BOOST_CHECK_EQUAL(b.size(), 3u);
    BOOST_CHECK_EQUAL(b[0].get_int(), 1);
    BOOST_CHECK_EQUAL(b[2].get_int(), 3);
}

BOOST_AUTO_TEST_CASE(value_move_assignment_int)
{
    Value a(100);
    Value b(999);

    b = std::move(a);

    // Moved-to value should have a's value
    BOOST_CHECK(b.is_int());
    BOOST_CHECK_EQUAL(b.get_int(), 100);
}

BOOST_AUTO_TEST_CASE(value_move_assignment_struct)
{
    Struct s;
    s.insert("key", Value(42));
    Value a(s);
    Value b(std::string("old value"));

    b = std::move(a);

    // Moved-to value should have the struct
    BOOST_CHECK(b.is_struct());
    BOOST_CHECK_EQUAL(b["key"].get_int(), 42);
}

BOOST_AUTO_TEST_CASE(value_move_self_assignment)
{
    Value a(123);
    Value* ptr = &a;

    // Self-assignment via move should be safe
    *ptr = std::move(a);

    // Value should still be valid (though possibly in moved-from state)
    // The important thing is that it doesn't crash
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE(value_use_after_move_throws_bad_cast)
{
    Value a(42);
    Value b(std::move(a));

    // Accessing moved-from value should throw Bad_cast
    BOOST_CHECK_THROW(a.get_int(), Value::Bad_cast);
    BOOST_CHECK_THROW(a.get_string(), Value::Bad_cast);
    BOOST_CHECK_THROW(a.get_double(), Value::Bad_cast);
    BOOST_CHECK_THROW(a.get_bool(), Value::Bad_cast);
    BOOST_CHECK_THROW(a.get_int64(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(value_use_after_move_assignment_throws)
{
    Value a(std::string("test"));
    Value b(0);

    b = std::move(a);

    // Accessing moved-from value should throw Bad_cast
    BOOST_CHECK_THROW(a.get_string(), Value::Bad_cast);
}

BOOST_AUTO_TEST_CASE(value_is_type_methods_return_false_after_move)
{
    Value a(42);
    Value b(std::move(a));

    // is_* methods should return false for moved-from value (not throw)
    BOOST_CHECK(!a.is_int());
    BOOST_CHECK(!a.is_string());
    BOOST_CHECK(!a.is_double());
    BOOST_CHECK(!a.is_bool());
    BOOST_CHECK(!a.is_int64());
    BOOST_CHECK(!a.is_binary());
    BOOST_CHECK(!a.is_datetime());
    BOOST_CHECK(!a.is_array());
    BOOST_CHECK(!a.is_struct());
    BOOST_CHECK(!a.is_nil());
}

BOOST_AUTO_TEST_CASE(array_push_back_rvalue)
{
    Array arr;

    // Push rvalue - should use move overload
    arr.push_back(Value(42));
    arr.push_back(Value(std::string("test")));

    BOOST_CHECK_EQUAL(arr.size(), 2u);
    BOOST_CHECK_EQUAL(arr[0].get_int(), 42);
    BOOST_CHECK_EQUAL(arr[1].get_string(), "test");
}

BOOST_AUTO_TEST_CASE(struct_insert_rvalue)
{
    Struct s;

    // Insert rvalue - should use move overload
    s.insert("num", Value(42));
    s.insert("str", Value(std::string("hello")));

    BOOST_CHECK_EQUAL(s.size(), 2u);
    BOOST_CHECK_EQUAL(s["num"].get_int(), 42);
    BOOST_CHECK_EQUAL(s["str"].get_string(), "hello");
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Additional coverage tests for value_type.cc edge cases
//=============================================================================

BOOST_AUTO_TEST_SUITE(value_type_edge_cases)

// Test Struct self-assignment
BOOST_AUTO_TEST_CASE(struct_self_assignment)
{
    Struct s;
    s.insert("key", Value(42));

    // Self-assignment should be a no-op
    Struct* ptr = &s;
    *ptr = s;

    BOOST_CHECK_EQUAL(s.size(), 1u);
    BOOST_CHECK_EQUAL(s["key"].get_int(), 42);
}

// Test Array self-assignment
BOOST_AUTO_TEST_CASE(array_self_assignment)
{
    Array a;
    a.push_back(Value(1));
    a.push_back(Value(2));

    // Self-assignment should be a no-op
    Array* ptr = &a;
    *ptr = a;

    BOOST_CHECK_EQUAL(a.size(), 2u);
    BOOST_CHECK_EQUAL(a[0].get_int(), 1);
    BOOST_CHECK_EQUAL(a[1].get_int(), 2);
}

// Test Struct::erase on non-existent key (should be no-op)
BOOST_AUTO_TEST_CASE(struct_erase_nonexistent)
{
    Struct s;
    s.insert("key", Value(42));

    // Erasing non-existent key should not throw
    BOOST_CHECK_NO_THROW(s.erase("nonexistent"));
    BOOST_CHECK_EQUAL(s.size(), 1u);
}

// Test Struct::clear
BOOST_AUTO_TEST_CASE(struct_clear)
{
    Struct s;
    s.insert("a", Value(1));
    s.insert("b", Value(2));
    s.insert("c", Value(3));

    BOOST_CHECK_EQUAL(s.size(), 3u);

    s.clear();

    BOOST_CHECK_EQUAL(s.size(), 0u);
    BOOST_CHECK(!s.has_field("a"));
}

// Test Array::clear
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

// Test Struct operator[] with non-existent key throws
BOOST_AUTO_TEST_CASE(struct_subscript_throws_no_field)
{
    Struct s;
    s.insert("exists", Value(42));

    BOOST_CHECK_THROW(s["nonexistent"], Struct::No_field);

    // Check const version too
    const Struct& cs = s;
    BOOST_CHECK_THROW(cs["nonexistent"], Struct::No_field);
}

// Test Array swap
BOOST_AUTO_TEST_CASE(array_swap)
{
    Array a, b;
    a.push_back(Value(1));
    b.push_back(Value(2));
    b.push_back(Value(3));

    a.swap(b);

    BOOST_CHECK_EQUAL(a.size(), 2u);
    BOOST_CHECK_EQUAL(b.size(), 1u);
    BOOST_CHECK_EQUAL(a[0].get_int(), 2);
    BOOST_CHECK_EQUAL(b[0].get_int(), 1);
}

// Test Struct swap
BOOST_AUTO_TEST_CASE(struct_swap)
{
    Struct a, b;
    a.insert("x", Value(1));
    b.insert("y", Value(2));
    b.insert("z", Value(3));

    a.swap(b);

    BOOST_CHECK_EQUAL(a.size(), 2u);
    BOOST_CHECK_EQUAL(b.size(), 1u);
    BOOST_CHECK(a.has_field("y"));
    BOOST_CHECK(b.has_field("x"));
}

// Test Binary_data get_base64 lazy encoding
BOOST_AUTO_TEST_CASE(binary_lazy_encoding)
{
    // Create from raw data (no base64 yet)
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("hello"));

    // First call to get_base64 should encode
    const std::string& b64_1 = bin->get_base64();
    BOOST_CHECK(!b64_1.empty());

    // Second call should return cached value
    const std::string& b64_2 = bin->get_base64();
    BOOST_CHECK_EQUAL(&b64_1, &b64_2);  // Same object (cached)
}

// Test Date_time with local time constructor
BOOST_AUTO_TEST_CASE(datetime_local_time)
{
    // Create with local time
    Date_time dt_local(true);

    // Should produce valid string
    const std::string& s = dt_local.to_string();
    BOOST_CHECK_EQUAL(s.length(), 17u);
    BOOST_CHECK_EQUAL(s[8], 'T');
}

// Test Date_time with UTC constructor
BOOST_AUTO_TEST_CASE(datetime_utc_time)
{
    // Create with UTC
    Date_time dt_utc(false);

    // Should produce valid string
    const std::string& s = dt_utc.to_string();
    BOOST_CHECK_EQUAL(s.length(), 17u);
    BOOST_CHECK_EQUAL(s[8], 'T');
}

// Test Date_time to_string caching
BOOST_AUTO_TEST_CASE(datetime_to_string_caching)
{
    // Note: Must use std::string explicitly, otherwise the string literal
    // converts to bool (standard conversion) instead of std::string (user-defined)
    Date_time dt(std::string("20230615T12:30:45"));

    // First call computes the string
    const std::string& s1 = dt.to_string();

    // Second call returns cached string
    const std::string& s2 = dt.to_string();

    BOOST_CHECK_EQUAL(&s1, &s2);  // Same object (cached)
    BOOST_CHECK_EQUAL(s1, "20230615T12:30:45");
}

// Test Nil type
BOOST_AUTO_TEST_CASE(nil_type_operations)
{
    Nil nil;

    // Check type name
    BOOST_CHECK_EQUAL(nil.type_name(), "nil");

    // Check clone
    std::unique_ptr<Value_type> cloned(nil.clone());
    BOOST_CHECK_EQUAL(cloned->type_name(), "nil");
}

// Test Scalar types - Int
BOOST_AUTO_TEST_CASE(scalar_int_operations)
{
    Int i(42);

    BOOST_CHECK_EQUAL(i.value(), 42);
    BOOST_CHECK_EQUAL(i.type_name(), "i4");

    std::unique_ptr<Value_type> cloned(i.clone());
    Int* cloned_int = dynamic_cast<Int*>(cloned.get());
    BOOST_REQUIRE(cloned_int != nullptr);
    BOOST_CHECK_EQUAL(cloned_int->value(), 42);
}

// Test Scalar types - Int64
BOOST_AUTO_TEST_CASE(scalar_int64_operations)
{
    Int64 i(9876543210LL);

    BOOST_CHECK_EQUAL(i.value(), 9876543210LL);
    BOOST_CHECK_EQUAL(i.type_name(), "i8");

    std::unique_ptr<Value_type> cloned(i.clone());
    Int64* cloned_int64 = dynamic_cast<Int64*>(cloned.get());
    BOOST_REQUIRE(cloned_int64 != nullptr);
    BOOST_CHECK_EQUAL(cloned_int64->value(), 9876543210LL);
}

// Test Scalar types - Bool
BOOST_AUTO_TEST_CASE(scalar_bool_operations)
{
    Bool b_true(true);
    Bool b_false(false);

    BOOST_CHECK_EQUAL(b_true.value(), true);
    BOOST_CHECK_EQUAL(b_false.value(), false);
    BOOST_CHECK_EQUAL(b_true.type_name(), "boolean");

    std::unique_ptr<Value_type> cloned(b_true.clone());
    Bool* cloned_bool = dynamic_cast<Bool*>(cloned.get());
    BOOST_REQUIRE(cloned_bool != nullptr);
    BOOST_CHECK_EQUAL(cloned_bool->value(), true);
}

// Test Scalar types - Double
BOOST_AUTO_TEST_CASE(scalar_double_operations)
{
    Double d(3.14159);

    BOOST_CHECK_CLOSE(d.value(), 3.14159, 0.0001);
    BOOST_CHECK_EQUAL(d.type_name(), "double");

    std::unique_ptr<Value_type> cloned(d.clone());
    Double* cloned_double = dynamic_cast<Double*>(cloned.get());
    BOOST_REQUIRE(cloned_double != nullptr);
    BOOST_CHECK_CLOSE(cloned_double->value(), 3.14159, 0.0001);
}

// Test Scalar types - String
BOOST_AUTO_TEST_CASE(scalar_string_operations)
{
    String s("hello world");

    BOOST_CHECK_EQUAL(s.value(), "hello world");
    BOOST_CHECK_EQUAL(s.type_name(), "string");

    std::unique_ptr<Value_type> cloned(s.clone());
    String* cloned_string = dynamic_cast<String*>(cloned.get());
    BOOST_REQUIRE(cloned_string != nullptr);
    BOOST_CHECK_EQUAL(cloned_string->value(), "hello world");
}

// Test Struct replace existing key
BOOST_AUTO_TEST_CASE(struct_replace_existing_key)
{
    Struct s;
    s.insert("key", Value(1));
    BOOST_CHECK_EQUAL(s["key"].get_int(), 1);

    // Replace with new value
    s.insert("key", Value(999));
    BOOST_CHECK_EQUAL(s["key"].get_int(), 999);
    BOOST_CHECK_EQUAL(s.size(), 1u);  // Still just one key
}

// Test Date_time edge cases for month/day/time validation
BOOST_AUTO_TEST_CASE(datetime_boundary_values)
{
    // January 1st
    Date_time dt1(std::string("20230101T00:00:00"));
    BOOST_CHECK_EQUAL(dt1.to_string(), "20230101T00:00:00");

    // December 31st
    Date_time dt2(std::string("20231231T23:59:59"));
    BOOST_CHECK_EQUAL(dt2.to_string(), "20231231T23:59:59");

    // Leap second (61 seconds is valid in tm)
    Date_time dt3(std::string("20230630T23:59:60"));
    BOOST_CHECK_EQUAL(dt3.to_string(), "20230630T23:59:60");
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Additional tests for clang-tidy PR coverage
// Covers specific code paths modified in the modernization effort
//=============================================================================

BOOST_AUTO_TEST_SUITE(clang_tidy_coverage_tests)

// Test Struct::operator[] const version returning valid value
// Covers value_type.cc lines 270-277 (const operator[])
BOOST_AUTO_TEST_CASE(struct_const_subscript_success)
{
    Struct s;
    s.insert("key1", Value(42));
    s.insert("key2", Value("hello"));
    s.insert("key3", Value(3.14));

    // Use const reference to ensure const operator[] is called
    const Struct& cs = s;

    BOOST_CHECK_EQUAL(cs["key1"].get_int(), 42);
    BOOST_CHECK_EQUAL(cs["key2"].get_string(), "hello");
    BOOST_CHECK_CLOSE(cs["key3"].get_double(), 3.14, 0.001);
}

// Test Struct::operator[] non-const version returning valid value
// Covers value_type.cc lines 281-288 (non-const operator[])
BOOST_AUTO_TEST_CASE(struct_nonconst_subscript_success)
{
    Struct s;
    s.insert("key", Value(100));

    // Non-const access and modification
    Value& ref = s["key"];
    BOOST_CHECK_EQUAL(ref.get_int(), 100);

    // Modify through reference
    ref = Value(200);
    BOOST_CHECK_EQUAL(s["key"].get_int(), 200);
}

// Test Struct iteration via range-based for (uses begin()/end())
// Covers value_type_xml.cc line 68 (for loop over struct)
BOOST_AUTO_TEST_CASE(struct_range_iteration)
{
    Struct s;
    s.insert("a", Value(1));
    s.insert("b", Value(2));
    s.insert("c", Value(3));

    int sum = 0;
    int count = 0;
    for (const auto& entry : s) {
        sum += entry.second->get_int();
        count++;
    }

    BOOST_CHECK_EQUAL(count, 3);
    BOOST_CHECK_EQUAL(sum, 6);
}

// Test Array iteration via range-based for
// Covers value_type_xml.cc line 83 (for loop over array)
BOOST_AUTO_TEST_CASE(array_range_iteration)
{
    Array a;
    a.push_back(Value(10));
    a.push_back(Value(20));
    a.push_back(Value(30));

    int sum = 0;
    for (const auto& elem : a) {
        sum += elem.get_int();
    }

    BOOST_CHECK_EQUAL(sum, 60);
}

// Test Date_time validation with boundary values for DeMorgan fix
// Covers value_type.cc lines 605-610 (validation with simplified boolean expression)
BOOST_AUTO_TEST_CASE(datetime_demorgan_validation_boundaries)
{
    // Test all boundary conditions that exercise the DeMorgan-simplified condition
    // tm_year < 0
    BOOST_CHECK_THROW(Date_time(std::string("-0010108T12:30:45")), Date_time::Malformed_iso8601);

    // tm_mon < 0 (month 00)
    BOOST_CHECK_THROW(Date_time(std::string("20260008T12:30:45")), Date_time::Malformed_iso8601);

    // tm_mon > 11 (month 13)
    BOOST_CHECK_THROW(Date_time(std::string("20261308T12:30:45")), Date_time::Malformed_iso8601);

    // tm_mday < 1 (day 00)
    BOOST_CHECK_THROW(Date_time(std::string("20260100T12:30:45")), Date_time::Malformed_iso8601);

    // tm_mday > 31 (day 32)
    BOOST_CHECK_THROW(Date_time(std::string("20260132T12:30:45")), Date_time::Malformed_iso8601);

    // tm_hour < 0 is not possible with string parsing
    // tm_hour > 23 (hour 24)
    BOOST_CHECK_THROW(Date_time(std::string("20260108T24:30:45")), Date_time::Malformed_iso8601);

    // tm_min > 59 (minute 60)
    BOOST_CHECK_THROW(Date_time(std::string("20260108T12:60:45")), Date_time::Malformed_iso8601);

    // tm_sec > 61 (second 62)
    BOOST_CHECK_THROW(Date_time(std::string("20260108T12:30:62")), Date_time::Malformed_iso8601);

    // Valid boundary cases should pass
    Date_time dt_valid1(std::string("20260101T00:00:00")); // min valid
    BOOST_CHECK_EQUAL(dt_valid1.to_string(), "20260101T00:00:00");

    Date_time dt_valid2(std::string("20261231T23:59:59")); // max valid (non-leap)
    BOOST_CHECK_EQUAL(dt_valid2.to_string(), "20261231T23:59:59");
}

// Test Date_time to_string with strftime check
// Covers value_type.cc line 638 (strftime return check)
BOOST_AUTO_TEST_CASE(datetime_strftime_coverage)
{
    // Create datetime and verify to_string produces expected output
    Date_time dt(std::string("20260515T14:30:00"));
    const std::string& result = dt.to_string();

    BOOST_CHECK_EQUAL(result.length(), 17u);
    BOOST_CHECK_EQUAL(result, "20260515T14:30:00");

    // Verify caching by checking pointer identity
    const std::string& cached = dt.to_string();
    BOOST_CHECK_EQUAL(&result, &cached);
}

// Test Value construction with complex nested types
// This verifies that nested Struct/Array values work correctly,
// which is a prerequisite for request parsing tests in test_request_response.cc
BOOST_AUTO_TEST_CASE(value_nested_construction)
{
    // Complex nested structures that would be used in request parameters
    Struct nested;
    nested.insert("inner_key", Value("inner_value"));

    Array arr;
    arr.push_back(Value(1));
    arr.push_back(Value(2));

    Struct outer;
    outer.insert("nested", Value(nested));
    outer.insert("array", Value(arr));

    // Verify nested structure is correctly constructed
    Value v(outer);
    BOOST_CHECK(v.is_struct());
    BOOST_CHECK(v["nested"].is_struct());
    BOOST_CHECK_EQUAL(v["nested"]["inner_key"].get_string(), "inner_value");
    BOOST_CHECK(v["array"].is_array());
    BOOST_CHECK_EQUAL(v["array"].size(), 2u);
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et
