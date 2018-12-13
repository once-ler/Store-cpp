/*
ubuntu: apt-get install libmysqlclient-dev
redhat: yum install mysql mysql-devel mysql-lib
*/
#define USE_BOOST_ASIO 1

#include "amy/connector.hpp"
#include "amy/placeholders.hpp"
#include "store.models/src/models.hpp"

using namespace std;
using namespace AMY_ASIO_NS::ip;
using namespace store::models;

namespace store::storage::mysql {
  
  using ResultHandlerFunc = function<void(
      AMY_SYSTEM_NS::error_code const&,
      amy::result_set&
    )>;

  class MyBaseClient {
    friend AMY_ASIO_NS::io_service;
  
  public:
    const string version = "0.1.0";
    MyBaseClient(const string& server_, int port_, const string& database_, const string& user_, const string& password_) :
      server(server_), port(port_), database(database_), user(user_), password(password_) {
      
      IO = make_shared<AMY_ASIO_NS::io_service>();
      transactor = make_shared<amy::connector>(*IO);
      url = tcp::endpoint(address::from_string(server), port);
      authInfo = make_shared<amy::auth_info>(user, password);
    }

    MyBaseClient(DBContext& ctx) :
      server(ctx.server), port(ctx.port), database(ctx.database), user(ctx.user), password(ctx.password) {
      
      IO = make_shared<AMY_ASIO_NS::io_service>();
      transactor = make_shared<amy::connector>(*IO);
      url = tcp::endpoint(address::from_string(server), port);
      authInfo = make_shared<amy::auth_info>(user, password);
    }

    void check_error(AMY_SYSTEM_NS::error_code const& ec) {
      if (ec) {
        auto e = AMY_SYSTEM_NS::system_error(ec);
        std::cerr
          << "System error: "
          << e.code().value() << " - " << e.what()
          << std::endl;
      }
    }

    void report_system_error(AMY_SYSTEM_NS::system_error const& e) {
      std::cerr
          << "System error: "
          << e.code().value() << " - " << e.what()
          << std::endl;
    }

    void onResult(AMY_SYSTEM_NS::error_code const& ec, amy::result_set rs, amy::connector& transactor) {
      check_error(ec);

      userResultCallback(ec, rs);
    }

    void onQuery(AMY_SYSTEM_NS::error_code const& ec, amy::connector& transactor) {
      check_error(ec);

      transactor.async_store_result(
        std::bind(
          &MyBaseClient::onResult, this,
          amy::placeholders::error,
          amy::placeholders::result_set,
          std::ref(transactor)
        )
      );
    }

    void onConnect(AMY_SYSTEM_NS::error_code const& ec, amy::connector& transactor, string& stmt) {
      check_error(ec);

      transactor.async_query(
        stmt.c_str(),
        std::bind(
          &MyBaseClient::onQuery, this,
          amy::placeholders::error,
          std::ref(transactor)
        )
      );
    }

    void connectAsync(
      string stmt, 
      ResultHandlerFunc&& handleResult = [](AMY_SYSTEM_NS::error_code const& ec, amy::result_set& rs){}
    ) {

      userResultCallback = handleResult;

      auto callback = std::bind(
        &MyBaseClient::onConnect, this,
        amy::placeholders::error,
        std::ref(*transactor),
        stmt
      );

      transactor->async_connect(
        url,
        *authInfo,
        database,
        amy::default_flags,
        callback
      );

      try {
          IO->run();
      } catch (AMY_SYSTEM_NS::system_error const& e) {
        report_system_error(e);
      } catch (std::exception const& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
      }
    }

  private:
    string server = "127.0.0.1";
    int port = 3306;
    string database = "test";
    string user;
    string password;
    AMY_ASIO_NS::ip::tcp::endpoint url = tcp::endpoint(address_v4::loopback(), port);
    
    shared_ptr<amy::auth_info> authInfo;
    shared_ptr<AMY_ASIO_NS::io_service> IO;
    shared_ptr<amy::connector> transactor;
    ResultHandlerFunc userResultCallback;
  };

}
