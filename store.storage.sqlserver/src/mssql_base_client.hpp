#pragma once

#include <iostream>
#include <sstream>
#include <tuple>
#include <typeinfo>
#include <ctpublic.h>
#include "tdspp.hh"
#include "store.common/src/runtime_get_tuple.hpp"

using namespace store::common;

namespace store::storage::mssql {
  
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

  class MsSqlBaseClient {
    friend TDSPP;
  public:
    const string version = "0.1.10";
    MsSqlBaseClient(const string& server_, int port_, const string& database_, const string& user_, const string& password_) :
      server(server_), port(port_), database(database_), user(user_), password(password_) {
      db = make_shared<TDSPP>();
      stringstream ss;
      ss << server << ":" << port;
      server_port = ss.str();
    }

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

      try {
        db->connect(server_port, user, password);
        db->execute(string("use " + database));
        db->execute(ss.str());
      } catch (TDSPP::Exception& e) {
        cerr << e.message << endl;
        return 0;
      }
      return 1;
    }

  protected:
    shared_ptr<TDSPP> db;

  private:
    string server;
    int port;
    string database;
    string user;
    string password;
    string server_port;
  };

}
