#pragma once

#include <tuple>
#include <typeinfo>
#include <ctpublic.h>
#include "tdspp.hh"
#include "json.hpp"
#include "base_client.hpp"
#include "runtime_get_tuple.hpp"

using namespace store::common;

namespace store::storage::mssql {
  
  template<typename T>
  class MsSqlClient : public BaseClient<T> {
    friend TDSPP;
    public:
    const string version = "0.1.9";
    MsSqlClient(DBContext _dbContext) : BaseClient<T>(_dbContext) {
      db = make_shared<TDSPP>();
    }

    struct TupleFormatFunc {
      TupleFormatFunc(stringstream* ss_) : ss(ss_) {};

      template<class U>
      void operator()(U p) {
        auto ty = string(typeid(p).name()).back();
        if (ty == 'c' || ty == 's') {
          *ss << "'" << p << "'";
        } else {
          *ss << p;
        }
        // std::cout << __PRETTY_FUNCTION__ << " : " << p << "\n";
      }

      stringstream* ss;
    };

    /*
      @usage:
      client.insertOne("epic", "participant_hist", {"firstname", "lastname", "processed"}, "foo", "bar", 0);
    */
    template<typename... Params>
    int64_t insertOne(const string& schema, const string& table_name, const std::initializer_list<std::string>& fields, Params... params) {
      tuple<Params...> values(params...);
      stringstream ss;

      ss << "insert into " << schema << "." << table_name << " (";

      auto it = fields.begin();
      while (it != fields.end() - 1) {
        ss << *it << ",";
        ++it;
      }          
      ss << *it;          
      ss << ") values \n(";
      
      int i = 0;
      while (i < fields.size() - 1) {
        tuples::apply(values, i, TupleFormatFunc{&ss});
        ss << ",";
        ++i;
      }          
      tuples::apply(values, fields.size() - 1, TupleFormatFunc{&ss});
      ss << ");";
    }
    private:
    shared_ptr<TDSPP> db;
  };
}