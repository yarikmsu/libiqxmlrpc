#include <iostream>
#include <libiqxmlrpc/libiqxmlrpc.h>
#include <libiqxmlrpc/http_client.h>

int main()
{
  using namespace iqxmlrpc;

  Client<Http_client_connection> client(iqnet::Inet_addr(3344));

  // Resource limits (recommended for production, see docs/HARDENING_GUIDE.md)
  // Defaults are 0/unlimited for backward compatibility.
  client.set_timeout(30);                              // 30 seconds
  client.set_max_response_sz(10 * 1024 * 1024);        // 10 MB

  Param_list pl;
  pl.push_back(Struct());
  pl[0].insert("var1", 1);
  pl[0].insert("var2", "value");

  Response r = client.execute("echo", pl);

  assert(r.value()["var1"].get_int() == 1);
  std::cout << "OK" << std::endl;
}
