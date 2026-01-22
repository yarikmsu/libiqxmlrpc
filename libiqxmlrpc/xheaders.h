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

  static bool validate(const std::string& val);
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
