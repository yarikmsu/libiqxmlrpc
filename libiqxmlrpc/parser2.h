//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef IQXMLRPC_PARSER2_H
#define IQXMLRPC_PARSER2_H

#include <memory>
#include <string>
#include <vector>

namespace iqxmlrpc {

class Parser;

class BuilderBase {
public:
  //! Maximum allowed XML nesting depth (DoS protection).
  //! XML-RPC typically needs ~10 levels; 32 is generous for legitimate use.
  static constexpr int MAX_PARSE_DEPTH = 32;

  //! Maximum allowed XML element count (DoS protection).
  //! Prevents "wide" XML attacks with excessive elements.
  //! 10 million supports large batch operations (e.g., 25K lines × 30 params × 4 elements).
  static constexpr int MAX_ELEMENT_COUNT = 10000000;

  BuilderBase(Parser&, bool expect_text = false);
  virtual ~BuilderBase() = default;

  void
  visit_element(const std::string& tag);

  void
  visit_element_end(const std::string& tag);

  void
  visit_text(const std::string&);

  //! Move version for efficiency when text data is a temporary.
  void
  visit_text(std::string&&);

  bool
  expects_text() const
  {
    return expect_text_;
  }

  int
  depth() const
  {
    return depth_;
  }

  bool
  wants_exit() const
  {
    return want_exit_;
  }

  void
  build(bool flat = false);

protected:
  template <class R, class BUILDER>
  R
  sub_build(bool flat = false)
  {
    BUILDER b(parser_);
    b.build(flat);
    return b.result();
  }

  void
  want_exit()
  {
    want_exit_ = true;
  }

  virtual void
  do_visit_element(const std::string&) = 0;

  virtual void
  do_visit_element_end(const std::string&);

  virtual void
  do_visit_text(const std::string&);

  //! Move version for efficient text handling. Default forwards to const-ref version.
  virtual void
  do_visit_text(std::string&&);

  Parser& parser_;
  int depth_;
  bool expect_text_;
  bool want_exit_;
};

class Parser {
public:
  explicit Parser(const std::string& buf);

  void
  parse(BuilderBase& builder);

  std::string
  get_data();

  std::string
  context() const;

  //! Returns the current XML nesting depth from libxml2.
  int
  xml_depth() const;

  //! Returns the total element count seen so far.
  int
  element_count() const;

  //! Increments and returns the element count.
  int
  increment_element_count();

private:
  class Impl;
  std::shared_ptr<Impl> impl_;
};

class StateMachine {
public:
  struct StateTransition {
    int prev_state;
    int new_state;
    const char* tag;
  };

  StateMachine(const Parser&, int start_state);

  void
  set_transitions(const StateTransition*);

  int
  get_state() const { return curr_; }

  int
  change(const std::string& tag);

  void
  set_state(int new_state);

private:
  typedef const StateTransition* TransitionMap;

  const Parser& parser_;
  int curr_;
  TransitionMap trans_ = nullptr;
};

} // namespace iqxmlrpc

#endif
// vim:sw=2:ts=2:et:
