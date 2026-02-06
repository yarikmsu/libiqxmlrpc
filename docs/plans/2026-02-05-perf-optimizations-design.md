# Performance Optimizations Design (Without Replacing libxml2)

Date: 2026-02-05
Status: Planned

## Context

Profiling shows 77% of CPU in XML parsing (libxml2) and 23% in XML serialization.
These optimizations target code *around* libxml2 and how we *use* libxml2 APIs.

---

## A. Eliminate xmlStrdup in Parser (HIGH PRIORITY)

**File:** `libiqxmlrpc/parser2.cc`
**Profile evidence:** 344 samples in `xmlStrdup` within `read_data()`

**Problem:** `xmlTextReaderName()` and `xmlTextReaderValue()` call `xmlStrdup()` internally,
allocating a copy of the string. We then copy it into `std::string` and free the xmlChar*.
This means every parse step does: malloc (xmlStrdup) -> memcpy (std::string ctor) -> free (xmlFree).

**Solution:** Use `xmlTextReaderConstName()` and `xmlTextReaderConstValue()` which return
pointers to libxml2's internal buffers without allocation. We still copy into std::string,
but skip the xmlStrdup + xmlFree cycle.

**Risk:** Low. `xmlTextReaderConstName()` returns a dictionary-interned string valid
for the reader's lifetime. `xmlTextReaderConstValue()` returns a pointer valid until
the next `xmlTextReaderRead()` call. Both are safe since we copy into std::string
immediately.

**Expected impact:** 10-20% reduction in parsing memory pressure, measurable RPS improvement.

---

## B. Cache HTTP Date String (HIGH PRIORITY)

**File:** `libiqxmlrpc/http.cc`, `Response_header::current_date()`

**Problem:** Every response calls `std::time()` + `gmtime_r()` + `strftime()` to format
the HTTP Date header. Under 10K RPS, that's 10K system calls per second.

**Solution:** Cache the formatted date string. Update once per second using atomic
compare-exchange on the timestamp. Use `std::atomic<time_t>` for the last-update time
and a thread-safe cached string.

**Risk:** Low. HTTP date only needs 1-second precision per RFC 7231.

**Expected impact:** Eliminates 99%+ of time/strftime syscalls.

---

## C. Eliminate CSP Mutex Lock Per Response (HIGH PRIORITY)

**File:** `libiqxmlrpc/http.cc`, `Response_header` constructor, lines ~596-608

**Problem:** A `std::mutex` is acquired on every response to check if CSP policy is set.
CSP policy is set once at startup and never changes during server lifetime.

**Solution:** Use an `std::atomic<bool>` flag to check if CSP is enabled. Only acquire
the mutex when the flag is true (which means CSP was configured). Since CSP is set
once at startup, the flag check is sufficient for the common case.

**Risk:** Very low. Read-after-write ordering is guaranteed by acquire/release semantics.

**Expected impact:** Eliminates mutex contention on every response.

---

## D. Avoid HSTS String Formatting Per Response (MEDIUM PRIORITY)

**File:** `libiqxmlrpc/http.cc`, `Response_header` constructor

**Problem:** When HSTS is enabled, every response does:
`"max-age=" + std::to_string(s_hsts_max_age.load(...))` — creating temporary strings.
HSTS max-age is a constant set once at startup.

**Solution:** Cache the full HSTS header value string when `enable_hsts()` is called.
Use a `std::atomic<bool>` flag + cached string (same pattern as CSP fix).

**Risk:** Very low.

**Expected impact:** Eliminates 2 string temporaries per response when HSTS is enabled.

---

## E. Reuse XML Visitor in Serialization (LOW PRIORITY)

**File:** `libiqxmlrpc/value_type_xml.cc`, `do_visit_struct()` and `do_visit_array()`

**Problem:** A new `Value_type_to_xml` visitor is created for each struct/array traversal.
The visitor is just 2 pointers (builder + server_mode), so construction is cheap,
but it's unnecessary.

**Solution:** Use `*this` instead of creating a new visitor:
```cpp
value->apply_visitor(*this);  // instead of: value->apply_visitor(vis);
```

**Risk:** Need to verify visitor has no mutable state that would conflict.

**Expected impact:** Minimal. Eliminates one small allocation per container element.

---

## F. Response_header Fallback Path (LOW PRIORITY)

**File:** `libiqxmlrpc/http.cc`, `Response_header::dump_head()`, line 639

**Problem:** Fallback string concatenation creates 4+ temporary strings.

**Solution:** Use larger snprintf buffer or std::string::reserve.

**Risk:** Very low. Only affects error/edge cases.

**Expected impact:** Negligible (fallback path is rarely hit).

---

## Implementation Order

1. **A** — xmlTextReaderConst* (biggest measured impact, hot path)
2. **B** — Date caching (simple, high-frequency syscall elimination)
3. **C** — CSP mutex (simple, reduces contention)
4. **D** — HSTS string caching (simple, follows same pattern as C)
5. **E** — Visitor reuse (verify safety first)
6. **F** — Fallback path (low priority)
