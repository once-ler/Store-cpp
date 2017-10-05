#pragma once

#include <iostream>
#include <memory>
#include "json.hpp"
#include "include/connection.hpp"
#include "include/utility.hpp"
#include "base_client.hpp"

using namespace std;
using namespace postgrespp;
using namespace store::interfaces;
using namespace store::storage;

using json = nlohmann::json;

namespace store {
  namespace storage {
    namespace pgsql {

      template<typename T>
      class Client : public BaseClient<T> {
      private:
        shared_ptr<Connection> connection;
      public:
        using BaseClient<T>::BaseClient;

        Client(DBContext _dbContext) : BaseClient<T>(_dbContext) {
          this->connection = Connection::create("host=127.0.0.1 port=5432 dbname=pccrms connect_timeout=10");
          
        }
        
        template<typename U>
        U save(U doc) {
          // auto f = this->List([](U doc) {

          // });

          // return f(doc);
          return doc;
        }

      };

    }
  }
}
