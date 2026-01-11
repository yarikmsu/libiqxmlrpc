#!/usr/bin/env python3
"""
XML-RPC test server for libiqxmlrpc compatibility testing.

This server implements various test methods to verify wire protocol
compatibility between libiqxmlrpc and Python's xmlrpc implementation.

Usage:
    python3 python_xmlrpc_server.py [--port PORT]

Default port: 8000
"""

import argparse
import sys
import datetime
import xmlrpc.server
import socketserver


class ThreadedXMLRPCServer(socketserver.ThreadingMixIn, xmlrpc.server.SimpleXMLRPCServer):
    """Thread-safe XML-RPC server."""
    daemon_threads = True
    allow_reuse_address = True


# =============================================================================
# Test Methods
# =============================================================================

def echo(value):
    """Echo back any value unchanged."""
    return value


def echo_int(i):
    """Echo an integer value."""
    return int(i)


def echo_double(d):
    """Echo a double/float value."""
    return float(d)


def echo_bool(b):
    """Echo a boolean value."""
    return bool(b)


def echo_string(s):
    """Echo a string value."""
    return str(s)


def echo_struct(s):
    """Echo a struct/dict value."""
    return dict(s)


def echo_array(a):
    """Echo an array/list value."""
    return list(a)


def echo_base64(b):
    """Echo base64 binary data."""
    return b


def echo_datetime(dt):
    """Echo a datetime value."""
    return dt


def add(a, b):
    """Add two numbers."""
    return a + b


def subtract(a, b):
    """Subtract two numbers."""
    return a - b


def multiply(a, b):
    """Multiply two numbers."""
    return a * b


def divide(a, b):
    """Divide two numbers."""
    if b == 0:
        raise xmlrpc.server.Fault(1, "Division by zero")
    return a / b


def get_all_types():
    """Return a struct containing all XML-RPC value types."""
    return {
        "int": 42,
        "negative_int": -123,
        "zero": 0,
        "bool_true": True,
        "bool_false": False,
        "double": 3.14159265358979,
        "negative_double": -2.71828,
        "string": "Hello, World!",
        "empty_string": "",
        "unicode_string": "日本語テスト",
        "special_chars": "<test>&\"'</test>",
        "array": [1, 2, 3, "four", 5.0],
        "empty_array": [],
        "nested_struct": {
            "level1": {
                "level2": {
                    "value": "deep"
                }
            }
        },
        "empty_struct": {}
    }


def get_large_response(size=1000):
    """Return a large array for testing large payloads."""
    return list(range(size))


def get_nested_struct(depth=5):
    """Return a deeply nested struct."""
    result = {"value": "leaf"}
    for i in range(depth):
        result = {"level": i, "child": result}
    return result


def raise_fault(code, message):
    """Raise an XML-RPC fault for testing error handling."""
    raise xmlrpc.server.Fault(int(code), str(message))


def system_listMethods():
    """List available methods (standard introspection)."""
    return [
        "echo", "echo_int", "echo_double", "echo_bool", "echo_string",
        "echo_struct", "echo_array", "echo_base64", "echo_datetime",
        "add", "subtract", "multiply", "divide",
        "get_all_types", "get_large_response", "get_nested_struct",
        "raise_fault", "system.listMethods"
    ]


def main():
    parser = argparse.ArgumentParser(description="XML-RPC test server")
    parser.add_argument("--port", type=int, default=8000,
                        help="Port to listen on (default: 8000)")
    parser.add_argument("--host", default="localhost",
                        help="Host to bind to (default: localhost)")
    args = parser.parse_args()

    server = ThreadedXMLRPCServer(
        (args.host, args.port),
        allow_none=True,
        logRequests=True
    )

    # Register all test methods
    server.register_function(echo)
    server.register_function(echo_int)
    server.register_function(echo_double)
    server.register_function(echo_bool)
    server.register_function(echo_string)
    server.register_function(echo_struct)
    server.register_function(echo_array)
    server.register_function(echo_base64)
    server.register_function(echo_datetime)
    server.register_function(add)
    server.register_function(subtract)
    server.register_function(multiply)
    server.register_function(divide)
    server.register_function(get_all_types)
    server.register_function(get_large_response)
    server.register_function(get_nested_struct)
    server.register_function(raise_fault)
    server.register_function(system_listMethods, "system.listMethods")

    print(f"XML-RPC test server running on http://{args.host}:{args.port}/")
    print("Press Ctrl+C to stop")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.shutdown()
        sys.exit(0)


if __name__ == "__main__":
    main()
