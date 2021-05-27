#pragma once

#include <map>
#include "store.storage.cassandra/src/cassandra_base_client.hpp"

using namespace std;

namespace store::storage::cassandra {

  auto getCellAsString = [](const CassRow* row, const vector<string>& fields) -> std::map<string, string> {
    std::map<string, string> kv;

    for (const auto& k : fields) {
      const char* string_value;
      size_t string_value_length;
      cass_value_get_string(cass_row_get_column_by_name(row, k.c_str()), &string_value, &string_value_length);
      #ifdef DEBUG
      cout << "key: " << k << "|value: " << string_value << "|value length: " << to_string(string_value_length) << endl;
      #endif
      string v(string_value, string_value_length);
      kv[k] = v;
    }

    return kv;
  };

  auto getCellAsInt64 = [](const CassRow* row, const vector<string>& fields) -> std::map<string, cass_int64_t> {
    std::map<string, cass_int64_t> kv;

    for (const auto& k : fields) {
      cass_int64_t int64_value;
      cass_value_get_int64(cass_row_get_column_by_name(row, k.c_str()), &int64_value);
      #ifdef DEBUG
      cout << "key: " << k << "|value: " << to_string(int64_value) << endl;
      #endif
      kv[k] = int64_value;
    }

    return kv;
  };

  auto getCellAsUuid = [](const CassRow* row, const vector<string>& fields) -> std::map<string, string> {
    std::map<string, string> kv;
    char key_str[CASS_UUID_STRING_LENGTH];

    for (const auto& k : fields) {
      CassUuid key;
      cass_value_get_uuid(cass_row_get_column_by_name(row, k.c_str()), &key);
      cass_uuid_string(key, key_str);
      #ifdef DEBUG
      cout << "key: " << k << "|value: " << key_str << endl;
      #endif
      kv[k] = string(key_str);
    }

    return kv;
  };

}
