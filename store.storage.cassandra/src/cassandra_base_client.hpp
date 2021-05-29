#pragma once

#include <string.h>
#include <list>
#include <vector>
#include <functional>
#include <memory>

#include "cassandra.h"
#include "store.common/src/logger.hpp"
#include "store.common/src/runtime_get_tuple.hpp"

using namespace std;
using namespace store::common;
using namespace store::common::tuples;

namespace store::storage::cassandra {

  using CassandraFutureTapFunc = std::function<void(CassFuture*)>;

  typedef struct Credentials_ {
    const char* username;
    const char* password;
  } Credentials;

  template<typename A>
  void BindCassParameter(CassStatement* statement, size_t position, A value){}

  void BindCassParameter(CassStatement* statement, size_t position, const char* value) {
    #ifdef DEBUG
    cout << "position: " << to_string(position) << endl;
    cout << "value: " << value << endl;
    #endif
    cass_statement_bind_string(statement, position, value);
  }

  void BindCassParameter(CassStatement* statement, size_t position, string value) {
    #ifdef DEBUG
    cout << "position: " << to_string(position) << endl;
    cout << "value: " << value << endl;
    #endif
    cass_statement_bind_string(statement, position, value.c_str());
  }

  void BindCassParameter(CassStatement* statement, size_t position, bool value) {
    #ifdef DEBUG
    cout << "position: " << to_string(position) << endl;
    cout << "value: " << value << endl;
    #endif
    cass_statement_bind_bool(statement, position, value ? cass_true : cass_false);
  }

  void BindCassParameter(CassStatement* statement, size_t position, float value) {
    #ifdef DEBUG
    cout << "position: " << to_string(position) << endl;
    cout << "value: " << to_string(value) << endl;
    #endif
    cass_statement_bind_float(statement, position, value);
  }

  void BindCassParameter(CassStatement* statement, size_t position, double value) {
    #ifdef DEBUG
    cout << "position: " << to_string(position) << endl;
    cout << "value: " << to_string(value) << endl;
    #endif
    cass_statement_bind_double(statement, position, value);
  }

  void BindCassParameter(CassStatement* statement, size_t position, int32_t value) {
    #ifdef DEBUG
    cout << "position: " << to_string(position) << endl;
    cout << "value: " << to_string(value) << endl;
    #endif
    cass_statement_bind_int32(statement, position, (cass_int32_t)value);
  }

  void BindCassParameter(CassStatement* statement, size_t position, int64_t value) {
    #ifdef DEBUG
    cout << "position: " << to_string(position) << endl;
    cout << "value: " << to_string(value) << endl;
    #endif
    cass_statement_bind_int64(statement, position, (cass_int64_t)value);
  }

  void BindCassParameter(CassStatement* statement, size_t position, CassUuid value) {
    cass_statement_bind_uuid(statement, position, (CassUuid)value);
  }

  struct TupleFormatCassParamFunc {
    TupleFormatCassParamFunc(CassStatement* statement_, size_t pos_) : statement(statement_), pos(pos_) {};
    
    template<class U>
    void operator()(U p) {
      BindCassParameter(statement, pos, p);
    }

    CassStatement* statement;
    size_t pos;
  };

  class CassandraBaseClient {
  public:
    const string version = "0.1.1";
    CassandraBaseClient() {}

    ~CassandraBaseClient() {
      for (auto future : futures) {
        cass_future_wait(future);
        cass_future_free(future);
      }
      futures.clear();
      
      if (cluster != NULL)
        cass_cluster_free(cluster);
      
      if (session != NULL)
        cass_session_free(session);

      if (uuid_gen != NULL)
        cass_uuid_gen_free(uuid_gen);  
    }

    explicit CassandraBaseClient(
      const string& hosts_,
      const string& username_ = "",
      const string& password_ = ""
    ) : hosts(hosts_), username(username_), password(password_) {}

    void tryConnect() {

      cluster = create_cluster(hosts.c_str());

      /* Set custom authentication callbacks and credentials */
      if (username.size() > 0) {
        Credentials credentials = {
          username.c_str(),
          password.c_str()
        };

        cass_cluster_set_authenticator_callbacks(
          cluster, &auth_callbacks,
          NULL,
          &credentials);      
      }
      
      /* Provide the cluster object as configuration to connect the session */
      connect_future = cass_session_connect(session, cluster);

      if (cass_future_error_code(connect_future) == CASS_OK) {
        printf("Successfully connected!\n");
      } else {
        /* Handle error */
        const char* message;
        size_t message_length;
        cass_future_error_message(connect_future, &message, &message_length);
        fprintf(stderr, "Unable to connect: '%.*s'\n", (int)message_length,
                                                            message);
      }

      cass_future_free(connect_future);
    }

    CassError executeQuery(const char* query, shared_ptr<CassandraFutureTapFunc> tapFunc = nullptr) {
      CassError rc = CASS_OK;
      CassFuture* future = NULL;
      CassStatement* statement = cass_statement_new(query, 0);

      future = cass_session_execute(session, statement);
      
      cass_future_wait(future);

      rc = cass_future_error_code(future);
      if (rc != CASS_OK) {
        print_error(future);
      }

      if (rc == CASS_OK && tapFunc) {
        (*tapFunc)(future);
      }

      cass_future_free(future);
      cass_statement_free(statement);

      return rc;
    }

    void executeQueryAsync(const char* query, CassFutureCallback callback) {
      CassStatement* statement = cass_statement_new(query, 0);
      CassFuture* future = cass_session_execute(session, statement);
      cass_future_set_callback(future, callback, session);
      cass_future_free(future);
      cass_statement_free(statement);
    }

    void insertSync(CassStatement* statement) {
      auto f = cass_session_execute(session, statement);
      // Waiting on future in callback will cause a deadlock.
      // Do not invoke insertSync on a callback, use insertAsync instead.
      cass_future_wait(f);
      size_t rc = cass_future_error_code(f);
      
      if (rc != CASS_OK) {
        print_error(f);
      } else {
        const CassResult* result = cass_future_get_result(f);
        size_t sz = cass_result_row_count(result);
        cout << "Insert count: " << sz << endl;
        cass_result_free(result);
      }
      cass_future_free(f);
      cass_statement_free(statement);
    }

    void insertAsync(CassStatement* statement) {
      auto insert_future = cass_session_execute(session, statement);
      cass_future_set_callback(insert_future, on_insert, session);
      cass_future_free(insert_future);
      cass_statement_free(statement);
    }

    void insertAsync(vector<CassStatement*> statements) {
      #ifdef DEBUG
      cout << "Executing batch for " << statements.size() << " statements.\n";
      #endif
      CassBatch* batch = cass_batch_new(CASS_BATCH_TYPE_UNLOGGED);

      // Set timeout for insert to 30 sec.
      cass_batch_set_request_timeout(batch, 30000);

      for(auto statement : statements) {
        cass_batch_add_statement(batch, statement);
        cass_statement_free(statement);
      }

      auto batch_future = cass_session_execute_batch(session, batch);
      cass_future_set_callback(batch_future, on_insert, session);
      cass_future_free(batch_future);
    }

    /*
      @usage:
      client.getInsertStatement("participant_hist", {"firstname", "lastname", "processed"}, "foo", "bar", 0);
    */
    template<typename... Params>
    CassStatement* getInsertStatement(const string& table_name, const std::initializer_list<std::string>& fields, Params... params) {
      tuple<Params...> values(params...);
      stringstream ss;

      ss << "insert into " << table_name << " (";

      auto it = fields.begin();
      while (it != fields.end() - 1) {
        ss << *it << ",";
        ++it;
      }          
      ss << *it;          
      ss << ") values \n(";
      
      size_t i = 0;
      while (i < fields.size() - 1) {
        ss << "?,";
        ++i;
      }          
      ss << "?);";

      #ifdef DEBUG
      fprintf(stdout, "%s\n", ss.str().c_str());
      #endif

      CassStatement* statement = cass_statement_new(ss.str().c_str(), fields.size());

      i = 0;
      while (i < fields.size()) {
        tuples::apply(values, i, TupleFormatCassParamFunc{statement, i});
        ++i;
      }

      return statement;
    }

    // Pass by value, trivial size.
    void getUUID(CassUuid& uuid) {
      cass_uuid_gen_time(uuid_gen, &uuid);
    }

    void getUUIDFromTimestamp(cass_uint64_t timestamp, CassUuid& uuid) {
      cass_uuid_gen_from_time(uuid_gen, timestamp, &uuid);
    }

  protected:
    static void on_auth_initial(CassAuthenticator* auth, void* data) {  
      const Credentials* credentials = (const Credentials *)data;

      size_t username_size = strlen(credentials->username);
      size_t password_size = strlen(credentials->password);
      size_t size = username_size + password_size + 2;

      char* response = cass_authenticator_response(auth, size);

      /* Credentials are prefixed with '\0' */
      response[0] = '\0';
      memcpy(response + 1, credentials->username, username_size);

      response[username_size + 1] = '\0';
      memcpy(response + username_size + 2, credentials->password, password_size);
    }

    static void on_auth_challenge(CassAuthenticator* auth, void* data, const char* token, size_t token_size) {}

    static void on_auth_success(CassAuthenticator* auth, void* data, const char* token, size_t token_size ) {}

    static void on_auth_cleanup(CassAuthenticator* auth, void* data) {}

    static void print_error(CassFuture* future) {
      const char* message;
      size_t message_length;
      cass_future_error_message(future, &message, &message_length);
      fprintf(stderr, "Error: %.*s\n", (int)message_length, message);
    }

    static void on_insert(CassFuture* future, void* data) {
      CassError code = cass_future_error_code(future);
      if (code != CASS_OK) {
        print_error(future);
      }
    }

    CassCluster* create_cluster(const char* hosts) {
      CassCluster* cluster = cass_cluster_new();
      cass_cluster_set_contact_points(cluster, hosts);
      return cluster;
    }

    CassFuture* connect_future = NULL;
    CassCluster* cluster = NULL;
    // CassCluster* cluster = cass_cluster_new();
    CassSession* session = cass_session_new();
    
    // Per application, a uuid generator, threadsafe.
    CassUuidGen* uuid_gen = cass_uuid_gen_new();
    
    /* Setup authentication callbacks and credentials */
    CassAuthenticatorCallbacks auth_callbacks = {
      on_auth_initial,
      on_auth_challenge,
      on_auth_success,
      on_auth_cleanup
    };

    std::list<CassFuture*> futures; // This will be deprecated.
  
  private:
    string hosts;
    string username;
    string password;

  };

}
