#define BOOST_TEST_MODULE socket_test

#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/socket.h"
#include "libiqxmlrpc/inet_addr.h"
#include "libiqxmlrpc/net_except.h"

using namespace iqnet;

BOOST_AUTO_TEST_SUITE(socket_basic_tests)

// Test socket creation (covers socket.cc lines 15-33)
BOOST_AUTO_TEST_CASE(socket_default_constructor)
{
    Socket sock;
    BOOST_CHECK(sock.get_handler() != -1);
    sock.close();
}

// Test socket close operation (covers socket.cc lines 46-53)
BOOST_AUTO_TEST_CASE(socket_close)
{
    Socket sock;
    Socket::Handler h = sock.get_handler();
    BOOST_CHECK(h != -1);
    sock.close();
    // Socket should be closed now
}

// Test socket shutdown operation (covers socket.cc lines 41-44)
BOOST_AUTO_TEST_CASE(socket_shutdown)
{
    Socket sock;
    // Shutdown should not throw even on unconnected socket
    sock.shutdown();
    sock.close();
}

// Test set_non_blocking with flag=false (covers socket.cc lines 62-63)
BOOST_AUTO_TEST_CASE(socket_set_blocking)
{
    Socket sock;
    // Setting non-blocking to false should be a no-op on Unix (line 62-63)
    sock.set_non_blocking(false);
    sock.close();
}

// Test set_non_blocking with flag=true (covers socket.cc lines 65-66)
BOOST_AUTO_TEST_CASE(socket_set_non_blocking)
{
    Socket sock;
    sock.set_non_blocking(true);
    sock.close();
}

// Test get_last_error on a fresh socket (covers socket.cc lines 196-210)
BOOST_AUTO_TEST_CASE(socket_get_last_error)
{
    Socket sock;
    int err = sock.get_last_error();
    // Fresh socket should have no error
    BOOST_CHECK_EQUAL(err, 0);
    sock.close();
}

// Test get_last_error errno fallback when getsockopt fails (covers socket.cc line 204)
// Uses an invalid socket descriptor to trigger the getsockopt failure path
BOOST_AUTO_TEST_CASE(socket_get_last_error_errno_fallback)
{
    // Create socket with invalid handler (-1) to trigger getsockopt failure
    Inet_addr dummy_addr("127.0.0.1", 0);
    Socket invalid_sock(-1, dummy_addr);

    // getsockopt on fd=-1 will fail, triggering the errno fallback
    int err = invalid_sock.get_last_error();

    // Should return EBADF (bad file descriptor) since -1 is invalid
    BOOST_CHECK_EQUAL(err, EBADF);

    // Don't call close() - the socket descriptor is invalid
}

// Test get_addr on a bound socket (covers socket.cc lines 152-161)
BOOST_AUTO_TEST_CASE(socket_get_addr_bound)
{
    Socket sock;
    Inet_addr addr("127.0.0.1", 0);  // Port 0 = let OS choose
    sock.bind(addr);

    Inet_addr bound_addr = sock.get_addr();
    BOOST_CHECK_EQUAL(bound_addr.get_host_name(), "127.0.0.1");
    // Port should be assigned by OS (non-zero)
    BOOST_CHECK_NE(bound_addr.get_port(), 0);

    sock.close();
}

// Test bind operation (covers socket.cc lines 104-110)
BOOST_AUTO_TEST_CASE(socket_bind)
{
    Socket sock;
    Inet_addr addr("127.0.0.1", 0);
    sock.bind(addr);
    sock.close();
}

// Test listen operation (covers socket.cc lines 112-116)
BOOST_AUTO_TEST_CASE(socket_listen)
{
    Socket sock;
    Inet_addr addr("127.0.0.1", 0);
    sock.bind(addr);
    sock.listen(5);
    sock.close();
}

// Test send_shutdown operation (covers socket.cc lines 96-102)
BOOST_AUTO_TEST_CASE(socket_send_shutdown)
{
    // Create a connected socket pair for testing send_shutdown
    Socket server_sock;
    Inet_addr addr("127.0.0.1", 0);
    server_sock.bind(addr);
    server_sock.listen(1);
    Inet_addr server_addr = server_sock.get_addr();

    Socket client_sock;
    client_sock.set_non_blocking(true);
    try {
        client_sock.connect(server_addr);
    } catch (...) {
        // Non-blocking connect may throw or return false
        (void)0;
    }

    Socket accepted = server_sock.accept();

    // Test send_shutdown on accepted socket
    const char* data = "test";
    accepted.send_shutdown(data, 4);

    client_sock.close();
    accepted.close();
    server_sock.close();
}

// Test connect operation (covers socket.cc lines 130-150)
BOOST_AUTO_TEST_CASE(socket_connect_nonblocking)
{
    Socket server_sock;
    Inet_addr addr("127.0.0.1", 0);
    server_sock.bind(addr);
    server_sock.listen(1);
    Inet_addr server_addr = server_sock.get_addr();

    Socket client_sock;
    client_sock.set_non_blocking(true);

    // Non-blocking connect returns false when connection is in progress
    bool immediate = client_sock.connect(server_addr);
    // Either immediate connection or in-progress is valid
    (void)immediate;

    client_sock.close();
    server_sock.close();
}

// Test accept operation (covers socket.cc lines 118-128)
BOOST_AUTO_TEST_CASE(socket_accept)
{
    Socket server_sock;
    Inet_addr addr("127.0.0.1", 0);
    server_sock.bind(addr);
    server_sock.listen(1);
    Inet_addr server_addr = server_sock.get_addr();

    Socket client_sock;
    client_sock.connect(server_addr);

    Socket accepted = server_sock.accept();
    BOOST_CHECK(accepted.get_handler() != -1);

    // Check peer address
    Inet_addr peer = accepted.get_peer_addr();
    BOOST_CHECK_EQUAL(peer.get_host_name(), "127.0.0.1");

    client_sock.close();
    accepted.close();
    server_sock.close();
}

// Test send and recv operations (covers socket.cc lines 76-94)
BOOST_AUTO_TEST_CASE(socket_send_recv)
{
    Socket server_sock;
    Inet_addr addr("127.0.0.1", 0);
    server_sock.bind(addr);
    server_sock.listen(1);
    Inet_addr server_addr = server_sock.get_addr();

    Socket client_sock;
    client_sock.connect(server_addr);

    Socket accepted = server_sock.accept();

    // Send data
    const char* send_data = "Hello, World!";
    size_t sent = client_sock.send(send_data, 13);
    BOOST_CHECK_EQUAL(sent, 13u);

    // Receive data
    char recv_buf[64];
    size_t received = accepted.recv(recv_buf, sizeof(recv_buf));
    BOOST_CHECK_EQUAL(received, 13u);
    BOOST_CHECK_EQUAL(std::string(recv_buf, received), "Hello, World!");

    client_sock.close();
    accepted.close();
    server_sock.close();
}

// Test Socket constructor with handler (covers socket.cc lines 35-39)
BOOST_AUTO_TEST_CASE(socket_handler_constructor)
{
    Socket sock1;
    Socket::Handler h = sock1.get_handler();
    Inet_addr addr("127.0.0.1", 12345);

    // Create socket using handler constructor
    Socket sock2(h, addr);
    BOOST_CHECK_EQUAL(sock2.get_handler(), h);
    BOOST_CHECK_EQUAL(sock2.get_peer_addr().get_port(), 12345);

    // Only close sock2 since it now owns the handler
    sock2.close();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(socket_error_tests)

// Test bind failure on already bound address
BOOST_AUTO_TEST_CASE(socket_bind_error)
{
    Socket sock1;
    Inet_addr addr("127.0.0.1", 0);
    sock1.bind(addr);
    sock1.listen(1);
    Inet_addr bound_addr = sock1.get_addr();

    Socket sock2;
    // Trying to bind to the same address should fail
    BOOST_CHECK_THROW(sock2.bind(bound_addr), network_error);

    sock1.close();
    sock2.close();
}

// Test connect failure to non-existent server
BOOST_AUTO_TEST_CASE(socket_connect_error)
{
    Socket sock;
    // Port 1 is privileged and unlikely to have a listening server
    Inet_addr addr("127.0.0.1", 59999);

    // This should throw or the connection should fail
    // The exact behavior depends on timing
    try {
        bool result = sock.connect(addr);
        // If connect returns, the connection is in progress
        (void)result;
    } catch (const network_error&) {
        // Expected for connection refused
        (void)0;
    }

    sock.close();
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et
