//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef IQXMLRPC_RESPONSE_PARSER_H
#define IQXMLRPC_RESPONSE_PARSER_H

#include <optional>
#include "value.h"
#include "parser2.h"
#include "response.h"

namespace iqxmlrpc {

class ResponseBuilder: public BuilderBase {
public:
  explicit ResponseBuilder(Parser&);

  Response
  get();

private:
  void
  do_visit_element(const std::string&) override;

  void
  parse_ok();

  void
  parse_fault();

  StateMachine state_;
  std::optional<Value> ok_;
  int fault_code_ = 0;
  std::optional<std::string> fault_str_;
};

} // namespace iqxmlrpc

#endif
