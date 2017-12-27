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

#ifndef MONGO_INIT_ONCE
#define MONGO_INIT_ONCE
//Call init() once
mongocxx::instance inst{};
#endif

namespace store::storage::mongo {
  using bsoncxx::builder::stream::close_array;
  using bsoncxx::builder::stream::close_document;
  using bsoncxx::builder::stream::document;
  using bsoncxx::builder::stream::finalize;
  using bsoncxx::builder::stream::open_array;
  using bsoncxx::builder::stream::open_document;

  class MongoBaseClient {
  public:
    const string version = "0.4.0";
    MongoBaseClient() = default;
    explicit MongoBaseClient(
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

    int insertOne(const bsoncxx::document::view_or_value& v, const string& collectionName = "") {
      try {
        mongocxx::client client{ mongocxx::uri{ url_ } };
        auto result = client[database_][collectionName.size() == 0 ? collection_ : collectionName].insert_one(v);
        return 1;
      } catch (const exception& e) {
        return 0;
      }
    }

    int insertOne(const string& jsonString, const string& collectionName = "") {
      auto doc = bsoncxx::from_json(jsonString);
      return insertOne(doc.view(), collectionName);      
    }

    int upsertOne(const bsoncxx::document::view_or_value& v, const string& _id, const string& collectionName = "") {
      auto filter_ = document{} << "_id" << _id << finalize;
      try {
        mongocxx::client client{ mongocxx::uri{ url_ } };
        auto result = client[database_][collectionName.size() == 0 ? collection_ : collectionName].replace_one(filter_.view(), v, mongocxx::options::update());
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

    int64_t getNextSequenceValue(const string& sequenceName, const string& collectionName = "") {
      auto filter_ = document{} << "_id" << sequenceName << finalize;

      auto update_ = document{}
          << "$inc" << open_document << "sequence_value" << (int64_t)1 << close_document
        << finalize;

      mongocxx::options::find_one_and_update options;
      options.upsert(true);
      options.return_document(mongocxx::options::return_document::k_after);
      options.bypass_document_validation(true);
      options.max_time(std::chrono::milliseconds(5000));

      try {
        mongocxx::client client{ mongocxx::uri{ url_ } };
        auto result = client[database_][collectionName.size() == 0 ? collection_ : collectionName].find_one_and_update(filter_.view(), update_.view(), options );
        return result->view()["sequence_value"].get_int64().value;
      } catch (const exception& e) {
        return -1;
      }
    }

    string getUid(const string& typeName, const string& uid, const string& collectionName = "") {
      string r_uid = "";
      auto filter_ = document{} << "type" << typeName << finalize;

      mongocxx::options::find findOptions;
      findOptions.limit(1);
      findOptions.projection(document{} << "_id" << 1 << finalize);

      try {
        mongocxx::client client{ mongocxx::uri{ url_ } };
        auto cursor = client[database_][collectionName.size() == 0 ? collection_ : collectionName].find(filter_.view(), findOptions );
        auto iter = (cursor.begin());

        if (iter != cursor.end()) {
          auto doc = *iter;
          r_uid = doc["_id"].get_utf8().value.to_string();
        } else {
          // Create the first instance.
          auto result = client[database_][collectionName.size() == 0 ? collection_ : collectionName].insert_one(document{} << "_id" << uid << "type" << typeName << finalize);
          r_uid = result->inserted_id().get_utf8().value.to_string();
        }
      } catch (const exception& e) {
        
      }
      return move(r_uid);
    }

  protected:
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
