#pragma once

#include "store.storage.sqlserver/src/mssql_base_client.hpp"
#include "store.storage/src/base_client.hpp"
#include "json.hpp"

using namespace store::common;
using namespace store::storage::mssql;

using json = nlohmann::json;

namespace store::storage::mssql {
  
  template<typename T>
  class MsSqlClient : public BaseClient<T>, public MsSqlBaseClient {
  public:
    const string version = "0.1.11";
    MsSqlClient(DBContext _dbContext) :
      BaseClient<T>(_dbContext),
      MsSqlBaseClient(_dbContext.server, _dbContext.port, _dbContext.database, _dbContext.user, _dbContext.password) {}
  
    vector<shared_ptr<json>> runQueryJson(const string& sqlStmt) override {
      vector<shared_ptr<json>> a;

      auto q = runQuery(sqlStmt);

      if (q != nullptr) {
        while (!q->eof()) {
          auto j = make_shared<json>(nullptr);

          for (int i=0; i < q->fieldcount; i++) {
            auto fd = q->fields(i);
            rowToJson(fd, *j);
          }

          a.push_back(move(j));

          q->next();
        }
      }

      return a;
    }

  private:
    // Reference for ct sql types:
    // https://github.com/FreeTDS/freetds/blob/master/include/cspublic.h#L546
    void rowToJson(Field* fd, json& j) {
      auto cname = fd->colname;
      int datatype = fd->getDataType();

      switch (datatype) {
        case CS_REAL_TYPE: case CS_FLOAT_TYPE: case CS_MONEY_TYPE: case CS_MONEY4_TYPE: case CS_NUMERIC_TYPE: case CS_DECIMAL_TYPE: 
        {
          j[cname] = fd->to_double();
          break;
        }
        case CS_INT_TYPE: case CS_UINT_TYPE:
        {
          j[cname] = fd->to_int();
          break;
        }
        case CS_BIGINT_TYPE: case CS_UBIGINT_TYPE: 
        {
          j[cname] = fd->to_int64();
          break;
        }
        case CS_DATETIME_TYPE: case CS_DATETIME4_TYPE:
        {
          struct tm tm;
          strptime(fd->to_str().c_str(), "%B %d %Y %I:%M:%S %p",&tm);
          
          if (fd->to_str().find("PM") != string::npos)
            tm.tm_hour += 12;

          ostringstream ss;
          ss << std::put_time(&tm, "%Y-%m-%d %X");
          
          j[cname] = ss.str();
          break;
        }
        default:
          j[cname] = fd->to_str();
          break;
      }
    }

  };
}