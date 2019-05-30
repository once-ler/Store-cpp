#pragma once

#include <string.h>
#include <list>
#include <vector>

#include "cassandra.h"
#include "store.common/src/logger.hpp"

using namespace std;
using namespace store::common;

namespace store::storage::cassandra {

  typedef struct Credentials_ {
    const char* password;
    const char* username;
  } Credentials;

  class CassandraBaseClient {
  public:
    const string version = "0.1.0";
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

    CassError executeQuery(CassSession* session, const char* query) {
      CassError rc = CASS_OK;
      CassFuture* future = NULL;
      CassStatement* statement = cass_statement_new(query, 0);

      future = cass_session_execute(session, statement);
      cass_future_wait(future);

      rc = cass_future_error_code(future);
      if (rc != CASS_OK) {
        print_error(future);
      }

      cass_future_free(future);
      cass_statement_free(statement);

      return rc;
    }

    void insertAsync(CassSession* session, vector<CassStatement*> statements) {
      for(auto statement : statements) {
        futures.push_back(cass_session_execute(session, statement));
        if (futures.size() > insertThreshold) {
          for (auto future : futures) {
            cass_future_wait(future);
            cass_future_free(future);
          }
          futures.clear();
        }
        cass_statement_free(statement);
      }
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

    CassCluster* create_cluster(const char* hosts) {
      CassCluster* cluster = cass_cluster_new();
      cass_cluster_set_contact_points(cluster, hosts);
      return cluster;
    }

    void print_error(CassFuture* future) {
      const char* message;
      size_t message_length;
      cass_future_error_message(future, &message, &message_length);
      fprintf(stderr, "Error: %.*s\n", (int)message_length, message);
    }

    CassFuture* connect_future = NULL;
    CassCluster* cluster = NULL;
    // CassCluster* cluster = cass_cluster_new();
    CassSession* session = cass_session_new();
    
    /* Setup authentication callbacks and credentials */
    CassAuthenticatorCallbacks auth_callbacks = {
      on_auth_initial,
      on_auth_challenge,
      on_auth_success,
      on_auth_cleanup
    };

    std::list<CassFuture*> futures;
    int insertThreshold = 5000;
  
  private:
    string hosts;
    string username;
    string password;

  };

}
