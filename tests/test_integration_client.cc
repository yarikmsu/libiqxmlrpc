//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011-2026 Anton Dedov
//
//  Integration tests: Client operations

#include <boost/test/unit_test.hpp>

#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_server.h"
#include "libiqxmlrpc/http_client.h"

#include "methods.h"
#include "test_common.h"

using namespace iqxmlrpc;
using namespace iqnet;
using namespace iqxmlrpc_test;

//=============================================================================
// Client Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(client_tests)

BOOST_FIXTURE_TEST_CASE(client_execute_single, IntegrationFixture)
{
  start_server(1, 50);
  auto client = create_client();

  Response r = client->execute("echo", Value("hello"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "hello");
}

BOOST_FIXTURE_TEST_CASE(client_multiple_params, IntegrationFixture)
{
  start_server(1, 51);
  auto client = create_client();

  Param_list params;
  params.emplace_back("a");
  params.emplace_back("b");
  params.emplace_back("c");

  Response r = client->execute("trace", params);
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "abc");
}

BOOST_FIXTURE_TEST_CASE(client_struct_value, IntegrationFixture)
{
  start_server(1, 52);
  auto client = create_client();

  Struct s;
  s.insert("key1", Value("value1"));
  s.insert("key2", Value(42));

  Response r = client->execute("echo", Value(s));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK(r.value().is_struct());
  BOOST_CHECK_EQUAL(r.value()["key1"].get_string(), "value1");
  BOOST_CHECK_EQUAL(r.value()["key2"].get_int(), 42);
}

BOOST_FIXTURE_TEST_CASE(client_array_value, IntegrationFixture)
{
  start_server(1, 53);
  auto client = create_client();

  Array a;
  a.push_back(Value(1));
  a.push_back(Value(2));
  a.push_back(Value(3));

  Response r = client->execute("echo", Value(a));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK(r.value().is_array());
  BOOST_CHECK_EQUAL(r.value().size(), 3u);
  BOOST_CHECK_EQUAL(r.value()[0].get_int(), 1);
}

BOOST_AUTO_TEST_SUITE_END()
