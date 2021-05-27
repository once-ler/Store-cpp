#pragma once

#include "store.storage.cassandra/src/ca_resource_common.hpp"
#include "store.storage.cassandra/src/cassandra_base_client.hpp"

using namespace std;

namespace store::storage::cassandra {

  class ca_resource_modified {
  public:
    ca_resource_modified() = default;
    ca_resource_modified(
      const string& _environment,
      const string& _store,
      const string& _type,
      cass_int64_t _start_time,
      const string& _id,
      const string& _oid,
      const string& _uid,
      const string& _current
    ) : environment(_environment), store(_store), type(_type), start_time(_start_time), id(_id), oid(_oid), uid(_uid), current(_current) {}
    ~ca_resource_modified() = default;
    string environment;
    string store;
    string type;
    cass_int64_t start_time;
    string id;
    string oid;
    string uid;
    string current;

    ca_resource_modified* rowToType(const CassRow* row) {
      this->getString(row)
        ->getInt64(row)
        ->getUuid(row);

      return this;
    }

  private:
    ca_resource_modified* getString(const CassRow* row) {
      vector<string> k{"environment", "store", "type", "id", "oid", "current"};
      std::map<string, string> m = getCellAsString(row, k);
      environment = m[k.at(0)];
      store = m[k.at(1)];
      type = m[k.at(2)];
      id = m[k.at(3)];
      oid = m[k.at(4)];
      current = m[k.at(5)];
      return this;
    }

    ca_resource_modified* getInt64(const CassRow* row) {
      vector<string> k{"start_time"};
      std::map<string, cass_int64_t> m = getCellAsInt64(row, k);
      start_time = m[k.at(0)];
      return this;
    }

    ca_resource_modified* getUuid(const CassRow* row) {
      vector<string> k{"uid"};
      std::map<string, string> m = getCellAsUuid(row, k);
      uid = m[k.at(0)];
      return this;
    }
  };

}
