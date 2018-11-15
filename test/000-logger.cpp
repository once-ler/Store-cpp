/*
g++ -std=c++14 -I /usr/local/include -I ../../ -I ../../../spdlog/include ../000-logger.cpp -o bin/000-logger -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -lpthread -lboost_thread -lboost_regex -lboost_system -lboost_filesystem

*/

#include <iostream>
#include "store.common/src/logger.hpp"
#include "store.common/src/spdlogger.hpp"

#define DEBUG

using namespace std;
using namespace store::common;

int main() {
  
  auto spdLogger = make_shared<SpdLogger>("test_logger", true);

  spdLogger->debug("{} {} {} {}", "this", "is", "a", "test");

  spdLogger->info("{} {} {} {}", "this", "is", "a", "test");

  spdLogger->error("{} {} {} {}", "this", "is", "an", "error");
  spdLogger->error("This is also an error");
  spdLogger->error("{} {} {} {}", "this", "is", "another", "error");
  
  cout << spdLogger->get_errors() << endl;

  spdLogger->flush_errors();

  cout << "After flushing" << endl;
  cout << spdLogger->get_errors() << endl;

  spdLogger->flush();
}
