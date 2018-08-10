#pragma once

#include <boost/filesystem.hpp>
#include "store.common/src/logger.hpp"
#include "store.common/src/spdlogger.hpp"
#include "store.storage.pgsql/src/pg_client.hpp"
#include "json.hpp"
#include "test_fixtures.hpp"

using namespace std;
using namespace store::storage::pgsql;
using namespace test::fixtures;
using namespace store::common; 

using json = nlohmann::json;

namespace test::logger {

  void test_splog_spec() {
    json dbConfig = {{"applicationName", "store_pq"}};
    cout << dbConfig.dump(2) << endl;
    DBContext dbContext{ 
      dbConfig.value("applicationName", "store_pq"), 
      dbConfig.value("server", "127.0.0.1"),
      dbConfig.value("port", 5432), 
      dbConfig.value("database", "eventstore"),
      dbConfig.value("user", "streamer"),
      dbConfig.value("password", "streamer"),
      10 
    };

    pgsql::Client<IEvent> pgClient{ dbContext };
    
    pgClient.events.setDbSchema("dwh");
    
    // Set the logger to spdlog
    auto spdLogger = make_shared<SpdLogger>("test_logger");
    pgClient.logger = spdLogger;

    // Error!
    Droid d{ "4", "c3po", "", "old" };
    pgClient.save(pgClient.events.getDbSchema(), d);

    // Set the logger to default
    auto defaultLogger = make_shared<ILogger>();
    pgClient.logger = defaultLogger;

    pgClient.save(pgClient.events.getDbSchema(), d);

  }
}
