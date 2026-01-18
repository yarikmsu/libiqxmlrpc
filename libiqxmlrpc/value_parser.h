//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef IQXMLRPC_VALUE_PARSER_H
#define IQXMLRPC_VALUE_PARSER_H

#include <memory>

#include "parser2.h"
#include "value.h"

namespace iqxmlrpc {

class ValueBuilderBase: public BuilderBase {
public:
  explicit ValueBuilderBase(Parser& parser, bool expect_text = false);
  ~ValueBuilderBase() override = default;

  Value_type*
  result()
  {
    return retval.release();
  }

protected:
  std::unique_ptr<Value_type> retval;
};

class ValueBuilder: public ValueBuilderBase {
public:
  explicit ValueBuilder(Parser& parser);

private:
  void
  do_visit_element(const std::string&) override;

  void
  do_visit_element_end(const std::string&) override;

  void
  do_visit_text(const std::string&) override;

  StateMachine state_;
};

} // namespace iqxmlrpc

#endif
// vim:sw=2:ts=2:et:
