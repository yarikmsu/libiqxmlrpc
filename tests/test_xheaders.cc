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
    opt["My-Custom"] = "3";
    opt["content-type"] = "4";
  }
};

BOOST_FIXTURE_TEST_CASE( find, XHeaders_fixture )
{
  BOOST_TEST_MESSAGE("Operator = test...");

  h = opt;

  BOOST_CHECK_EQUAL(h.size(), 4);
  BOOST_CHECK(h.find("X-id") != h.end());
  BOOST_CHECK(h.find("x-id") != h.end());
  BOOST_CHECK(h.find("X-cmd") != h.end());
  BOOST_CHECK(h.find("x-cmd") != h.end());
  BOOST_CHECK(h.find("my-custom") != h.end());
  BOOST_CHECK(h.find("content-type") != h.end());
}

BOOST_FIXTURE_TEST_CASE( brackets, XHeaders_fixture )
{
  BOOST_TEST_MESSAGE("Operator [] test...");

  h["X-new"] = "new";
  BOOST_CHECK_EQUAL(h.size(), 1);
  BOOST_CHECK(h.find("X-new") != h.end());
  BOOST_CHECK(h.find("x-new") != h.end());
  BOOST_CHECK(h.find("X-new1") == h.end());

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

BOOST_AUTO_TEST_CASE(xheaders_assignment_accepts_all)
{
  std::map<std::string, std::string> source;
  source["X-Valid-1"] = "v1";
  source["x-Valid-2"] = "v2";
  source["Custom-Header"] = "v3";
  source["content-type"] = "v4";
  source["Authorization"] = "v5";

  XHeaders h;
  h = source;
  BOOST_CHECK_EQUAL(h.size(), 5);
  BOOST_CHECK(h.find("X-Valid-1") != h.end());
  BOOST_CHECK(h.find("x-Valid-2") != h.end());
  BOOST_CHECK(h.find("Custom-Header") != h.end());
  BOOST_CHECK(h.find("content-type") != h.end());
  BOOST_CHECK(h.find("Authorization") != h.end());
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
  h["Custom-C"] = "3";

  int count = 0;
  for (const auto& entry : h) {
    count++;
    (void)entry;
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

BOOST_AUTO_TEST_CASE(xheaders_arbitrary_custom_headers)
{
  XHeaders h;
  h["My-Custom"] = "val";
  BOOST_CHECK_EQUAL(h["my-custom"], "val");

  h["Authorization"] = "Bearer token";
  BOOST_CHECK_EQUAL(h["authorization"], "Bearer token");

  h["Content-Type"] = "text/xml";
  BOOST_CHECK_EQUAL(h["content-type"], "text/xml");

  BOOST_CHECK_EQUAL(h.size(), 3);
}

// vim:ts=2:sw=2:et
