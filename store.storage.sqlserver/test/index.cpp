#include "store.storage.sqlserver/src/mssql_client.hpp"
#include "store.models/src/models.hpp"

using namespace store::models;
using namespace store::storage::mssql;
using json = nlohmann::json;

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
  return spec_0();
}

