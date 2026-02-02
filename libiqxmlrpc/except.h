//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef IQXMLRPC_EXCEPT_H
#define IQXMLRPC_EXCEPT_H

#include "api_export.h"

#include <stdexcept>
#include <string>

// Exceptions are conformant ot Fault Code Interoperability, version 20010516.
// http://xmlrpc-epi.sourceforge.net/specs/rfc.fault_codes.php
namespace iqxmlrpc
{

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4275)
#endif

//! Base class for iqxmlrpc exceptions.
class LIBIQXMLRPC_API Exception: public std::runtime_error {
  int ex_code;

public:
  explicit Exception( const std::string& i, int c = -32000 /*undefined error*/ ):
    runtime_error( i ), ex_code(c) {}

  virtual int code() const { return ex_code; }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

//! XML Parser error.
class LIBIQXMLRPC_API Parse_error: public Exception {
public:
  explicit Parse_error( const std::string& d ):
    Exception(std::string("Parser error. ") += d, -32700) {}
};

//! XML Parser depth exceeded error (DoS protection).
class LIBIQXMLRPC_API Parse_depth_error: public Exception {
public:
  explicit Parse_depth_error( int depth, int max_depth ):
    Exception("Parser error. Maximum XML depth exceeded (" +
              std::to_string(depth) + " > " + std::to_string(max_depth) + ")", -32700) {}
};

//! XML Parser element count exceeded error (DoS protection).
class LIBIQXMLRPC_API Parse_element_count_error: public Exception {
public:
  explicit Parse_element_count_error( int count, int max_count ):
    Exception("Parser error. Maximum XML element count exceeded (" +
              std::to_string(count) + " > " + std::to_string(max_count) + ")", -32700) {}
};

//! XML Parser error.
class LIBIQXMLRPC_API XmlBuild_error: public Exception {
public:
  explicit XmlBuild_error( const std::string& d ):
    Exception(std::string("XML build error. ") += d, -32705) {}
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4275)
#endif

//! XML-RPC structures not conforming to spec.
class LIBIQXMLRPC_API XML_RPC_violation: public Exception {
public:
  XML_RPC_violation():
    Exception("Server error. XML-RPC violation.", -32600) {}

  explicit XML_RPC_violation( const std::string& s ):
    Exception(std::string("Server error. XML-RPC violation: ") += s, -32600) {}
};

//! Exception is being thrown when user tries to create
//! Method object for unregistered name.
class LIBIQXMLRPC_API Unknown_method: public Exception {
public:
  explicit Unknown_method( const std::string& name ):
    Exception(std::string("Server error. Method '") + sanitize_method_name(name) + "' not found.", -32601) {}

private:
  //! SECURITY: Sanitize method name to prevent log injection and info disclosure.
  static std::string sanitize_method_name(const std::string& name) {
    // Limit length to prevent memory exhaustion and log flooding
    constexpr size_t MAX_METHOD_NAME_LEN = 128;
    std::string result;
    result.reserve(std::min(name.length(), MAX_METHOD_NAME_LEN));

    for (size_t i = 0; i < name.length() && result.length() < MAX_METHOD_NAME_LEN; ++i) {
      char c = name[i];
      // Allow alphanumeric, dots, underscores, colons (for namespaces)
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '_' || c == ':') {
        result += c;
      }
    }

    if (name.length() > MAX_METHOD_NAME_LEN) {
      result += "...";
    }
    return result;
  }
};

//! Invalid method parameters exception.
class LIBIQXMLRPC_API Invalid_meth_params: public Exception {
public:
  Invalid_meth_params():
    Exception( "Server error. Invalid method parameters.", -32602 ) {}
};

//! Exception which user should throw from Method to
//! initiate fault response.
class LIBIQXMLRPC_API Fault: public Exception {
public:
  Fault( int c, const std::string& s ):
    Exception(s, c) {}
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace iqxmlrpc

#endif
