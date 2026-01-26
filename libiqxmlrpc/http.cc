//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include "sysinc.h"

#include "http_errors.h"
#include "method.h"
#include "safe_math.h"
#include "version.h"

#include "num_conv.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <ctime>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>

namespace iqxmlrpc {
namespace http {

namespace {

// Helper: convert string to lowercase in-place
inline void to_lower_inplace(std::string& s) {
  std::transform(s.begin(), s.end(), s.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
}

// Helper: check if all characters are digits
inline bool all_digits(const std::string& s) {
  return !s.empty() && std::all_of(s.begin(), s.end(),
    [](unsigned char c) { return std::isdigit(c); });
}

// Helper: check if string starts with prefix
inline bool starts_with(const std::string& s, const char* prefix) {
  size_t len = std::strlen(prefix);
  return s.size() >= len && s.compare(0, len, prefix) == 0;
}

// Helper: check if string contains substring
inline bool contains(const std::string& s, const char* sub) {
  return s.find(sub) != std::string::npos;
}

// Helper: split string by whitespace, compressing consecutive delimiters
template<typename Container>
void split_by_whitespace(Container& result, const std::string& s) {
  result.clear();
  size_t start = 0;
  size_t len = s.size();

  while (start < len) {
    // Skip leading whitespace
    while (start < len && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    if (start >= len) break;

    // Find end of token
    size_t end = start;
    while (end < len && !std::isspace(static_cast<unsigned char>(s[end]))) ++end;

    result.push_back(s.substr(start, end - start));
    start = end;
  }
}

// Helper: split string by any character in delimiters
template<typename Container>
void split_by_chars(Container& result, const std::string& s, const char* delims) {
  result.clear();
  size_t start = 0;
  size_t len = s.size();

  while (start < len) {
    // Find next delimiter
    size_t end = s.find_first_of(delims, start);
    if (end == std::string::npos) end = len;

    if (end > start) {
      result.push_back(s.substr(start, end - start));
    }
    start = end + 1;
  }
}

// SECURITY: Validate header name/value against CRLF injection
// Throws Http_header_error if \r or \n found (prevents HTTP header injection)
inline void validate_header_crlf(const std::string& name, const std::string& value) {
  if (name.find_first_of("\r\n") != std::string::npos) {
    throw Http_header_error("Header name contains invalid CRLF characters");
  }
  if (value.find_first_of("\r\n") != std::string::npos) {
    throw Http_header_error("Header value contains invalid CRLF characters");
  }
}

} // anonymous namespace

namespace names {
  const char crlf[]           = "\r\n";
  const char content_length[] = "content-length";
  const char content_type[]   = "content-type";
  const char connection[]     = "connection";
  const char host[]           = "host";
  const char user_agent[]     = "user-agent";
  const char server[]         = "server";
  const char date[]           = "date";
  const char authorization[]  = "authorization";
  const char expect_continue[]= "expect";
} // namespace names

// SECURITY: Thread-safe static configuration for server version disclosure
// Note: Strings protected by mutex; bools/ints are atomic for lock-free reads
static std::mutex s_config_mutex;
static std::string s_custom_server_header;
static std::atomic<bool> s_hide_server_version{false};

void Header::set_server_header(const std::string& header)
{
  // SECURITY: Validate early for fail-fast behavior
  if (header.find_first_of("\r\n") != std::string::npos) {
    throw Http_header_error("Server header contains invalid CRLF characters");
  }
  std::lock_guard<std::mutex> lock(s_config_mutex);
  s_custom_server_header = header;
}

void Header::hide_server_version(bool hide)
{
  s_hide_server_version.store(hide, std::memory_order_relaxed);
}

// SECURITY: Thread-safe static configuration for HSTS and CSP headers
static std::atomic<bool> s_hsts_enabled{false};
static std::atomic<int> s_hsts_max_age{31536000};  // 1 year
static std::string s_csp_policy;

void Header::enable_hsts(bool enable, int max_age)
{
  s_hsts_enabled.store(enable, std::memory_order_relaxed);
  s_hsts_max_age.store(max_age, std::memory_order_relaxed);
}

void Header::set_content_security_policy(const std::string& policy)
{
  // SECURITY: Validate early for fail-fast behavior
  if (policy.find_first_of("\r\n") != std::string::npos) {
    throw Http_header_error("CSP policy contains invalid CRLF characters");
  }
  std::lock_guard<std::mutex> lock(s_config_mutex);
  s_csp_policy = policy;
}

namespace validator {

void unsigned_number(const std::string& val)
{
  const char errmsg[] = "bad format of numeric option";

  if (!all_digits(val))
    throw Malformed_packet(errmsg);

  try {
    num_conv::from_string<unsigned>(val);
  } catch (const num_conv::conversion_error&) {
    throw Malformed_packet(errmsg);
  }
}

void content_type(const std::string& val)
{
  std::string cont_type(val);
  to_lower_inplace(cont_type);

  if (!contains(cont_type, "text/xml"))
    throw Unsupported_content_type(cont_type);
}

void expect_continue(const std::string& val)
{
  std::string exp(val);
  to_lower_inplace(exp);

  if (!starts_with(exp, "100-continue"))
    throw Expectation_failed();
}

} // namespace validator


Header::Header(Verification_level lev):
  head_line_(),
  options_(),
  validators_(),
  ver_level_(lev)
{
  set_option_default(names::connection, "close");
  register_validator(names::content_length, validator::unsigned_number, HTTP_CHECK_WEAK);
  register_validator(names::expect_continue, validator::expect_continue, HTTP_CHECK_WEAK);
  register_validator(names::content_type, validator::content_type, HTTP_CHECK_STRICT);
}

Header::~Header() = default;

void Header::register_validator(
  const std::string& name,
  Header::Option_validator_fn fn,
  Verification_level level)
{
  Option_validator v = { level, std::move(fn) };
  validators_.insert(std::make_pair(name, v));
}

// Single-pass HTTP header parser
// Replaces 5-pass approach (split, find, trim×2, lowercase) with one scan
void Header::parse(const std::string& s)
{
  const char* data = s.data();
  const char* end = data + s.size();
  const char* pos = data;

  // Helper: find end of current line (returns pointer to \r or \n, or end)
  auto find_line_end = [end](const char* p) {
    while (p < end && *p != '\r' && *p != '\n') ++p;
    return p;
  };

  // Helper: skip \r\n or \n
  auto skip_newline = [end](const char* p) {
    if (p < end && *p == '\r') ++p;
    if (p < end && *p == '\n') ++p;
    return p;
  };

  // Helper: check if character is whitespace (space or tab)
  auto is_ws = [](char c) { return c == ' ' || c == '\t'; };

  // Parse first line (request/response line)
  const char* line_end = find_line_end(pos);
  head_line_.assign(pos, line_end);
  pos = skip_newline(line_end);

  // Parse header options
  while (pos < end) {
    line_end = find_line_end(pos);

    // Skip empty lines (can occur with \r\n\r\n sequences)
    if (pos == line_end) {
      pos = skip_newline(line_end);
      continue;
    }

    // Find colon separator
    const char* colon = pos;
    while (colon < line_end && *colon != ':') ++colon;

    if (colon == line_end)
      throw Malformed_packet("option line does not contain a colon symbol");

    // Extract option name: trim whitespace and convert to lowercase in one pass
    const char* name_start = pos;
    const char* name_end = colon;

    // Skip leading whitespace
    while (name_start < name_end && is_ws(*name_start)) ++name_start;
    // Skip trailing whitespace
    while (name_end > name_start && is_ws(*(name_end - 1))) --name_end;

    // Build lowercase name
    std::string opt_name;
    opt_name.reserve(name_end - name_start);
    for (const char* p = name_start; p < name_end; ++p) {
      char c = *p;
      // Fast ASCII lowercase: 'A'-'Z' have bit 5 clear, 'a'-'z' have it set
      if (c >= 'A' && c <= 'Z') c |= 0x20;
      opt_name.push_back(c);
    }

    // Extract option value: trim whitespace
    const char* value_start = colon + 1;
    const char* value_end = line_end;

    // Skip leading whitespace
    while (value_start < value_end && is_ws(*value_start)) ++value_start;
    // Skip trailing whitespace
    while (value_end > value_start && is_ws(*(value_end - 1))) --value_end;

    std::string opt_value(value_start, value_end);

    set_option_checked(opt_name, opt_value);

    pos = skip_newline(line_end);
  }
}

template <class T>
T Header::get_option(const std::string& name) const
{
  auto i = options_.find(name);

  if (i == options_.end()) {
      throw Malformed_packet("Missing mandatory header option '" + name + "'.");
  }

  try {
    if constexpr (std::is_same_v<T, std::string>) {
      return i->second;
    } else {
      return num_conv::from_string<T>(i->second);
    }
  } catch (const num_conv::conversion_error&) {
    throw Malformed_packet("Header option '" + name + "' has wrong format.");
  }
}

std::string Header::get_string(const std::string& name) const
{
  return get_option<std::string>(name);
}

unsigned Header::get_unsigned(const std::string& name) const
{
  return get_option<unsigned>(name);
}

inline
void Header::set_option_checked(const std::string& name, const std::string& value)
{
  std::pair<Validators::const_iterator, Validators::const_iterator> v =
    validators_.equal_range(name);

  for (auto i = v.first; i != v.second; ++i)
  {
    if (i->second.level <= ver_level_)
      i->second.fn(value);
  }

  set_option(name, value);
}

void Header::set_option(const std::string& name, const std::string& value)
{
  validate_header_crlf(name, value);
  options_[name] = value;
}

void Header::set_option(const std::string& name, size_t value)
{
  set_option(name, num_conv::to_string(value));
}

bool Header::option_exists(const std::string& name) const
{
  return options_.find(name) != options_.end();
}

void Header::set_option_default(const std::string& name, const std::string& value)
{
  validate_header_crlf(name, value);
  // insert() is a no-op if key exists, avoiding double lookup
  options_.insert({name, value});
}

std::string Header::dump() const
{
  std::string retval = dump_head();
  // Pre-reserve space: estimate ~64 bytes per option (with overflow protection)
  if (!safe_math::would_overflow_mul(options_.size(), size_t(64))) {
    size_t options_size = options_.size() * 64;
    if (!safe_math::would_overflow_add(retval.size(), options_size) &&
        !safe_math::would_overflow_add(retval.size() + options_size, size_t(4))) {
      retval.reserve(retval.size() + options_size + 4);
    }
  }

  for (const auto& opt : options_) {
    retval += opt.first;
    retval += ": ";
    retval += opt.second;
    retval += names::crlf;
  }

  retval += names::crlf;
  return retval;
}

void Header::set_content_length(size_t len)
{
  set_option(names::content_length, len);

  if (len)
    set_option(names::content_type, "text/xml");
}

void Header::set_conn_keep_alive(bool c)
{
  set_option(names::connection, c ? "keep-alive" : "close");
}

unsigned Header::content_length() const
{
  if (!option_exists(names::content_length))
    throw Length_required();

  return get_unsigned(names::content_length);
}

bool Header::conn_keep_alive() const
{
  return get_string(names::connection) == "keep-alive";
}

bool Header::expect_continue() const
{
  return option_exists(names::expect_continue);
}

// ----------------------------------------------------------------------------
Request_header::Request_header(Verification_level lev, const std::string& to_parse):
  Header(lev),
  uri_()
{
  parse(to_parse);
  set_option_default(names::host, "");
  set_option_default(names::user_agent, "unknown");

  // parse method
  typedef std::deque<std::string> Token;
  Token method_line;
  split_by_whitespace(method_line, get_head_line());

  if (method_line.empty())
    throw Bad_request();

  if( method_line[0] != "POST" )
    throw Method_not_allowed();

  if (method_line.size() > 1)
    uri_ = method_line[1];
}

Request_header::Request_header(
  const std::string& req_uri,
  const std::string& vhost,
  int port
):
  uri_(req_uri)
{
  std::string host_opt = vhost + ":" + std::to_string(port);
  set_option(names::host, host_opt);
  set_option(names::user_agent, PACKAGE " " VERSION);
}

std::string Request_header::dump_head() const
{
  return "POST " + uri() + " HTTP/1.0" + names::crlf;
}

std::string Request_header::host() const
{
  return get_string(names::host);
}

std::string Request_header::agent() const
{
  return get_string(names::user_agent);
}

void Header::get_xheaders(iqxmlrpc::XHeaders& xheaders) const
{
  // Clear first to match original assignment semantics (xheaders = options_)
  // This prevents stale X-headers from persisting when object is reused
  xheaders = std::map<std::string, std::string>();

  // Copy only X-headers (Options is now unordered_map, XHeaders uses map)
  for (const auto& opt : options_) {
    if (iqxmlrpc::XHeaders::validate(opt.first)) {
      xheaders[opt.first] = opt.second;
    }
  }
}

void Header::set_xheaders(const iqxmlrpc::XHeaders& xheaders)
{
  for (const auto& header : xheaders) {
    set_option(header.first, header.second);
  }
}

bool Request_header::has_authinfo() const
{
  return option_exists(names::authorization);
}

void Request_header::get_authinfo(std::string& user, std::string& pw) const
{
  if (!has_authinfo())
    throw Unauthorized();

  std::vector<std::string> v;
  std::string authstring = get_string(names::authorization);
  split_by_chars(v, authstring, " \t");

  if (v.size() != 2)
    throw Unauthorized();

  to_lower_inplace(v[0]);
  if (v[0] != "basic")
    throw Unauthorized();

  std::unique_ptr<Binary_data> bin_authinfo( Binary_data::from_base64(v[1]) );
  std::string data = bin_authinfo->get_data();

  size_t colon_pos = data.find(':');
  if (colon_pos == std::string::npos) {
    user = data;
    pw.clear();
  } else {
    user = data.substr(0, colon_pos);
    pw = data.substr(colon_pos + 1);
  }
}

void Request_header::set_authinfo(const std::string& u, const std::string& p)
{
  std::string h = u + ":" + p;
  std::unique_ptr<Binary_data> bin_authinfo( Binary_data::from_data(h) );
  set_option( names::authorization, "Basic " + bin_authinfo->get_base64() );
}

// ---------------------------------------------------------------------------
Response_header::Response_header(Verification_level lev, const std::string& to_parse):
  Header(lev),
  code_(0),
  phrase_()
{
  parse(to_parse);
  set_option_default(names::server, "unknown");

  typedef std::deque<std::string> Token;
  Token resp_line;
  split_by_whitespace(resp_line, get_head_line());

  if (resp_line.size() < 2) {
    throw Malformed_packet("Bad response");
  }

  try {
    code_ = num_conv::from_string<int>(resp_line[1]);
  } catch (const num_conv::conversion_error&) {
    code_ = 0;
  }

  if (resp_line.size() > 2)
    phrase_ = resp_line[2];
}

Response_header::Response_header( int c, const std::string& p ):
  code_(c),
  phrase_(p)
{
  set_option(names::date, current_date());

  // SECURITY: Allow hiding or customizing server version disclosure
  // Use atomic load for bool, mutex-protected copy for string
  if (!s_hide_server_version.load(std::memory_order_relaxed)) {
    std::string server_header;
    {
      std::lock_guard<std::mutex> lock(s_config_mutex);
      server_header = s_custom_server_header;
    }
    // NOLINTNEXTLINE(bugprone-branch-clone) - false positive: different string arguments
    if (server_header.empty()) {
      set_option(names::server, PACKAGE " " VERSION);
    } else {
      set_option(names::server, server_header);
    }
  }

  // SECURITY: Add standard security headers for defense-in-depth
  // These protect against content type sniffing and framing attacks
  set_option("x-content-type-options", "nosniff");
  set_option("x-frame-options", "DENY");
  set_option("cache-control", "no-store");

  // SECURITY: Optional HSTS header (only enable for HTTPS servers)
  if (s_hsts_enabled.load(std::memory_order_relaxed)) {
    set_option("strict-transport-security",
               "max-age=" + std::to_string(s_hsts_max_age.load(std::memory_order_relaxed)));
  }

  // SECURITY: Optional Content-Security-Policy header
  {
    std::lock_guard<std::mutex> lock(s_config_mutex);
    if (!s_csp_policy.empty()) {
      set_option("content-security-policy", s_csp_policy);
    }
  }
}

std::string Response_header::current_date()
{
  // Use strftime directly instead of boost::posix_time + locale/facet
  // This avoids expensive std::locale construction (~10μs -> ~100ns)
  std::time_t now = std::time(nullptr);
  std::tm gmt{};
#ifdef _WIN32
  gmtime_s(&gmt, &now);
#else
  gmtime_r(&now, &gmt);
#endif
  char buf[30];  // "Fri, 10 Jan 2026 12:30:45 GMT" = 29 chars + null
  size_t len = std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &gmt);
  return std::string(buf, len);
}

std::string Response_header::dump_head() const
{
  // Use snprintf instead of ostringstream for performance (M4 optimization)
  // Avoids allocation overhead and locale baggage (~3x faster)
  char buf[128];
  int len = snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\n",
                     code(), phrase().c_str());
  if (len >= 0 && len < static_cast<int>(sizeof(buf))) {
    return std::string(buf, static_cast<size_t>(len));
  }
  // Fallback to string concatenation for long phrases or encoding errors
  // Preserves original status code instead of silently changing to 500
  return "HTTP/1.1 " + std::to_string(code()) + " " + phrase() + "\r\n";
}

std::string Response_header::server() const
{
  return get_string(names::server);
}

// ---------------------------------------------------------------------------
Packet::Packet( Header* h, const std::string& co ):
  header_(h),
  content_(co)
{
  header_->set_content_length(content_.length());
}

Packet::~Packet() = default;

void Packet::set_keep_alive( bool keep_alive )
{
  header_->set_conn_keep_alive( keep_alive );
}

// ---------------------------------------------------------------------------
void Packet_reader::clear()
{
  header = nullptr;
  content_cache.erase();
  header_cache.erase();
  constructed = false;
  total_sz = 0;
}

void Packet_reader::check_sz( size_t sz )
{
  if( !pkt_max_sz )
    return;

  if (header) {
    // Use safe arithmetic to prevent integer overflow when summing sizes
    size_t total_expected;
    if (safe_math::would_overflow_add(header->content_length(), header_cache.length())) {
      throw Request_too_large();
    }
    total_expected = header->content_length() + header_cache.length();
    if (total_expected >= pkt_max_sz)
      throw Request_too_large();
  }

  // Use safe arithmetic for cumulative size tracking
  if (safe_math::would_overflow_add(total_sz, sz)) {
    throw Request_too_large();
  }
  total_sz += sz;
  if( total_sz >= pkt_max_sz )
    throw Request_too_large();
}

bool Packet_reader::read_header( const std::string& s )
{
  header_cache += s;

  // Find header/body separator (RFC 7230: CRLF CRLF)
  // Also handle common non-compliant variations for robustness
  size_t sep_pos = header_cache.find("\r\n\r\n");
  size_t sep_len = 4;

  // Check for mixed line endings: CRLF followed by LF (3 chars)
  if (sep_pos == std::string::npos) {
    sep_pos = header_cache.find("\r\n\n");
    sep_len = 3;
  }

  // Check for Unix-style: LF LF (2 chars)
  if (sep_pos == std::string::npos) {
    sep_pos = header_cache.find("\n\n");
    sep_len = 2;
  }

  // SECURITY: Check header size limit to prevent header-based DoS attacks
  if (header_max_sz) {
    // NOLINTNEXTLINE(bugprone-branch-clone) - false positive: different size conditions
    if (sep_pos == std::string::npos) {
      // No separator found yet - if accumulated data exceeds limit, reject
      // (headers alone shouldn't be this big)
      if (header_cache.length() > header_max_sz) {
        throw Request_too_large();
      }
    } else {
      // Separator found - check actual header size
      if (sep_pos > header_max_sz) {
        throw Request_too_large();
      }
    }
  }

  if (sep_pos == std::string::npos)
    return false;

  // Content starts after the separator
  size_t content_start = sep_pos + sep_len;
  content_cache.append(header_cache, content_start, std::string::npos);
  header_cache.erase(sep_pos);
  return true;
}

template <class Header_type>
Packet* Packet_reader::read_packet( const std::string& s, bool hdr_only )
{
  if( constructed )
    clear();

  check_sz( s.length() );

  if( !header )
  {
    if( s.empty() )
      throw http::Malformed_packet();

    if (read_header(s))
      header = new Header_type(ver_level_, header_cache);
  }
  else
    content_cache += s;

  if( header )
  {
    if ( hdr_only )
    {
      constructed = true;
      return new Packet( header, std::string() );
    }

    bool ready = (header->content_length() == 0 && s.empty()) ||
                 content_cache.length() >= header->content_length();

    if( ready )
    {
      content_cache.erase( header->content_length(), std::string::npos );
      auto* packet = new Packet( header, content_cache );
      constructed = true;
      return packet;
    }
  }

  return nullptr;
}

bool Packet_reader::expect_continue() const
{
  return header && header->expect_continue() && !continue_sent_;
}

void Packet_reader::set_continue_sent()
{
  continue_sent_ = true;
}

Packet* Packet_reader::read_request( const std::string& s )
{
  return read_packet<Request_header>(s);
}

Packet* Packet_reader::read_response( const std::string& s, bool hdr_only )
{
  return read_packet<Response_header>(s, hdr_only);
}

} // namespace http
} // namespace iqxmlrpc

// vim:ts=2:sw=2:et
