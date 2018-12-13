/*
g++ -std=c++14 -Wall -O0 -g3 -I ../../../ -I ../../../../amy/include -I /usr/local/include/boost -I /usr/local/include ../000-basic-select.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/lib -L /usr/local/lib -lmysqlclient -lpthread -lboost_system
*/

#include "store.storage.mysql/src/my_client.hpp"

using MyClient = store::storage::mysql::MyBaseClient;

void simple_get_async(MyClient& client, string stub) {
  client.connectAsync("SELECT '" + stub + "' stub, " +
    R"(
      character_set_name, maxlen
      FROM information_schema.character_sets
      WHERE character_set_name LIKE 'latin%';
    )",
    [](AMY_SYSTEM_NS::error_code const& ec, amy::result_set& rs) {
      const auto& fields_info = rs.fields_info();

      std::cout
        << "Affected rows: " << rs.affected_rows()
        << ", field count: " << rs.field_count()
        << ", result set size: " << rs.size()
        << std::endl;

      for (const auto& row : rs) {
        std::cout
          << fields_info[0].name() << ": " << row[0].as<std::string>() << ", "
          << fields_info[1].name() << ": " << row[1].as<amy::sql_bigint>()
          << std::endl;
      }
    }

  );
}

auto main(int argc, char *argv[]) -> int {
  DBContext ctx{"127.0.0.1", 3306, "test", "admin", "12345678"};

  thread a([] {
    auto client = MyClient("127.0.0.1", 3306, "test", "admin", "12345678");
    simple_get_async(client, "1");
  });

  thread b([&ctx] {
    auto client = MyClient(ctx);
    simple_get_async(client, "2");
  });

  thread c([&ctx] {
    auto client = MyClient(ctx);
    simple_get_async(client, "3");
  });

  thread t([]{while(true) std::this_thread::sleep_for(std::chrono::milliseconds(500));});
  t.join();

  return 0;
}
