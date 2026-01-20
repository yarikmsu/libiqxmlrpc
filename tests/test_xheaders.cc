#define BOOST_TEST_MODULE xheaders_test

#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/xheaders.h"

using namespace boost::unit_test;
using namespace iqxmlrpc;

class XHeaders_fixture {
public:
  typedef std::map<std::string, std::string> Headers;
  Headers opt;
  XHeaders h;
public:
  XHeaders_fixture():
    opt(),
    h()
  {
    opt["X-id"] = "1";
    opt["x-cmd"] = "2";
    opt["Xcmd"] = "3";
    opt["xmesg"] = "4";
    opt["content_length"] = "5";
  }
};

BOOST_AUTO_TEST_CASE( validate ) {
  BOOST_CHECK(XHeaders::validate("X-d"));
  BOOST_CHECK(XHeaders::validate("x-d"));
  BOOST_CHECK(!XHeaders::validate("Xd"));
  BOOST_CHECK(!XHeaders::validate("xd"));
  BOOST_CHECK(!XHeaders::validate("data"));
}

BOOST_FIXTURE_TEST_CASE( find, XHeaders_fixture )
{
  BOOST_TEST_MESSAGE("Operator = test...");

  h = opt;

  BOOST_CHECK_EQUAL(h.size(), 2);
  BOOST_CHECK(h.find("X-id") != h.end());
  BOOST_CHECK(h.find("x-id") != h.end());
  BOOST_CHECK(h.find("X-cmd") != h.end());
  BOOST_CHECK(h.find("x-cmd") != h.end());

  std::string res;
  for (const auto& entry : h) {
    res += entry.first;
    res += entry.second;
  }
  BOOST_CHECK(res == "x-id1x-cmd2" || res == "x-cmd2x-id1");
}

BOOST_FIXTURE_TEST_CASE( brackets, XHeaders_fixture )
{
  BOOST_TEST_MESSAGE("Operator [] test...");

  h["X-new"] = "new";
  BOOST_CHECK_EQUAL(h.size(), 1);
  BOOST_CHECK(h.find("X-new") != h.end());
  BOOST_CHECK(h.find("x-new") != h.end());
  BOOST_CHECK(h.find("X-new1") == h.end());

  BOOST_CHECK_THROW(h["length"], Error_xheader);

  BOOST_CHECK_EQUAL(h["X-new"], "new");
  BOOST_CHECK_EQUAL(h["x-new"], "new");
}

BOOST_AUTO_TEST_CASE(xheaders_empty)
{
  XHeaders h;
  BOOST_CHECK_EQUAL(h.size(), 0);
  BOOST_CHECK(h.begin() == h.end());
}

BOOST_AUTO_TEST_CASE(xheaders_case_insensitive_access)
{
  XHeaders h;
  h["X-Test-Header"] = "value1";
  BOOST_CHECK_EQUAL(h["x-test-header"], "value1");
  BOOST_CHECK_EQUAL(h["X-TEST-HEADER"], "value1");
  BOOST_CHECK_EQUAL(h["X-Test-Header"], "value1");
}

BOOST_AUTO_TEST_CASE(xheaders_update_existing)
{
  XHeaders h;
  h["X-Key"] = "original";
  BOOST_CHECK_EQUAL(h["X-Key"], "original");
  h["x-key"] = "updated";
  BOOST_CHECK_EQUAL(h["X-Key"], "updated");
  BOOST_CHECK_EQUAL(h.size(), 1);
}

BOOST_AUTO_TEST_CASE(xheaders_assignment_filters_invalid)
{
  std::map<std::string, std::string> source;
  source["X-Valid-1"] = "v1";
  source["x-Valid-2"] = "v2";
  source["Invalid-1"] = "bad1";
  source["content-type"] = "bad2";
  source["X-Valid-3"] = "v3";

  XHeaders h;
  h = source;
  BOOST_CHECK_EQUAL(h.size(), 3);
  BOOST_CHECK(h.find("X-Valid-1") != h.end());
  BOOST_CHECK(h.find("x-Valid-2") != h.end());
  BOOST_CHECK(h.find("X-Valid-3") != h.end());
  BOOST_CHECK(h.find("Invalid-1") == h.end());
}

BOOST_AUTO_TEST_CASE(xheaders_error_message)
{
  Error_xheader err("Test error message");
  BOOST_CHECK(std::string(err.what()).find("Test error message") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(xheaders_iterate_all)
{
  XHeaders h;
  h["X-A"] = "1";
  h["X-B"] = "2";
  h["X-C"] = "3";

  int count = 0;
  for (const auto& entry : h) {
    count++;
    BOOST_CHECK(entry.first.substr(0, 2) == "x-");
  }
  BOOST_CHECK_EQUAL(count, 3);
}

BOOST_AUTO_TEST_CASE(xheaders_find_nonexistent)
{
  XHeaders h;
  h["X-Exists"] = "yes";
  BOOST_CHECK(h.find("X-NotExists") == h.end());
  BOOST_CHECK(h.find("x-notexists") == h.end());
}

BOOST_AUTO_TEST_CASE(validate_edge_cases)
{
  BOOST_CHECK(!XHeaders::validate(""));
  BOOST_CHECK(!XHeaders::validate("X"));
  BOOST_CHECK(!XHeaders::validate("x"));
  BOOST_CHECK(XHeaders::validate("X-"));
  BOOST_CHECK(XHeaders::validate("x-"));
  BOOST_CHECK(XHeaders::validate("X-A"));
  BOOST_CHECK(XHeaders::validate("x-a"));
  BOOST_CHECK(!XHeaders::validate("Y-Header"));
  BOOST_CHECK(!XHeaders::validate("Header-X"));
}

// vim:ts=2:sw=2:et
