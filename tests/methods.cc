#include <cstdlib>
#include <iostream>
#include <fstream>
#include <mutex>
#include <random>
#include <thread>
#include <chrono>
#include <openssl/md5.h>
#include <boost/test/test_tools.hpp>
#include "libiqxmlrpc/server.h"
#include "methods.h"
#include "test_common.h"

using namespace iqxmlrpc;
// THREAD_SAFE_TEST_MESSAGE macro is defined in test_common.h

void register_user_methods(iqxmlrpc::Server& s)
{
  register_method<serverctl_stop>(s, "serverctl.stop");
  register_method<serverctl_log>(s, "serverctl.log");
  register_method(s, "echo", echo_method);
  register_method(s, "echo_user", echo_user);
  register_method(s, "error_method", error_method);
  register_method(s, "std_exception_method", std_exception_method);
  register_method(s, "unknown_exception_method", unknown_exception_method);
  register_method(s, "trace", trace_method);
  register_method(s, "sleep", sleep_method);
  register_method<Get_file>(s, "get_file");
}

void serverctl_stop::execute(
  const iqxmlrpc::Param_list&, iqxmlrpc::Value& )
{
  THREAD_SAFE_TEST_MESSAGE("Stop_server method invoked.");
  server().log_message( "Stopping the server." );
  server().set_exit_flag();
}

void serverctl_log::execute(
  const iqxmlrpc::Param_list& args, iqxmlrpc::Value& result )
{
  THREAD_SAFE_TEST_MESSAGE("Log_message method invoked.");
  std::string msg = "Test log message";
  if (!args.empty() && args[0].is_string()) {
    msg = args[0].get_string();
  }
  server().log_message(msg);
  result = true;
}


void echo_method(
  iqxmlrpc::Method*,
  const iqxmlrpc::Param_list& args,
  iqxmlrpc::Value& retval )
{
  THREAD_SAFE_TEST_MESSAGE("Echo method invoked.");

  if (!args.empty())
    retval = args[0];
}

void trace_method(
  iqxmlrpc::Method*,
  const iqxmlrpc::Param_list& args,
  iqxmlrpc::Value& retval )
{
  THREAD_SAFE_TEST_MESSAGE("Trace method invoked.");
  std::string s;
  for (const auto& arg : args) {
    if (arg.is_string()) {
      s += arg.get_string();
    }
  }

  retval = s;
}

void echo_user(
  iqxmlrpc::Method* m,
  const iqxmlrpc::Param_list&,
  iqxmlrpc::Value& retval )
{
  THREAD_SAFE_TEST_MESSAGE("echo_user method invoked.");
  retval = m->authname();
}

void error_method(
  iqxmlrpc::Method* /*m*/,
  const iqxmlrpc::Param_list&,
  iqxmlrpc::Value& /*retval*/ )
{
  THREAD_SAFE_TEST_MESSAGE("error_method method invoked.");
  throw iqxmlrpc::Fault(123, "My fault");
}

void std_exception_method(
  iqxmlrpc::Method* /*m*/,
  const iqxmlrpc::Param_list&,
  iqxmlrpc::Value& /*retval*/ )
{
  THREAD_SAFE_TEST_MESSAGE("std_exception_method invoked.");
  throw std::runtime_error("Test std::exception");
}

void unknown_exception_method(
  iqxmlrpc::Method* /*m*/,
  const iqxmlrpc::Param_list&,
  iqxmlrpc::Value& /*retval*/ )
{
  THREAD_SAFE_TEST_MESSAGE("unknown_exception_method invoked.");
  throw 42;  // Throw a non-exception type
}

void Get_file::execute( 
  const iqxmlrpc::Param_list& args, iqxmlrpc::Value& retval )
{
  THREAD_SAFE_TEST_MESSAGE("Get_file method invoked.");

  int retsize = args[0]["requested-size"]; 
  if (retsize <= 0)
	  throw Fault( 0, "requested-size must be > 0" );

  THREAD_SAFE_TEST_MESSAGE("Generating data...");
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> dist(0, 255);
  std::string s(retsize, '\0');
  std::generate(s.begin(), s.end(), [&]() { return static_cast<char>(dist(rng)); });

  retval = Struct();
  retval.insert("data", Binary_data::from_data(s));

  THREAD_SAFE_TEST_MESSAGE("Calculating MD5 checksum...");
  typedef const unsigned char md5char;
  typedef const char strchar;

  unsigned char md5[16];
  // MD5 is deprecated in OpenSSL 3.0 but still works; suppress warning for test code
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  MD5(reinterpret_cast<md5char*>(s.data()), s.length(), md5);
#pragma GCC diagnostic pop
  
  retval.insert("md5", Binary_data::from_data(
    reinterpret_cast<strchar*>(md5), sizeof(md5)));
}

void sleep_method(
  iqxmlrpc::Method*,
  const iqxmlrpc::Param_list& args,
  iqxmlrpc::Value& retval )
{
  THREAD_SAFE_TEST_MESSAGE("Sleep method invoked.");

  double sleep_seconds = 0.1;  // Default 100ms
  if (!args.empty() && args[0].is_double()) {
    sleep_seconds = args[0].get_double();
  } else if (!args.empty() && args[0].is_int()) {
    sleep_seconds = static_cast<double>(args[0].get_int());
  }

  auto duration = std::chrono::duration<double>(sleep_seconds);
  std::this_thread::sleep_for(
    std::chrono::duration_cast<std::chrono::milliseconds>(duration));

  retval = "done";
}
