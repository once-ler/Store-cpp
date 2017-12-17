#pragma once

// #define BSONCXX_STATIC
// #define MONGOCXX_STATIC

#include <iostream>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/create_collection.hpp>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <windows.h>
#endif

using namespace std;
using namespace bsoncxx::builder::stream;

//Call init() once
mongocxx::instance inst{};

namespace store {
  namespace storage {
    namespace mongodb {
      using bsoncxx::builder::stream::open_array;
      using bsoncxx::builder::stream::close_array;

      class BaseClient {
      public:
        BaseClient() = default;
        explicit BaseClient(
          const string& url,
          const string& database,
          const string& collection
        ) : url_(url), database_(database), collection_(collection) {}

        int insert(
          const string& type,
          const string& jsonString,
          const string& collectionName = ""
        ) {          
          auto doc = prepareDocument(type, jsonString);
          
          try {
            mongocxx::client client{ mongocxx::uri{ url_ } };
            auto result = client[database_][collectionName.size() == 0 ? collection_ : collectionName].insert_one(doc.view());
            return 1;
          } catch (const exception& e) {
            cout << e.what() << endl;
            return 0;
          }
        }

        int upsert(
          const string& type,
          const string& jsonString,
          const string& _id,
          const string& collectionName = ""
        ) {
          auto doc = prepareDocument(type, jsonString, _id);
          
          bsoncxx::builder::stream::document filter_;
          filter_ << "_id" << _id;

          return upsertImpl<document>(doc, filter_, collectionName);
        }

        int upsert(const string& jsonString, const string& _id, const string& collectionName = "") {
          auto doc = bsoncxx::from_json(jsonString);
          
          bsoncxx::builder::stream::document filter_;
          // filter_ << "_id" << doc.view()["_id"].get_value();
          filter_ << "_id" << _id;

          return upsertImpl<bsoncxx::v_noabi::document::value>(doc, filter_, collectionName);
        }

      private:
        string url_;
        string database_;
        string collection_;

        template<typename T>
        int upsertImpl(const T& doc, const document& filter_, const string& collectionName) {
          try {
            mongocxx::client client{ mongocxx::uri{ url_ } };
            auto result = client[database_][collectionName.size() == 0 ? collection_ : collectionName].replace_one(filter_.view(), doc.view(), mongocxx::options::update());
            return 1;
          } catch (const exception& e) {
            cout << e.what() << endl;
            return 0;
          }
        }

        document prepareDocument(
          const string& type,
          const string& jsonString,
          const string& _id = ""
        ) {
          document b;
          if (_id.size() > 0) b << "_id" << _id;

          b << "type" << type
            << "start_time" << bsoncxx::types::b_date(std::chrono::system_clock::now())
            << "data" << bsoncxx::types::b_document{ bsoncxx::from_json(jsonString) };
          return b;
        }

      };
    }
  }
}
