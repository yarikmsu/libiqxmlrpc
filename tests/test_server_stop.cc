#include <iostream>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_server.h"
#include "libiqxmlrpc/executor.h"

using namespace boost::unit_test_framework;
using namespace iqxmlrpc;

class TestServer {
public:
  TestServer(const TestServer&) = delete;
  TestServer& operator=(const TestServer&) = delete;

  TestServer(int port, unsigned threads):
    exec_factory_(threads > 1 ?
      static_cast<Executor_factory_base*>(new Pool_executor_factory(threads)) :
      static_cast<Executor_factory_base*>(new Serial_executor_factory)),
    serv_(new Http_server(port, exec_factory_.get())),
    thread_(new std::thread([this]() { this->serv_->work(); }))
  {
  }

  void stop()
  {
    serv_->set_exit_flag();
  }

  void join()
  {
    if (thread_->joinable()) {
      thread_->join();
    }
  }

private:
  std::unique_ptr<iqxmlrpc::Executor_factory_base> exec_factory_;
  std::unique_ptr<iqxmlrpc::Server> serv_;
  std::unique_ptr<std::thread> thread_;
};

void stop_and_join(unsigned threads, std::condition_variable* on_stop, std::mutex* mtx)
{
  TestServer s(3344, threads);

  std::this_thread::sleep_for(std::chrono::seconds(1));

  s.stop();
  s.join();

  {
    std::lock_guard<std::mutex> lk(*mtx);
  }
  on_stop->notify_all();
}

void stop_test_server(unsigned server_threads)
{
  std::condition_variable stopped;
  std::mutex c_mutex;

  std::thread thr([server_threads, &stopped, &c_mutex]() {
    stop_and_join(server_threads, &stopped, &c_mutex);
  });

  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::unique_lock<std::mutex> lck(c_mutex);
  BOOST_CHECK( stopped.wait_for(lck, std::chrono::seconds(3)) == std::cv_status::no_timeout );

  if (thr.joinable()) {
    thr.join();
  }
}

void stop_test_server_st()
{
  stop_test_server(1);
}

void stop_test_server_mt(unsigned fnum)
{
  BOOST_CHECKPOINT(fnum);
  stop_test_server(16);
}

bool init_tests()
{
  test_suite& test = framework::master_test_suite();
  test.add( BOOST_TEST_CASE(&stop_test_server_st) );

  for (unsigned i = 0; i < 50; ++i)
    test.add( BOOST_TEST_CASE(std::bind(stop_test_server_mt, i)));

  return true;
}

int main(int argc, char* argv[])
{
  boost::unit_test::unit_test_main( &init_tests, argc, argv );
}

// vim:ts=2:sw=2:et
