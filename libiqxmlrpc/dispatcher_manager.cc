//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include "dispatcher_manager.h"

#include "builtins.h"

#include <algorithm>
#include <deque>
#include <iterator>
#include <unordered_map>

namespace iqxmlrpc {

//
// Default method dispatcher
//

class Default_method_dispatcher: public Method_dispatcher_base {
  typedef std::unordered_map<std::string, Method_factory_base*> Factory_map;
  Factory_map fs;

public:
  Default_method_dispatcher() : fs() {}
  ~Default_method_dispatcher() override;

  void register_method(const std::string& name, Method_factory_base*);

private:
  Method*
  do_create_method(const std::string&) override;

  void
  do_get_methods_list(Array&) const override;
};

Default_method_dispatcher::~Default_method_dispatcher()
{
  util::delete_ptrs( fs.begin(), fs.end(),
    util::Select2nd<Factory_map>());
}

void Default_method_dispatcher::register_method
  ( const std::string& name, Method_factory_base* fb )
{
  // Use insert() to avoid double O(1) lookup
  auto result = fs.insert({name, fb});
  if (!result.second) {
    // Key already existed: delete old factory, update via iterator
    delete result.first->second;
    result.first->second = fb;
  }
}

Method* Default_method_dispatcher::do_create_method(const std::string& name)
{
  // Use iterator to avoid double O(1) lookup
  auto it = fs.find(name);
  if (it == fs.end())
    return nullptr;

  return it->second->create();
}

void Default_method_dispatcher::do_get_methods_list(Array& retval) const
{
  std::transform(fs.begin(), fs.end(), std::back_inserter(retval),
    [](const Factory_map::value_type& entry) { return Value(entry.first); });
}

//
// System method factory
//

template <class T>
class System_method_factory: public Method_factory_base {
  Method_dispatcher_manager* dmgr_;

public:
  explicit System_method_factory(Method_dispatcher_manager* dmgr):
    dmgr_(dmgr)
  {
  }

  System_method_factory(const System_method_factory&) = delete;
  System_method_factory& operator=(const System_method_factory&) = delete;

private:
  T* create() override
  {
    return new T(dmgr_);
  }
};

//
// Method dispatcher menager
//

class Method_dispatcher_manager::Impl {
public:
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  typedef std::deque<Method_dispatcher_base*> DispatchersSet;
  DispatchersSet dispatchers;
  Default_method_dispatcher* default_disp;

  Impl():
    dispatchers(),
    default_disp(new Default_method_dispatcher)
  {
    dispatchers.push_back(default_disp);
  }

  ~Impl()
  {
    util::delete_ptrs(dispatchers.begin(), dispatchers.end());
  }
};


Method_dispatcher_manager::Method_dispatcher_manager():
  impl_(new Impl)
{
}

Method_dispatcher_manager::~Method_dispatcher_manager()
{
  delete impl_;
}

void Method_dispatcher_manager::register_method(
  const std::string& name, Method_factory_base* mfactory)
{
  impl_->default_disp->register_method(name, mfactory);
}

void Method_dispatcher_manager::push_back(Method_dispatcher_base* mdisp)
{
  impl_->dispatchers.push_back(mdisp);
}

Method* Method_dispatcher_manager::create_method(const Method::Data& mdata)
{
  // SECURITY: Reject excessively long method names to prevent
  // memory exhaustion. Defense-in-depth: parse-time check in
  // request_parser.cc is the primary enforcement.
  // Must match MAX_METHOD_NAME_LEN in request_parser.cc
  constexpr size_t MAX_METHOD_NAME_LEN = 256;
  if (mdata.method_name.length() > MAX_METHOD_NAME_LEN) {
    throw Unknown_method(mdata.method_name);  // Sanitized in exception
  }

  for (const auto& dispatcher : impl_->dispatchers)
  {
    Method* tmp = dispatcher->create_method(mdata);
    if (tmp)
      return tmp;
  }

  throw Unknown_method(mdata.method_name);
}

void Method_dispatcher_manager::get_methods_list(Array& retval) const
{
  // cppcheck-suppress constVariableReference
  for (const auto& dispatcher : impl_->dispatchers)
    dispatcher->get_methods_list(retval);
}

void Method_dispatcher_manager::enable_introspection()
{
  impl_->default_disp->register_method("system.listMethods",
    new System_method_factory<builtins::List_methods>(this));
}

} // namespace iqxmlrpc
