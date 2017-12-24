#pragma once

#include "json.hpp"
#include "base_client.hpp"
#include "extensions.hpp"
#include "eventstore.hpp"
#include "mongo_base_client.hpp"

using namespace std;
using namespace store::interfaces;
using namespace store::storage;
using namespace store::extensions;

using json = nlohmann::json;

using MongoBaseClient = store::storage::mongo::BaseClient;

/*
pgClient.events.Append<Droid>(droidEvent);
pgClient.events.Append<Human>(humanEvent);

pgClient.events.Save();
*/

namespace store::storage::mongo {
  template<typename T>
  class Client : public BaseClient<T>, MongoBaseClient {      
  public:
    using BaseClient<T>::BaseClient;
    
    class MongoEventStore : public EventStore {
      public:
      explicit MongoEventStore(Client<T>* session_) : session(session_) {}

      int Save() override {

      }
      private:
      Client<T>* session;
    };

    // Client(DBContext _dbContext) : BaseClient<T>(_dbContext) {
      // connectionInfo = string_format("application_name=%s host=%s port=%d dbname=%s connect_timeout=%d user=%s password=%s", dbContext.applicationName.c_str(), dbContext.server.c_str(), dbContext.port, dbContext.database.c_str(), dbContext.connectTimeout, dbContext.user.c_str(), dbContext.password.c_str());
    // }
    
    explicit Client(const string& url, const string& database, const string& collection) : 
      MongoBaseClient(url, database, collection) {}

    MongoEventStore events{ this };

  };
}
