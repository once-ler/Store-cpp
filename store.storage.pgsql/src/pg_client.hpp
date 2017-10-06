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
        U save(string version, U doc) {
          auto f = this->Save([this, &doc](string version) {

            auto onQueryExecuted = [this, &doc](const boost::system::error_code& ec, Result result) {
              if (!ec) {
                while (result.next()) {
                  char* str;

                  str = result.get<char*>(0);
                  std::cout << "STR0: " << str << std::endl;
                  std::cout << result.getColumn<char*>(0) << std::endl;
                  str = result.get<char*>(1);
                  std::cout << "STR1: " << str << std::endl;
                  str = result.get<char*>(2);
                  std::cout << "STR2: " << str << std::endl;
                  str = result.get<char*>(3);
                  std::cout << "STR3: " << str << std::endl;
                }
              } else {
                std::cout << ec << std::endl;
              }

              // Connection::ioService().service().stop();
            };

            connection->queryParams("select * from v$12345678.droid", std::move(onQueryExecuted), Connection::ResultFormat::TEXT);

            Connection::ioService().thread().join();

            return doc;
          });

          return f(version);
        }

      };

    }
  }
}
