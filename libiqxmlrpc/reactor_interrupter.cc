//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include "reactor_interrupter.h"
#include "connection.h"
#include "lock.h"
#include "socket.h"

#include <memory>
#include <mutex>

namespace iqnet {

class Interrupter_connection: public Connection {
public:
  Interrupter_connection(Reactor_base* r, const Socket& sock):
    Connection(sock), reactor_(r)
  {
    this->sock.set_non_blocking(true);
    reactor_->register_handler(this, Reactor_base::INPUT);
  }

  Interrupter_connection(const Interrupter_connection&) = delete;
  Interrupter_connection& operator=(const Interrupter_connection&) = delete;

  ~Interrupter_connection() override
  {
    reactor_->unregister_handler(this);
  }

  bool is_stopper() const override { return true; }

  void handle_input(bool& /* terminate */) override
  {
    char nothing;
    recv(&nothing, 1);
  }

private:
  Reactor_base* reactor_;
};

class Reactor_interrupter::Impl {
public:
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  explicit Impl(Reactor_base* reactor);
  ~Impl() {
    client_.shutdown();
    client_.close();
  }

  void make_interrupt();

private:
  std::unique_ptr<Interrupter_connection> server_;
  Socket client_;
  std::mutex lock_;
};


Reactor_interrupter::Impl::Impl(Reactor_base* reactor):
  server_(),
  client_(),
  lock_()
{
  Socket srv;
  srv.bind(Inet_addr("127.0.0.1", 0)); // bind to port 0, which means any port beyond 1024
  srv.listen(1);

  Inet_addr srv_addr(srv.get_addr());
  client_.connect( Inet_addr("127.0.0.1", srv_addr.get_port()) );
  Socket srv_conn(srv.accept());
  srv.close();  // Close listening socket - no longer needed after accept()

  server_ = std::make_unique<Interrupter_connection>(reactor, srv_conn);
}

void Reactor_interrupter::Impl::make_interrupt()
{
  std::lock_guard<std::mutex> lk(lock_);
  client_.send("\0", 1);
}


Reactor_interrupter::Reactor_interrupter(Reactor_base* r):
  impl_(std::make_unique<Impl>(r))
{
}

Reactor_interrupter::~Reactor_interrupter() = default;

void Reactor_interrupter::make_interrupt()
{
  impl_->make_interrupt();
}

} // namespace iqnet
