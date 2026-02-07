#!/usr/bin/env python3
"""
RPS (Requests Per Second) Benchmark for libiqxmlrpc server.

This script measures server throughput using Python's standard xmlrpc.client
library to send concurrent requests and measure RPS and latency percentiles.

Usage:
    python3 rps_benchmark.py --host localhost --port 8080 --requests 10000

Requirements: Python 3.6+ (uses standard library only)
"""

import argparse
import http.client
import json
import sys
import threading
import time
import xmlrpc.client
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Dict, List, NamedTuple


class KeepAliveTransport(xmlrpc.client.Transport):
    """
    HTTP Transport with keep-alive connection reuse.

    Standard xmlrpc.client.Transport creates a new connection per request.
    This transport maintains a persistent connection for the lifetime of
    the transport object, significantly reducing connection overhead.
    """

    def __init__(self, host: str, port: int):
        super().__init__()
        self._host = host
        self._port = port
        self._conn = None

    def make_connection(self, host):
        """Return a persistent HTTP connection."""
        if self._conn is None:
            self._conn = http.client.HTTPConnection(
                self._host, self._port,
                timeout=30
            )
        return self._conn

    def send_content(self, connection, request_body):
        """Send content with keep-alive header."""
        connection.putheader("Connection", "keep-alive")
        super().send_content(connection, request_body)

    def close(self):
        """Close the persistent connection."""
        if self._conn:
            self._conn.close()
            self._conn = None

    def single_request(self, host, handler, request_body, verbose=False):
        """Override to handle connection errors and reconnect."""
        try:
            return super().single_request(host, handler, request_body, verbose)
        except (http.client.RemoteDisconnected, ConnectionError, OSError):
            # Server closed connection, reconnect
            self.close()
            return super().single_request(host, handler, request_body, verbose)


class BenchmarkResult(NamedTuple):
    """Results from a single benchmark run."""
    rps: float
    total_requests: int
    elapsed_sec: float
    p50_ms: float
    p90_ms: float
    p95_ms: float
    p99_ms: float
    min_ms: float
    max_ms: float
    errors: int


def percentile(sorted_data: List[float], p: float) -> float:
    """Calculate percentile from sorted data."""
    if not sorted_data:
        return 0.0
    idx = int(len(sorted_data) * p / 100)
    idx = min(idx, len(sorted_data) - 1)
    return sorted_data[idx]


def run_benchmark(
    host: str,
    port: int,
    method: str,
    payload,
    num_requests: int,
    num_clients: int,
    warmup_requests: int = 100
) -> BenchmarkResult:
    """
    Run benchmark with given configuration.

    Args:
        host: Server hostname
        port: Server port
        method: XML-RPC method name to call
        payload: Payload to send with each request
        num_requests: Total number of requests to make
        num_clients: Number of concurrent client threads
        warmup_requests: Warmup requests before measuring (not counted)

    Returns:
        BenchmarkResult with RPS and latency metrics
    """
    url = f"http://{host}:{port}/RPC"
    all_latencies: List[float] = []
    errors = 0
    errors_lock = threading.Lock()

    def worker(requests_per_worker: int, is_warmup: bool = False) -> List[float]:
        """Worker thread that makes RPC calls and measures latency."""
        nonlocal errors
        local_latencies: List[float] = []

        # Use keep-alive transport for connection reuse within this worker
        transport = KeepAliveTransport(host, port)
        proxy = xmlrpc.client.ServerProxy(url, transport=transport)

        try:
            for _ in range(requests_per_worker):
                try:
                    start = time.perf_counter_ns()
                    getattr(proxy, method)(payload)
                    end = time.perf_counter_ns()
                    if not is_warmup:
                        local_latencies.append(end - start)
                except Exception as e:
                    with errors_lock:
                        errors += 1
                        should_log = not is_warmup and errors <= 3
                    if should_log:
                        print(f"Error: {type(e).__name__}: {e}", file=sys.stderr)
        finally:
            transport.close()

        return local_latencies

    # Warmup phase - spread warmup across clients
    if warmup_requests > 0:
        warmup_per_client = max(1, warmup_requests // num_clients)
        with ThreadPoolExecutor(max_workers=num_clients) as executor:
            list(executor.map(lambda _: worker(warmup_per_client, is_warmup=True),
                              range(num_clients)))

    # Reset errors after warmup (use lock for thread safety consistency)
    with errors_lock:
        errors = 0

    # Calculate requests per client
    requests_per_client = num_requests // num_clients
    remainder = num_requests % num_clients

    # Benchmark phase
    start_time = time.perf_counter()
    with ThreadPoolExecutor(max_workers=num_clients) as executor:
        futures = []
        for i in range(num_clients):
            # Distribute remainder across first few clients
            extra = 1 if i < remainder else 0
            futures.append(executor.submit(worker, requests_per_client + extra))

        for future in as_completed(futures):
            all_latencies.extend(future.result())
    end_time = time.perf_counter()

    elapsed = end_time - start_time
    successful_requests = len(all_latencies)

    if successful_requests == 0:
        return BenchmarkResult(
            rps=0, total_requests=0, elapsed_sec=elapsed,
            p50_ms=0, p90_ms=0, p95_ms=0, p99_ms=0,
            min_ms=0, max_ms=0, errors=errors
        )

    rps = successful_requests / elapsed

    # Convert to milliseconds and sort for percentile calculation
    latencies_ms = sorted(ns / 1_000_000 for ns in all_latencies)

    return BenchmarkResult(
        rps=rps,
        total_requests=successful_requests,
        elapsed_sec=elapsed,
        p50_ms=percentile(latencies_ms, 50),
        p90_ms=percentile(latencies_ms, 90),
        p95_ms=percentile(latencies_ms, 95),
        p99_ms=percentile(latencies_ms, 99),
        min_ms=latencies_ms[0],
        max_ms=latencies_ms[-1],
        errors=errors
    )


def print_header():
    """Print benchmark results table header."""
    print(f"{'Scenario':<20} | {'Clients':>7} | {'Requests':>8} | "
          f"{'RPS':>10} | {'p50':>8} | {'p90':>8} | {'p95':>8} | "
          f"{'p99':>8} | {'Errors':>6}")
    print("-" * 105)


def print_result(scenario: str, num_clients: int, result: BenchmarkResult):
    """Print a single benchmark result row."""
    print(f"{scenario:<20} | {num_clients:>7} | {result.total_requests:>8} | "
          f"{result.rps:>10.1f} | {result.p50_ms:>7.2f}ms | {result.p90_ms:>7.2f}ms | "
          f"{result.p95_ms:>7.2f}ms | {result.p99_ms:>7.2f}ms | {result.errors:>6}")


def run_scenario(
    name: str,
    host: str,
    port: int,
    payload,
    num_requests: int,
    client_counts: List[int],
    warmup: int
) -> Dict[int, BenchmarkResult]:
    """Run a benchmark scenario with multiple client counts."""
    results = {}
    for num_clients in client_counts:
        result = run_benchmark(
            host=host,
            port=port,
            method='echo',
            payload=payload,
            num_requests=num_requests,
            num_clients=num_clients,
            warmup_requests=warmup
        )
        results[num_clients] = result
        print_result(name, num_clients, result)
    return results


def main():
    parser = argparse.ArgumentParser(
        description='RPS Benchmark for libiqxmlrpc server',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --host localhost --port 8080
  %(prog)s --port 8080 --requests 50000 --clients 1,4,8,16
  %(prog)s --port 8080 --scenario small  # Only small payload
  %(prog)s --port 8080 --scenario large  # Only large payload
        """
    )
    parser.add_argument('--host', default='localhost',
                        help='Server hostname (default: localhost)')
    parser.add_argument('--port', type=int, default=8080,
                        help='Server port (default: 8080)')
    parser.add_argument('--requests', type=int, default=10000,
                        help='Number of requests per scenario (default: 10000)')
    parser.add_argument('--clients', default='1,2,4,8,16',
                        help='Comma-separated client counts (default: 1,2,4,8,16)')
    parser.add_argument('--warmup', type=int, default=100,
                        help='Warmup requests before measuring (default: 100)')
    parser.add_argument('--scenario', choices=['small', 'large', 'both'],
                        default='both',
                        help='Which scenario to run (default: both)')
    parser.add_argument('--large-requests', type=int, default=None,
                        help='Override request count for large payload scenario')
    parser.add_argument('--output', choices=['table', 'csv', 'json'],
                        default='table',
                        help='Output format (default: table)')
    parser.add_argument('--quiet', action='store_true',
                        help='Suppress header and extra output')

    args = parser.parse_args()

    # Parse client counts
    try:
        client_counts = [int(c.strip()) for c in args.clients.split(',')]
        if not client_counts or any(c <= 0 for c in client_counts):
            raise ValueError("Client counts must be positive integers")
    except ValueError as e:
        print(f"Error parsing --clients: {e}", file=sys.stderr)
        sys.exit(1)

    # Verify server is reachable
    url = f"http://{args.host}:{args.port}/RPC"
    try:
        proxy = xmlrpc.client.ServerProxy(url)
        proxy.echo("test")
    except Exception as e:
        print(f"Error: Cannot connect to server at {url}", file=sys.stderr)
        print(f"Details: {e}", file=sys.stderr)
        print("\nMake sure the benchmark server is running:", file=sys.stderr)
        print(f"  ./build/tests/benchmark-server --port {args.port} --numthreads 4",
              file=sys.stderr)
        sys.exit(1)

    # Define payloads
    # Realistic smallest production payload: struct with auth + nested array
    small_payload = {
        "Username": "user",
        "Password": "12345678",
        "Server": "SERVER",
        "Method": "Method_API",
        "Params": [87654321],
    }
    large_payload = "x" * (1024 * 1024)  # ~1MB

    # Determine request counts
    small_requests = args.requests
    # Large payload uses fewer requests by default to keep runtime reasonable
    large_requests = args.large_requests or min(1000, args.requests)

    if not args.quiet:
        print(f"\n=== RPS Benchmark Results ===")
        print(f"Server: {args.host}:{args.port}")
        print(f"Warmup: {args.warmup} requests per client count\n")

    all_results = {}

    if args.output == 'table':
        print_header()

        if args.scenario in ('small', 'both'):
            all_results['small_payload'] = run_scenario(
                name='small_payload',
                host=args.host,
                port=args.port,
                payload=small_payload,
                num_requests=small_requests,
                client_counts=client_counts,
                warmup=args.warmup
            )

        if args.scenario in ('large', 'both'):
            all_results['large_payload'] = run_scenario(
                name='large_payload',
                host=args.host,
                port=args.port,
                payload=large_payload,
                num_requests=large_requests,
                client_counts=client_counts,
                warmup=args.warmup
            )

        print()

    elif args.output == 'csv':
        print("scenario,clients,requests,rps,p50_ms,p90_ms,p95_ms,p99_ms,min_ms,max_ms,errors")

        if args.scenario in ('small', 'both'):
            for num_clients in client_counts:
                result = run_benchmark(
                    host=args.host, port=args.port, method='echo',
                    payload=small_payload, num_requests=small_requests,
                    num_clients=num_clients, warmup_requests=args.warmup
                )
                print(f"small_payload,{num_clients},{result.total_requests},"
                      f"{result.rps:.1f},{result.p50_ms:.3f},{result.p90_ms:.3f},"
                      f"{result.p95_ms:.3f},{result.p99_ms:.3f},{result.min_ms:.3f},"
                      f"{result.max_ms:.3f},{result.errors}")

        if args.scenario in ('large', 'both'):
            for num_clients in client_counts:
                result = run_benchmark(
                    host=args.host, port=args.port, method='echo',
                    payload=large_payload, num_requests=large_requests,
                    num_clients=num_clients, warmup_requests=args.warmup
                )
                print(f"large_payload,{num_clients},{result.total_requests},"
                      f"{result.rps:.1f},{result.p50_ms:.3f},{result.p90_ms:.3f},"
                      f"{result.p95_ms:.3f},{result.p99_ms:.3f},{result.min_ms:.3f},"
                      f"{result.max_ms:.3f},{result.errors}")

    elif args.output == 'json':
        results_data = {'server': f"{args.host}:{args.port}", 'scenarios': {}}

        if args.scenario in ('small', 'both'):
            scenario_results = []
            for num_clients in client_counts:
                result = run_benchmark(
                    host=args.host, port=args.port, method='echo',
                    payload=small_payload, num_requests=small_requests,
                    num_clients=num_clients, warmup_requests=args.warmup
                )
                scenario_results.append({
                    'clients': num_clients,
                    'requests': result.total_requests,
                    'rps': round(result.rps, 1),
                    'latency_ms': {
                        'p50': round(result.p50_ms, 3),
                        'p90': round(result.p90_ms, 3),
                        'p95': round(result.p95_ms, 3),
                        'p99': round(result.p99_ms, 3),
                        'min': round(result.min_ms, 3),
                        'max': round(result.max_ms, 3)
                    },
                    'errors': result.errors
                })
            results_data['scenarios']['small_payload'] = scenario_results

        if args.scenario in ('large', 'both'):
            scenario_results = []
            for num_clients in client_counts:
                result = run_benchmark(
                    host=args.host, port=args.port, method='echo',
                    payload=large_payload, num_requests=large_requests,
                    num_clients=num_clients, warmup_requests=args.warmup
                )
                scenario_results.append({
                    'clients': num_clients,
                    'requests': result.total_requests,
                    'rps': round(result.rps, 1),
                    'latency_ms': {
                        'p50': round(result.p50_ms, 3),
                        'p90': round(result.p90_ms, 3),
                        'p95': round(result.p95_ms, 3),
                        'p99': round(result.p99_ms, 3),
                        'min': round(result.min_ms, 3),
                        'max': round(result.max_ms, 3)
                    },
                    'errors': result.errors
                })
            results_data['scenarios']['large_payload'] = scenario_results

        print(json.dumps(results_data, indent=2))

    return 0


if __name__ == '__main__':
    sys.exit(main())
