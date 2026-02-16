//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#pragma once

#include "api_export.h"

#include <string>
#include <map>  // For operator= compatibility
#include <unordered_map>
#include <iostream>
#include <stdexcept>

namespace iqxmlrpc
{
//! Custom HTTP "X-" headers container.
//!
//! \note Since 0.14.1, the "X-" prefix validation (validate() static method)
//! has been removed.  The caller is now responsible for ensuring header
//! names follow the desired convention.  This also means operator[] and
//! operator= no longer filter non-"X-" headers.
//!
//! \note Since 0.14.1, the internal storage changed from std::map to
//! std::unordered_map.  Iteration order over begin()/end() is no longer
//! sorted by key name.
class LIBIQXMLRPC_API XHeaders {
private:
  std::unordered_map<std::string, std::string> xheaders_;
public:
  // NOLINTNEXTLINE(modernize-use-equals-default) - explicit init required for -Weffc++
  XHeaders() : xheaders_() {}
  typedef std::unordered_map<std::string, std::string>::const_iterator const_iterator;
  virtual XHeaders& operator=(const std::map<std::string, std::string>& v);
  virtual std::string& operator[](const std::string& v);
  virtual size_t size() const;
  virtual const_iterator find(const std::string& k) const;
  virtual const_iterator begin() const;
  virtual const_iterator end() const;
  virtual ~XHeaders();

};

inline std::ostream& operator<<(std::ostream& os, const XHeaders& xheaders) {
  for (const auto& header : xheaders) {
    os << " " << header.first << "[" << header.second << "]";
  }
  return os;
}

class LIBIQXMLRPC_API Error_xheader: public std::invalid_argument {
public:
  explicit Error_xheader(const char* msg);
};
}
