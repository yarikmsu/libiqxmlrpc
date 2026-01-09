//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef _iqxmlrpc_request_parser_h_
#define _iqxmlrpc_request_parser_h_

#include <optional>
#include "parser2.h"
#include "request.h"

namespace iqxmlrpc {

class RequestBuilder: public BuilderBase {
public:
  explicit RequestBuilder(Parser&);

  Request*
  get();

private:
  void
  do_visit_element(const std::string&) override;

  StateMachine state_;
  std::optional<std::string> method_name_;
  Param_list params_;
};

} // namespace iqxmlrpc

#endif
