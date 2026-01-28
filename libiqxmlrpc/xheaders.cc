//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include "xheaders.h"

#include <algorithm>
#include <cctype>

namespace iqxmlrpc
{

namespace {

inline void to_lower_inplace(std::string& s) {
  std::transform(s.begin(), s.end(), s.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
}

} // anonymous namespace

XHeaders& XHeaders::operator=(const std::map<std::string, std::string>& v) {
  xheaders_.clear();
  for (const auto& entry : v) {
    std::string key(entry.first);
    to_lower_inplace(key);
    xheaders_[key] = entry.second;
  }
  return *this;
}


std::string& XHeaders::operator[] (const std::string& v) {
  std::string key(v);
  to_lower_inplace(key);
  return xheaders_[key];
}

size_t XHeaders::size() const {
  return xheaders_.size();
}

XHeaders::const_iterator XHeaders::find (const std::string& k) const {
  std::string key(k);
  to_lower_inplace(key);
  return xheaders_.find(key);
}

XHeaders::const_iterator XHeaders::begin() const {
  return xheaders_.begin();
}

XHeaders::const_iterator XHeaders::end() const {
  return xheaders_.end();
}

// NOLINTNEXTLINE(modernize-use-equals-default)
XHeaders::~XHeaders() {}

Error_xheader::Error_xheader(const char* msg) : invalid_argument(msg) {}
}
