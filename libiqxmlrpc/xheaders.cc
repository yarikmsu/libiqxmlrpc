//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include "xheaders.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace iqxmlrpc
{

namespace {

inline void to_lower_inplace(std::string& s) {
  std::transform(s.begin(), s.end(), s.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
}

inline bool starts_with(const std::string& s, const char* prefix) {
  size_t len = std::strlen(prefix);
  return s.size() >= len && s.compare(0, len, prefix) == 0;
}

} // anonymous namespace

XHeaders& XHeaders::operator=(const std::map<std::string, std::string>& v) {
  xheaders_.clear();
  for (const auto& entry : v) {
    if (validate(entry.first)) {
      std::string key(entry.first);
      to_lower_inplace(key);
      xheaders_[key] = entry.second;
    }
  }
  return *this;
}


std::string& XHeaders::operator[] (const std::string& v) {
  if (!validate(v)) {
    throw Error_xheader("The header doesn't starts with `X-`");
  }
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

bool XHeaders::validate(const std::string& val) {
  return starts_with(val, "X-") || starts_with(val, "x-");
}

Error_xheader::Error_xheader(const char* msg) : invalid_argument(msg) {}
}
