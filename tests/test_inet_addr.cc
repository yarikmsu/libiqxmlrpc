#define BOOST_TEST_MODULE inet_addr_test
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/inet_addr.h"
#include "libiqxmlrpc/net_except.h"

#if defined(WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

using namespace boost::unit_test;
using namespace iqnet;

BOOST_AUTO_TEST_SUITE(inet_addr_construction_tests)

BOOST_AUTO_TEST_CASE(construct_with_host_and_port)
{
    Inet_addr addr("127.0.0.1", 8080);
    BOOST_CHECK_EQUAL(addr.get_host_name(), "127.0.0.1");
    BOOST_CHECK_EQUAL(addr.get_port(), 8080);
}

BOOST_AUTO_TEST_CASE(construct_with_port_only)
{
    Inet_addr addr(9000);
    BOOST_CHECK_EQUAL(addr.get_port(), 9000);
    BOOST_CHECK_EQUAL(addr.get_host_name(), "0.0.0.0");
}

BOOST_AUTO_TEST_CASE(construct_with_localhost)
{
    Inet_addr addr("localhost", 80);
    BOOST_CHECK_EQUAL(addr.get_host_name(), "localhost");
    BOOST_CHECK_EQUAL(addr.get_port(), 80);
}

BOOST_AUTO_TEST_CASE(construct_with_zero_port)
{
    Inet_addr addr("127.0.0.1", 0);
    BOOST_CHECK_EQUAL(addr.get_port(), 0);
}

BOOST_AUTO_TEST_CASE(construct_from_sockaddr)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(1234);
    sa.sin_addr.s_addr = inet_addr("192.168.1.1");

    Inet_addr addr(sa);
    BOOST_CHECK_EQUAL(addr.get_host_name(), "192.168.1.1");
    BOOST_CHECK_EQUAL(addr.get_port(), 1234);
}

BOOST_AUTO_TEST_CASE(default_construction)
{
    Inet_addr addr;
    // Default construction should not crash
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(inet_addr_host_name_tests)

BOOST_AUTO_TEST_CASE(ip_address_preserved)
{
    Inet_addr addr("10.0.0.1", 80);
    BOOST_CHECK_EQUAL(addr.get_host_name(), "10.0.0.1");
}

BOOST_AUTO_TEST_CASE(hostname_preserved)
{
    Inet_addr addr("example.local", 8080);
    BOOST_CHECK_EQUAL(addr.get_host_name(), "example.local");
}

BOOST_AUTO_TEST_CASE(hostname_with_crlf_throws)
{
    BOOST_CHECK_THROW(Inet_addr("host\nname", 80), network_error);
    BOOST_CHECK_THROW(Inet_addr("host\rname", 80), network_error);
    BOOST_CHECK_THROW(Inet_addr("host\r\nname", 80), network_error);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(inet_addr_port_tests)

BOOST_AUTO_TEST_CASE(various_ports)
{
    BOOST_CHECK_EQUAL(Inet_addr("127.0.0.1", 80).get_port(), 80);
    BOOST_CHECK_EQUAL(Inet_addr("127.0.0.1", 443).get_port(), 443);
    BOOST_CHECK_EQUAL(Inet_addr("127.0.0.1", 8080).get_port(), 8080);
    BOOST_CHECK_EQUAL(Inet_addr("127.0.0.1", 65535).get_port(), 65535);
}

BOOST_AUTO_TEST_CASE(port_only_construction)
{
    BOOST_CHECK_EQUAL(Inet_addr(80).get_port(), 80);
    BOOST_CHECK_EQUAL(Inet_addr(443).get_port(), 443);
    BOOST_CHECK_EQUAL(Inet_addr(8080).get_port(), 8080);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(inet_addr_sockaddr_tests)

BOOST_AUTO_TEST_CASE(get_sockaddr_port_only)
{
    Inet_addr addr(8080);
    const struct sockaddr_in* sa = addr.get_sockaddr();
    BOOST_REQUIRE(sa != nullptr);
    BOOST_CHECK_EQUAL(ntohs(sa->sin_port), 8080);
    BOOST_CHECK_EQUAL(sa->sin_family, PF_INET);
    BOOST_CHECK_EQUAL(sa->sin_addr.s_addr, INADDR_ANY);
}

BOOST_AUTO_TEST_CASE(get_sockaddr_with_localhost)
{
    Inet_addr addr("127.0.0.1", 80);
    const struct sockaddr_in* sa = addr.get_sockaddr();
    BOOST_REQUIRE(sa != nullptr);
    BOOST_CHECK_EQUAL(ntohs(sa->sin_port), 80);
    BOOST_CHECK_EQUAL(sa->sin_family, PF_INET);
}

BOOST_AUTO_TEST_CASE(get_sockaddr_from_existing_sockaddr)
{
    struct sockaddr_in original;
    memset(&original, 0, sizeof(original));
    original.sin_family = AF_INET;
    original.sin_port = htons(5000);
    original.sin_addr.s_addr = inet_addr("10.20.30.40");

    Inet_addr addr(original);
    const struct sockaddr_in* sa = addr.get_sockaddr();
    BOOST_REQUIRE(sa != nullptr);
    BOOST_CHECK_EQUAL(ntohs(sa->sin_port), 5000);
    char buf[INET_ADDRSTRLEN];
    const char* result = inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf));
    BOOST_REQUIRE(result != nullptr);
    BOOST_CHECK_EQUAL(std::string(buf), "10.20.30.40");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(inet_addr_copy_tests)

BOOST_AUTO_TEST_CASE(copy_constructor)
{
    Inet_addr original("192.168.1.100", 3000);
    Inet_addr copy(original);  // NOLINT(performance-unnecessary-copy-initialization) - copy constructor under test
    BOOST_CHECK_EQUAL(copy.get_host_name(), original.get_host_name());
    BOOST_CHECK_EQUAL(copy.get_port(), original.get_port());
}

BOOST_AUTO_TEST_CASE(assignment_operator)
{
    Inet_addr original("10.0.0.1", 4000);
    Inet_addr assigned("0.0.0.0", 0);
    assigned = original;
    BOOST_CHECK_EQUAL(assigned.get_host_name(), original.get_host_name());
    BOOST_CHECK_EQUAL(assigned.get_port(), original.get_port());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(get_host_name_function_tests)

BOOST_AUTO_TEST_CASE(get_local_hostname)
{
    std::string hostname = get_host_name();
    BOOST_CHECK(!hostname.empty());
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et
