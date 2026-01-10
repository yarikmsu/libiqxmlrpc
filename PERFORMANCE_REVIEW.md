# Performance Review: libiqxmlrpc

**Date:** 2026-01-10 (Updated: 2026-01-10)
**Version Analyzed:** 0.13.6

### Changelog
| Date | Change |
|------|--------|
| 2026-01-10 | Completed: Add benchmark for atomic vs mutex comparison (PR #50) |
| 2026-01-10 | Completed: Optimize Value::cast<T>() with type tags (29% faster array access) (PR #48) |
| 2026-01-10 | Completed: Use atomic<bool> for thread pool destructor flag (4.3x faster) (PR #46) |
| 2026-01-10 | Completed: Enable TCP_NODELAY by default for RPC latency (PR #45) |
| 2026-01-10 | Completed: Use unordered_map for Struct (53% faster access) (PR #44) |
| 2026-01-10 | Completed: Optimize Base64 decoding with lookup table (PR #42) |
| 2026-01-10 | Completed: Migrate Struct to unique_ptr for memory safety (PR #40) |
| 2026-01-09 | Completed: Add move semantics and optimize DateTime parsing (PR #35) |
| 2026-01-09 | Completed: Use strftime for HTTP date formatting (PR #34) |
| 2026-01-09 | Completed: Replace `dynamic_cast` type checking with type tags (PR #33) |
| 2026-01-09 | Completed: Replace `boost::lexical_cast` with standard library (PR #29) |

## Executive Summary

This performance review analyzes the libiqxmlrpc library, an object-oriented XML-RPC implementation in C++17. The library demonstrates solid architectural foundations with room for targeted optimizations. The analysis covers XML parsing, memory management, network I/O, threading, string handling, and data structures.

---

## Table of Contents

1. [XML Parsing Performance](#1-xml-parsing-performance)
2. [Memory Allocation Patterns](#2-memory-allocation-patterns)
3. [Network I/O Efficiency](#3-network-io-efficiency)
4. [Threading and Concurrency](#4-threading-and-concurrency)
5. [String Handling Efficiency](#5-string-handling-efficiency)
6. [Data Structure Analysis](#6-data-structure-analysis)
7. [HTTP Protocol Implementation](#7-http-protocol-implementation)
8. [Recommendations Summary](#8-recommendations-summary)
9. [Benchmarking Suggestions](#9-benchmarking-suggestions)

---

## 1. XML Parsing Performance

### Current Implementation

The library uses **libxml2's xmlTextReader API** for streaming XML parsing (`parser2.cc:111`), which is a good choice for memory efficiency when handling large XML documents.

**Strengths:**
- Uses `XML_PARSE_NONET | XML_PARSE_HUGE` flags for security and large document support
- XXE protection enabled via `xmlTextReaderSetParserProp(reader, XML_PARSER_SUBST_ENTITIES, 0)`
- Streaming parser avoids loading entire document into memory

**Areas for Improvement:**

1. **String Conversion Overhead** (`parser2.cc:16-26`)
   ```cpp
   inline std::string to_string(xmlChar* s) {
     if (s) {
       std::string retval(reinterpret_cast<const char*>(s));
       xmlFree(s);
       return retval;
     }
     return std::string();
   }
   ```
   - Creates a copy before freeing the original
   - Consider using `std::string_view` where possible for read-only access

2. **Tag Name Processing** (`parser2.cc:188-199`)
   ```cpp
   std::string tag_name() {
     std::string rv = to_string(xmlTextReaderName(reader));
     size_t pos = rv.find_first_of(":");
     if (pos != std::string::npos) {
       rv.erase(0, pos+1);
     }
     return rv;
   }
   ```
   - Namespace stripping performs string search and erase on every element
   - Consider caching common tag names or using a string pool

3. **StateMachine Linear Search** (`parser2.cc:287-306`)
   ```cpp
   for (; trans_[i].tag != 0; ++i) {
     if (trans_[i].tag == tag && trans_[i].prev_state == curr_) {
       found = true;
       break;
     }
   }
   ```
   - Linear search through transition table
   - For small transition tables this is acceptable, but could use a hash map for larger state machines

### Recommendations

| Priority | Issue | Recommendation | Impact |
|----------|-------|----------------|--------|
| Medium | String copies in parser | Use `std::string_view` where lifetime permits | 10-15% parse time reduction |
| Low | Namespace stripping | Pre-compute namespace-free tag names or cache results | Minor improvement |
| Low | State transition lookup | Consider `std::unordered_map` for large state machines | Negligible for current use |

---

## 2. Memory Allocation Patterns

### Current Implementation

**Value Type Hierarchy** (`value_type.h`)
- Uses raw pointer-based polymorphism with virtual `clone()` method
- `Value` class owns a `Value_type*` and manages lifetime

**Array and Struct Types:**
```cpp
// Array: stores vector of Value*
typedef std::vector<Value*> Val_vector;

// Struct: stores map of string to Value*
typedef std::map<std::string, Value*> Value_stor;
```

### Issues Identified

1. **Raw Pointer Ownership** (`value_type.h:83-158`)
   - Arrays and Structs store raw pointers requiring manual memory management
   - Every insertion creates a `new Value`, adding allocation overhead

2. **Clone-Based Copy Semantics** (`value.cc:69-72`)
   ```cpp
   Value::Value( const Value& v ):
     value( v.value->clone() )
   {}
   ```
   - Deep copy requires heap allocation for every copy
   - Consider move semantics more aggressively

3. **Excessive Allocations in Parsing** (`value_parser.cc:55-56`, `120-121`)
   ```cpp
   Value_ptr v(new Value(value_));   // Allocates Value
   proxy_->insert(name_, v);         // May allocate map node
   ```

4. **ExplicitPtr Custom Smart Pointer** (`util.h:43-65`)
   - Custom implementation when `std::unique_ptr` would suffice
   - Transfer semantics in copy constructor could be confusing

### Positive Patterns

1. **Move Semantics on Array/Struct** (`value_type.h:111, 234`)
   ```cpp
   Array( Array&& other ) noexcept : values(std::move(other.values)) {}
   Struct( Struct&& other ) noexcept : values(std::move(other.values)) {}
   ```

2. **Pre-allocation in Base64** (`value_type.cc:396, 487`)
   ```cpp
   base64.reserve((dsz * 4) / 3 + 4);  // Encoding
   data.reserve((dsz * 3) / 4);        // Decoding
   ```

3. **Memory Release on Clear** (`value_type.cc:174-180`)
   ```cpp
   void Array::clear() {
     util::delete_ptrs(values.begin(), values.end());
     std::vector<Value*>().swap(values);  // Forces memory release
   }
   ```

### Recommendations

| Priority | Issue | Recommendation | Impact |
|----------|-------|----------------|--------|
| High | Raw pointer storage | Consider `std::unique_ptr<Value>` in containers | Safety (primary), enables optimization patterns |
| Medium | Clone overhead | Add move constructors to Value, use COW or shared_ptr where appropriate | Significant for large values |

#### Note on Smart Pointers and Performance

The recommendation to use smart pointers is **primarily about safety** (preventing leaks, clarifying ownership), not direct performance improvement. Raw pointer access is zero-cost, and `unique_ptr::get()` compiles to identical code.

**When smart pointers enable performance gains:**

1. **Move semantics** - `unique_ptr` moves are just pointer swaps, enabling efficient ownership transfer:
   ```cpp
   // Current: may copy
   Value_ptr v(new Value(value_));
   proxy_->insert(name_, v);

   // With unique_ptr: clear move semantics
   proxy_->values.emplace(name_, std::make_unique<Value>(std::move(value_)));
   ```

2. **Copy-on-write with shared_ptr** - Avoid deep copies when multiple readers share data:
   ```cpp
   // Current: always deep copy
   Value::Value(const Value& v) : value(v.value->clone()) {}

   // With shared_ptr: O(1) copy, clone only on modification
   Value::Value(const Value& v) : value(v.value) {}  // ref count increment
   ```

3. **Compiler optimization** - Clear ownership semantics can help the optimizer reason about lifetimes

**When it doesn't help (or adds overhead):**

| Scenario | Impact |
|----------|--------|
| Raw pointer dereference | No difference |
| `shared_ptr` ref counting | Adds atomic operations |
| Very tight loops | Cache effects dominate |
| Construction | `make_unique` ≈ `new` |
| Medium | Replace ExplicitPtr | Use `std::unique_ptr` throughout | Code clarity, no runtime impact |
| Low | Allocation pooling | Consider memory pool for Value allocations in hot paths | High-throughput scenarios |

---

## 3. Network I/O Efficiency

### Current Implementation

**Socket Layer** (`socket.h`, `socket.cc`)
- Thin wrapper over POSIX/Winsock sockets
- Uses `SO_REUSEADDR`, `SO_NOSIGPIPE` (macOS), `MSG_NOSIGNAL`

**Reactor Pattern** (`reactor_impl.h`)
- Supports both `poll()` and `select()` implementations
- Template-based design allows lock-free (Serial) or mutex-protected (Pool) operation

### Analysis

1. **Buffer Sizes Not Optimized**
   - No explicit TCP buffer tuning (`SO_RCVBUF`, `SO_SNDBUF`)
   - Default system values used

2. **No TCP_NODELAY Option** (Significant for RPC latency)
   - Nagle's algorithm buffers small packets, waiting for ACKs or more data
   - Combined with delayed ACK on the receiver, this can add **40-400ms latency per call**
   - XML-RPC messages are typically small and follow request-response pattern - exactly where Nagle hurts most
   - Should be configurable (enabled by default for RPC workloads)

3. **Single send/recv per call** (`socket.cc:76-94`)
   - No scatter-gather I/O (`writev`/`readv`)
   - For small XML-RPC messages, this is acceptable

4. **Reactor Efficiency** (`reactor_impl.h:253-279`)
   ```cpp
   bool Reactor<Lock>::handle_system_events(Timeout ms) {
     scoped_lock lk(lock);
     HandlerStateList tmp(handlers_states);  // Full copy
     lk.unlock();
     // ...
   }
   ```
   - Full copy of handler list on every poll cycle
   - Consider COW or double-buffering for high connection counts

5. **HTTP Packet Reading** (`http.cc:439-451`)
   ```cpp
   bool Packet_reader::read_header( const std::string& s ) {
     header_cache += s;  // String concatenation
     // ...
     content_cache.append(i.end(), header_cache.end());
   }
   ```
   - Progressive header building through string concatenation
   - Could pre-allocate based on typical header sizes

### Recommendations

| Priority | Issue | Recommendation | Impact |
|----------|-------|----------------|--------|
| High | TCP_NODELAY | Add option to disable Nagle's algorithm (default on for RPC) | **40-400ms latency reduction per call** |
| Medium | Handler list copy | Use copy-on-write or version counter for handler updates | High connection scalability |
| Low | Buffer tuning | Add socket buffer size configuration | Throughput optimization |
| Low | Vectored I/O | Consider `writev` for header+body transmission | Minor efficiency gain |

---

## 4. Threading and Concurrency

### Current Implementation

**Execution Models** (`executor.h`, `executor.cc`)

1. **Serial_executor**: Single-threaded, no locking
2. **Pool_executor_factory**: Thread pool with condition variable-based work queue

**Thread Pool Implementation** (`executor.cc:70-115`)
```cpp
void Pool_executor_factory::Pool_thread::operator ()() {
  for(;;) {
    scoped_lock lk(pool_ptr->req_queue_lock);
    if (pool_ptr->req_queue.empty()) {
      pool_ptr->req_queue_cond.wait(lk);
      // ...
    }
    Pool_executor* executor = pool_ptr->req_queue.front();
    pool_ptr->req_queue.pop_front();
    lk.unlock();
    executor->process_actual_execution();
  }
}
```

### Analysis

1. **Work Queue Contention**
   - Single mutex for entire queue
   - All threads compete for same lock
   - Consider lock-free queue or work stealing

2. **Destructor Flag Check** (`executor.cc:179-183`)
   ```cpp
   bool Pool_executor_factory::is_being_destructed() {
     scoped_lock lk(destructor_lock);  // Mutex to read a single bool
     return in_destructor;
   }
   ```
   - Using a mutex to protect a bool that's written once (at shutdown) is overkill
   - `std::atomic<bool>` is the right tool: single atomic load (~1-5 cycles) vs mutex (~50-200 cycles)
   - **Note:** This is a minor optimization since the check only occurs after waking from `wait()`, not in a tight loop. The benefit is primarily **code clarity** (using appropriate synchronization primitives)

3. **No Thread Affinity or Priority**
   - Default thread scheduling
   - May want CPU affinity for high-performance scenarios

4. **Reactor Lock Granularity** (`reactor_impl.h`)
   - Lock type is template parameter (good design)
   - But registration/unregistration locks entire map

### Positive Patterns

1. **Traits-Based Configuration**
   ```cpp
   struct Serial_executor_traits { typedef iqnet::Null_lock Lock; };
   struct Pool_executor_traits { typedef boost::mutex Lock; };
   ```

2. **Proper Shutdown Sequence** (`executor.cc:126-134`)
   ```cpp
   Pool_executor_factory::~Pool_executor_factory() {
     destruction_started();
     threads.join_all();
     // cleanup...
   }
   ```

### Recommendations

| Priority | Issue | Recommendation | Impact |
|----------|-------|----------------|--------|
| Medium | Queue contention | Consider lock-free queue (boost::lockfree or moodycamel) | Better multi-core scaling |
| Low | Destructor flag | Use `std::atomic<bool>` | Code clarity (primary), minor overhead reduction |
| Low | Work stealing | Implement work-stealing for better load balance | Complex workload scenarios |
| Low | Thread naming | Add thread naming for debugging (`pthread_setname_np`) | Debugging ease |

---

## 5. String Handling Efficiency

### Current Implementation

The library uses `std::string` throughout, with several areas of concern:

### Issues Identified

1. **Repeated String Allocations in HTTP** (`http.cc:186-201`)
   ```cpp
   std::string Header::dump() const {
     std::string retval = dump_head();
     retval.reserve(retval.size() + options_.size() * 64 + 4);  // Good!
     for (const auto& opt : options_) {
       retval += opt.first;    // += causes realloc check
       retval += ": ";
       retval += opt.second;
       retval += names::crlf;
     }
     // ...
   }
   ```
   - Pre-reservation is good, but multiple `+=` operations still have overhead

2. **Date/Time Parsing** (`value_type.cc:544-558`)
   ```cpp
   Date_time::Date_time( const std::string& s ) {
     // Multiple substr calls, each allocating
     tm_.tm_year = atoi( s.substr(0, 4).c_str() ) - 1900;
     tm_.tm_mon  = atoi( s.substr(4, 2).c_str() ) - 1;
     // ...
   }
   ```
   - Each `substr()` allocates a new string
   - Consider using `std::string_view` or direct character parsing

3. **Boost String Algorithms** (`http.cc:96-119`)
   ```cpp
   boost::split(lines, s, boost::is_any_of(names::crlf), boost::token_compress_on);
   boost::trim(opt_name);
   boost::trim(opt_value);
   boost::to_lower(opt_name);
   ```
   - Multiple passes over string data
   - Consider single-pass parsing

4. **Value to String Conversions** (`value_type.cc:588-598`)
   ```cpp
   const std::string& Date_time::to_string() const {
     if( cache.empty() ) {
       char s[18];
       strftime( s, 18, "%Y%m%dT%H:%M:%S", &tm_ );
       cache = std::string( s, 17 );  // Lazy caching - good!
     }
     return cache;
   }
   ```
   - Lazy caching pattern is efficient

### Positive Patterns

1. **Type Name Constants** (`value_type.cc:17-28`)
   ```cpp
   namespace type_names {
     const std::string nil_type_name = "nil";
     // ... static strings, no repeated allocations
   }
   ```

2. **String Return by Reference**
   - `type_name()` returns `const std::string&`
   - `get_base64()`, `to_string()` return cached references

### Recommendations

| Priority | Issue | Recommendation | Impact |
|----------|-------|----------------|--------|
| Medium | substr allocations | Use `std::string_view` for parsing operations | Reduced allocations |
| Medium | HTTP header parsing | Single-pass parser with pre-allocated buffers | Parsing performance |
| Low | Multiple string passes | Combine trim+lowercase in single pass | Minor improvement |
| Low | Consider SSO | Ensure strings under 15-22 chars benefit from SSO | Already automatic |

---

## 6. Data Structure Analysis

### Current Choices

| Data Structure | Usage | Analysis |
|----------------|-------|----------|
| `std::vector<Value*>` | Array type | Good cache locality, O(1) append |
| `std::map<std::string, Value*>` | Struct type | O(log n) lookup, consider `unordered_map` |
| `std::map<std::string, std::string>` | HTTP headers | Reasonable for small header counts |
| `std::deque<Pool_executor*>` | Work queue | Good for producer-consumer pattern |
| `std::deque<std::string>` | HTTP header parsing | Unnecessary overhead |

### Issues Identified

1. **Struct Uses Ordered Map** (`value_type.h:223`)
   ```cpp
   typedef std::map<std::string, Value*> Value_stor;
   ```
   - Tree-based map has O(log n) lookup
   - For typical struct sizes (<20 fields), flat map or `unordered_map` may be faster

2. **Linear Search for Handler State** (`reactor_impl.h:93-97`)
   ```cpp
   hs_iterator find_handler_state(Event_handler* eh) {
     return std::find(begin(), end(), HandlerState(eh->get_handler()));
   }
   ```
   - O(n) search through handler list
   - Should use a map indexed by socket handler (one already exists for `find_handler`)
   - **Note:** For typical deployments (16-64 connections), this adds ~50 nanoseconds per register/unregister operation - negligible compared to request processing. Becomes significant only at 500+ connections with high connection churn.

3. **Array Index Access** (`value_type.h:125-145`)
   ```cpp
   const Value& operator []( unsigned i ) const {
     try {
       return (*values.at(i));  // at() checks bounds
     }
     catch( const std::out_of_range& ) {
       throw Out_of_range();  // Re-throw as custom exception
     }
   }
   ```
   - Exception-based bounds checking has overhead
   - Consider assert for debug, unchecked for release

### Recommendations

| Priority | Issue | Recommendation | Impact |
|----------|-------|----------------|--------|
| Medium | Struct map type | Benchmark `unordered_map` vs `map` for typical sizes | Lookup performance |
| Medium | Handler state search | Add map indexed by socket descriptor | O(1) lookup |
| Low | Array bounds check | Use assert in debug, raw access in release | Minor |
| Low | Small struct optimization | Consider flat_map for structs < 10 members | Cache efficiency |

---

## 7. HTTP Protocol Implementation

### Current Implementation

**Verification Levels** (`http.h:28`)
```cpp
enum Verification_level { HTTP_CHECK_WEAK, HTTP_CHECK_STRICT };
```

**Packet Construction** (`http.cc:394-409`)
- Header and content stored as separate strings
- Final dump concatenates header + body

### Analysis

1. **HTTP Version Mismatch**
   - Request header: `HTTP/1.0` (`http.cc:272`)
   - Response header: `HTTP/1.1` (`http.cc:384`)
   - Should be consistent

2. **Date Formatting Overhead** (`http.cc:367-379`)
   ```cpp
   std::string Response_header::current_date() const {
     ptime p = second_clock::universal_time();
     std::ostringstream ss;
     time_facet* tf = new time_facet("%a, %d %b %Y %H:%M:%S GMT");
     ss.imbue(std::locale(std::locale::classic(), tf));
     ss << p;
     return ss.str();
   }
   ```
   - Creates new locale/facet for every response
   - Should cache or use simpler formatting

3. **Authorization Parsing** (`http.cc:302-325`)
   - Base64 decoding happens on every request with auth
   - Consider caching decoded credentials briefly

4. **Content-Type Always Set** (`http.cc:206-209`)
   ```cpp
   if (len)
     set_option(names::content_type, "text/xml");
   ```
   - Good that empty content doesn't set type

### Positive Patterns

1. **100-Continue Support** (`http.cc:496-504`)
   - Proper HTTP/1.1 expect handling

2. **Keep-Alive Support**
   - Properly implemented for connection reuse

3. **Request Size Limiting** (`http.cc:420-432`)
   - DoS protection via `set_max_request_sz`

### Recommendations

| Priority | Issue | Recommendation | Impact |
|----------|-------|----------------|--------|
| Medium | Date formatting | Cache locale/facet, or use `strftime` directly | Response latency |
| Low | HTTP version | Make consistent (1.1) or configurable | Correctness |
| Low | Auth caching | Brief credential cache to avoid repeated decoding | Auth-heavy workloads |

---

## 8. Recommendations Summary

### Completed ✅

1. ~~**Replace `boost::lexical_cast` with `std::to_chars`/`std::from_chars`**~~ ✅ **DONE (PR #29)**
   - Files: `value_type_xml.cc`, `value_parser.cc`, `http.cc`, `https_client.cc`
   - New file: `libiqxmlrpc/num_conv.h`
   - **Measured Results:**
     | Operation | Before | After | Speedup |
     |-----------|--------|-------|---------|
     | int → string | 95 ns | 11 ns | **8.8x** |
     | int64 → string | 175 ns | 14 ns | **12.5x** |
     | string → int | 93 ns | 57 ns | **1.6x** |
     | string → int64 | 156 ns | 89 ns | **1.8x** |
     | double → string | 278 ns | 233 ns | **1.2x** |
     | string → double | 266 ns | 42 ns | **6.4x** |
   - Integration impact: `dump_response_1000` improved by **21%**

2. ~~**Replace `dynamic_cast` type checking with type tags**~~ ✅ **DONE (PR #33)**
   - Files: `value.cc`, `value_type.cc`, `value_type.h`
   - **Measured Results:**
     | Operation | Before | After | Speedup |
     |-----------|--------|-------|---------|
     | type_check (match) | 7.3 ns | 4.1 ns | **1.8x** |
     | type_check (no match) | 42.3 ns | 4.0 ns | **10.5x** |
     | type_check (mixed) | 270.7 ns | 32.7 ns | **8.3x** |
   - Added `ValueTypeTag` enum, `ScalarTypeTag<T>` trait, null-safety check

3. ~~**Cache date formatting in HTTP responses**~~ ✅ **DONE (PR #34)**
   - File: `http.cc`
   - **Measured Results:**
     | Operation | Before | After | Speedup |
     |-----------|--------|-------|---------|
     | http_response_header | 12.7 μs | 3.4 μs | **3.7x** |
     | http_response_dump | 14.7 μs | 5.4 μs | **2.7x** |
   - Replaced `boost::posix_time` + `std::locale`/`time_facet` with direct `strftime`
   - Removed `boost/date_time` dependency from `http.cc`

4. ~~**Use `std::from_chars` for DateTime parsing**~~ ✅ **DONE (PR #35)**
   - File: `value_type.cc`
   - **Measured Results:**
     | Operation | Before | After | Speedup |
     |-----------|--------|-------|---------|
     | datetime_parse | 1,137 ns | 1,125 ns | ~1% |
   - Replaced `atoi(substr().c_str())` with `std::from_chars`
   - Note: Minimal improvement because original `substr()` strings were SSO-optimized

5. ~~**Add move semantics to Value class and containers**~~ ✅ **DONE (PR #35)**
   - Files: `value.h`, `value.cc`, `value_type.h`, `value_type.cc`
   - **Measured Results:**
     | Operation | Before | After | Speedup |
     |-----------|--------|-------|---------|
     | array_push_back (100 elements) | 10,978 ns | 9,121 ns | **17%** |
     | struct_insert (20 fields) | 16,048 ns | 14,750 ns | **8%** |
   - Added `Value(Value&&)` move constructor and `operator=(Value&&)`
   - Added `Array::push_back(Value&&)` and `Struct::insert(string, Value&&)` overloads
   - Added 9 unit tests for move semantics

6. ~~**Replace raw pointers with smart pointers in Struct**~~ ✅ **DONE (PR #40)**
   - Files: `value_type.h`, `value_type.cc`, `value_parser.cc`
   - **Measured Results:**
     | Operation | Before | After | Change |
     |-----------|--------|-------|--------|
     | clone_struct_5 | 2,796 ns | 2,690 ns | **-3.8%** ✓ |
     | clone_struct_20 | 15,879 ns | 15,990 ns | +0.7% |
     | struct_insert | 14,231 ns | 14,343 ns | +0.8% |
     | struct_destroy | 14,293 ns | 14,467 ns | +1.2% |
   - Changed `Struct::Value_stor` from `std::map<string, Value*>` to `std::map<string, unique_ptr<Value>>`
   - Simplified `clear()`, `insert()`, `erase()` - no more manual `delete` calls
   - Removed `Struct_inserter` helper class
   - **Note:** Array keeps raw pointers as unique_ptr showed ~15-20% regression there due to vector reallocation overhead

7. ~~**Optimize Base64 decoding with lookup table**~~ ✅ **DONE (PR #42)**
   - Files: `value_type.h`, `value_type.cc`
   - **Measured Results:**
     | Operation | Before | After | Speedup |
     |-----------|--------|-------|---------|
     | base64_decode_1kb | 25,073 ns | 5,361 ns | **4.7x** |
     | base64_decode_64kb | 1,339,331 ns | 328,213 ns | **4.1x** |
   - Added 256-byte decode lookup table for O(1) character-to-value conversion
   - Replaced exception-based padding detection with if-statement checks
   - Used `reserve()` + `push_back()` instead of `resize()` to avoid zero-initialization

8. ~~**Use unordered_map for Struct**~~ ✅ **DONE (PR #44)**
   - Files: `value_type.h`
   - **Measured Results:**
     | Operation | Before | After | Speedup |
     |-----------|--------|-------|---------|
     | struct_access | 1,340 ns | 623 ns | **53% faster** |
     | struct_insert | 11,003 ns | 5,417 ns | **51% faster** |
     | clone_struct_20 | 11,607 ns | 5,090 ns | **56% faster** |
     | struct_destroy | 9,448 ns | 5,764 ns | **39% faster** |
   - Replaced `std::map` with `std::unordered_map` for O(1) average lookup
   - Trade-off: Iteration order no longer alphabetical (not important for XML-RPC)

9. ~~**Enable TCP_NODELAY by default**~~ ✅ **DONE (PR #45)**
   - Files: `socket.h`, `socket.cc`
   - **Impact:** 40-400ms latency reduction per RPC call
   - Disables Nagle's algorithm to eliminate buffering delays for small messages
   - Added `set_nodelay(bool)` method to Socket class
   - Enabled by default in constructor and `accept()`

10. ~~**Use atomic<bool> for destructor flag**~~ ✅ **DONE (PR #46)**
    - Files: `executor.h`, `executor.cc`
    - **Measured Results (PR #50 benchmark):**
      | Operation | Mutex (before) | Atomic (after) | Speedup |
      |-----------|----------------|----------------|---------|
      | Bool read | 13.65 ns | 3.21 ns | **4.3x faster** |
      | Bool write | 14.20 ns | 3.24 ns | **4.4x faster** |
    - Replaced mutex-protected bool with `std::atomic<bool>` for `in_destructor` flag
    - Uses proper memory ordering (release/acquire) for synchronization

11. ~~**Optimize Value::cast<T>() with type tags**~~ ✅ **DONE (PR #48)**
    - Files: `value.cc`, `value_type.h`
    - **Measured Results:**
      | Operation | Before | After | Speedup |
      |-----------|--------|-------|---------|
      | array_access | 11,543 ns | 8,244 ns | **29% faster** |
      | array_iterate | 19,745 ns | 16,869 ns | **15% faster** |
    - Added `TypeTag<T>` trait to map Value_type subclasses to their ValueTypeTag
    - Replaced `dynamic_cast` with type tag check + `static_cast` in `cast<T>()`
    - Benefits all getter methods (`get_int()`, `get_string()`, etc.)

### Remaining Low Priority

1. **Reduce per-connection buffer allocation**
   - File: `server_conn.cc`
   - Current: 65KB per connection (2MB for 32 connections)
   - Consider smaller initial size with dynamic growth

2. **Single-pass HTTP header parsing**
   - File: `http.cc`
   - Current: 5 passes (split, find, trim×2, lowercase)
   - Could parse in one pass with no intermediate allocations

---

## 9. Benchmarking Suggestions

### Existing Benchmarks

The project includes basic performance tests:
- `tests/parse_performance.cc` - XML response parsing
- `tests/dump_performance.cc` - XML response generation
- `tests/client_stress.cc` - Client load testing

### Recommended Additions

1. **Micro-benchmarks**
   ```cpp
   // Value creation/copy/move performance
   // Struct field access patterns
   // Array iteration performance
   ```

2. **Memory Profiling**
   - Use Valgrind/ASan to track allocations per request
   - Target: < 10 allocations for simple RPC call

3. **Latency Benchmarks**
   - Measure P50, P99 latency under load
   - Single request round-trip time

4. **Scaling Tests**
   - Thread pool scaling with core count
   - Connection count impact on reactor performance

### Suggested Tools

- **Google Benchmark** for micro-benchmarks
- **Valgrind/Massif** for heap profiling
- **perf** for CPU profiling on Linux
- **wrk/wrk2** for HTTP throughput testing

---

## Conclusion

The libiqxmlrpc library has a well-designed architecture with clear separation of concerns. The main performance opportunities lie in:

1. Reducing memory allocations through smart pointers and string_view
2. Optimizing threading for higher concurrency
3. Improving reactor efficiency for high connection counts
4. Caching frequently computed values (HTTP dates, parsed credentials)

The library is suitable for moderate-scale XML-RPC deployments. For high-performance scenarios (>10K requests/second), the recommended optimizations should be prioritized based on profiling data from actual workload patterns.
