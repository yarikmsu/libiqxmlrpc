//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef IQXMLRPC_HTTP_AUTH_PLUGIN_H
#define IQXMLRPC_HTTP_AUTH_PLUGIN_H

#include "sysinc.h"

#include <string>

namespace iqxmlrpc {

//! SECURITY: Constant-time string comparison to prevent timing attacks.
/*! Use this when comparing passwords or other secrets to prevent
    attackers from inferring password length via response timing.
    \param a First string to compare
    \param b Second string to compare
    \return true if strings are equal, false otherwise
*/
inline bool constant_time_compare(const std::string& a, const std::string& b)
{
  // Always iterate over the longer string to avoid leaking length information
  const size_t max_len = (a.length() > b.length()) ? a.length() : b.length();

  // Start with length difference to ensure unequal lengths always fail
  volatile unsigned char result = (a.length() != b.length()) ? 1 : 0;

  for (size_t i = 0; i < max_len; ++i) {
    // Use 0 as padding for the shorter string (XOR with 0 has no effect on result
    // when lengths match, but ensures constant iteration count)
    unsigned char ca = (i < a.length()) ? static_cast<unsigned char>(a[i]) : 0;
    unsigned char cb = (i < b.length()) ? static_cast<unsigned char>(b[i]) : 0;
    result |= ca ^ cb;
  }

  return result == 0;
}

//! HTTP Authentication plugin.
/*! \warning SECURITY: HTTP Basic Authentication transmits credentials
    in Base64 encoding (NOT encryption). Always use HTTPS (TLS) when
    authentication is enabled to protect credentials from interception.

    When implementing do_authenticate(), use constant_time_compare()
    for password verification to prevent timing attacks:
    \code
    bool do_authenticate(const std::string& user, const std::string& pw) const override {
      std::string stored_hash = get_password_hash(user);
      std::string provided_hash = hash_password(pw);
      return constant_time_compare(stored_hash, provided_hash);
    }
    \endcode
*/
class Auth_Plugin_base {
public:
  virtual ~Auth_Plugin_base() = default;

  bool authenticate(
    const std::string& user,
    const std::string& password) const;

  bool authenticate_anonymous() const;

private:
  //! User should implement this function. Method must return true when
  //! authentication succeed. Authorization goes synchronously.
  /*! \warning SECURITY: Use constant_time_compare() for password comparison
      to prevent timing attacks. Never compare passwords with == operator.
  */
  virtual bool do_authenticate(const std::string&, const std::string&) const = 0;

  //! User should implement this function too.
  //! Just return false to make anonymous requests forbiden or
  //! return true to allow clients process requests without authentication.
  virtual bool do_authenticate_anonymous() const = 0;
};

} // namespace iqxmlrpc

#endif
// vim:sw=2:ts=2:et:
