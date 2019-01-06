#include "store.storage.sqlserver/src/mssql_client.hpp"
#include "store.models/src/models.hpp"
#include "store.common/src/spdlogger.hpp"

using namespace store::models;
using namespace store::storage::mssql;
using json = nlohmann::json;

auto spdLogger = make_shared<SpdLogger>("mssql-test", "log", true);

void sqlToJson(Field* fd, json& j) {
  auto cname = fd->colname;
  int datatype = fd->getDataType();

  switch (datatype) {
    case CS_REAL_TYPE: case CS_FLOAT_TYPE: case CS_MONEY_TYPE: case CS_MONEY4_TYPE: case CS_NUMERIC_TYPE: case CS_DECIMAL_TYPE: 
    {
      j[cname] = fd->to_double();
    }
    case CS_INT_TYPE: case CS_UINT_TYPE:
    {
      j[cname] = fd->to_int();
    }
    case CS_BIGINT_TYPE: case CS_UBIGINT_TYPE: case CS_BIGDATETIME_TYPE: case CS_BIGTIME_TYPE:
    {
      j[cname] = fd->to_int64();
    } 
    default:
      j[cname] = fd->to_str();
  }
}

struct Wrapper {
  using MsSqlClient = store::storage::mssql::MsSqlClient<IEvent>;
  shared_ptr<MsSqlClient> ptr;
};

int spec_3() {
  DBContext db_ctx("testing", "localhost", 1433, "master", "admin", "12345678", 30);
  using MsSqlClient = store::storage::mssql::MsSqlClient<IEvent>;
  
  Wrapper wrapper;
  wrapper.ptr = make_shared<MsSqlClient>(db_ctx);
  wrapper.ptr = nullptr;

  wrapper.ptr = make_shared<MsSqlClient>(db_ctx);
  wrapper.ptr = nullptr;
  
  return 0;
}

int spec_2() {
  DBContext db_ctx("testing", "localhost", 1433, "master", "admin", "12345678", 30);
  
  using MsSqlClient = store::storage::mssql::MsSqlClient<IEvent>;
  MsSqlClient client(db_ctx);
  auto sqlClient = make_shared<MsSqlClient>(client);
  sqlClient->logger = spdLogger;

  string fakesql = "select convert(bigint, 999) rowid, 'PROJECT_STATUS' prefix_type, 'FOO' type, '1234' id, 'ALIVE' status";    

  auto a = sqlClient->runQueryJson(fakesql);  

  cout << sqlClient->logger->get_errors() << endl;

  auto b = sqlClient->runQueryJson(fakesql);  

  cout << sqlClient->logger->get_errors() << endl;

  return 0;
}

int spec_1() {
  DBContext db_ctx("testing", "localhost", 1433, "master", "admin", "12345678", 30);
  
  using MsSqlClient = store::storage::mssql::MsSqlClient<IEvent>;
  MsSqlClient client(db_ctx);
  auto sqlClient = make_shared<MsSqlClient>(client);

  string fakesql = "select convert(bigint, 999) rowid, 'PROJECT_STATUS' prefix_type, 'FOO' type, '1234' id, 'ALIVE' status";    

  auto q = sqlClient->runQuery(fakesql);  

  if (q != nullptr) {
    while (!q->eof()) {
      json j(nullptr);

      cout << "| ";
      for (int i=0; i < q->fieldcount; i++) {
        auto fd = q->fields(i);
        auto v2 = fd->to_str();
        cout << v2 << " | ";
        cout << fd->getDataType() << endl;

        sqlToJson(fd, j);
      }

      cout << j.dump(2) << endl;

      q->next();
    }

  } else {
    cout << "Failure" << endl;
  }
}

int spec_0() {
  DBContext db_ctx("testing", "localhost", 1433, "master", "admin", "12345678", 30);
  
  using MsSqlClient = store::storage::mssql::MsSqlClient<IEvent>;
  MsSqlClient client(db_ctx);

  {
    auto q = client.runQuery("select current_timestamp");

    if (q != nullptr) {
      while (!q->eof()) {
        cout << "| ";
        for (int i=0; i < q->fieldcount; i++) {
          auto fd = q->fields(i);
          auto v2 = fd->to_str();
          cout << v2 << " | ";
        }
        q->next();
      }

    } else {
      cout << "Failure" << endl;
    }
  }

  auto r = client.insertOne("epic", "participant_hist", {"firstname", "lastname", "processed"}, "foo", "bar", 0);

  if (!r.first) {
    cout << r.first << ": " << r.second << endl;
  } else {
    cout << "Success" << endl;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  spec_2();

  return 0;
}

