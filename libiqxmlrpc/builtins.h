//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef _iqxmlrpc_buildins_h_
#define _iqxmlrpc_buildins_h_

#include "method.h"

namespace iqxmlrpc {

class Method_dispatcher_manager;

namespace builtins {

//! Implementation of system.listMethods
//! See http://xmlrpc.usefulinc.com/doc/reserved.html
class LIBIQXMLRPC_API List_methods: public Method {
  Method_dispatcher_manager* disp_manager_;

public:
  explicit List_methods(Method_dispatcher_manager*);

  List_methods(const List_methods&) = delete;
  List_methods& operator=(const List_methods&) = delete;

private:
  void execute( const Param_list& params, Value& response ) override;
};

} // namespace builtins
} // namespace iqxmlrpc

#endif
