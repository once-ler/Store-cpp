// #include "json.hpp"
#include "mssql_client.hpp"
#include "models.hpp"
// #include "extensions.hpp"

using namespace store::models;
// using namespace store::extensions;
using json = nlohmann::json;

int spec_0() {
  DBContext db_ctx("testing", "localhost", 1433, "dwh$staging", "admin", "12345678", 30);
  
  using MsSqlClient = store::storage::mssql::MsSqlClient<IEvent>;
  MsSqlClient client(db_ctx);

  client.insertOne("epic", "participant_hist", {"firstname", "lastname", "processed"}, "foo", "bar", 0);

  return 0;
}

int main(int argc, char *argv[]) {
  return spec_0();
}

