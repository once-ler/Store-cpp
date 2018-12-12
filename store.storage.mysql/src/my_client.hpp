/*
ubuntu: apt-get install libmysqlclient-dev
redhat: yum install mysql mysql-devel mysql-lib
*/
#define USE_BOOST_ASIO 1
// #include "amy/asio.hpp"
// #include "amy/auth_info.hpp"
#include <functional>
#include <mutex>
#include "amy/connector.hpp"
#include "amy/placeholders.hpp"

using namespace std;
using namespace AMY_ASIO_NS::ip;

namespace store::storage::mysql {
  class Spinlock {
    std::atomic_flag lock_;
  public:
    Spinlock() {
      lock_.clear();
    }
 
    inline void lock() {
      while (lock_.test_and_set(std::memory_order_acquire));
    }
 
    inline void unlock() {
      lock_.clear(std::memory_order_release);
    }
 
    inline bool try_lock() {
      return !lock_.test_and_set(std::memory_order_acquire);
    }
  };

  Spinlock _lock;

  class MyBaseClient {
    friend AMY_ASIO_NS::io_service;
  
  public:
    const string version = "0.0.2";
    MyBaseClient(const string& server_, int port_, const string& database_, const string& user_, const string& password_) :
      server(server_), port(port_), database(database_), user(user_), password(password_) {
      stringstream ss;
      ss << server << ":" << port;
      // server_port = ss.str();

      IO = make_shared<AMY_ASIO_NS::io_service>();
      transactor = make_shared<amy::connector>(*IO);
      url = tcp::endpoint(address::from_string(server), port);
      authInfo = make_shared<amy::auth_info>(user, password);
    }

    void check_error(AMY_SYSTEM_NS::error_code const& ec) {
      if (ec) {
        throw AMY_SYSTEM_NS::system_error(ec);
      }
    }

    /*
      callback should be passed in by user.
    */
    void onResult(
      AMY_SYSTEM_NS::error_code const& ec,
      amy::result_set rs,
      amy::connector& transactor
    ) {
      cout << "Response acquired" << endl;
      check_error(ec);

      std::cout
        << "Affected rows: " << rs.affected_rows()
        << ", field count: " << rs.field_count()
        << ", result set size: " << rs.size()
        << std::endl;

      const auto& fields_info = rs.fields_info();

      for (const auto& row : rs) {
        std::cout
          << fields_info[0].name() << ": " << row[0].as<std::string>() << ", "
          << fields_info[1].name() << ": " << row[1].as<amy::sql_bigint>()
          << std::endl;
      }

      _lock.unlock();
    }

    void onQuery(AMY_SYSTEM_NS::error_code const& ec, amy::connector& transactor) {
      cout << "Query sent" << endl;
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
      cout << "Connected" << endl;
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

    void connectAsync(string stmt) {
      _lock.lock();

      if (transactor->is_open()) {
        onConnect(boost::system::error_code(), *transactor, stmt);
        return;
      }

      transactor->async_connect(
        url,
        *authInfo,
        database,
        amy::default_flags,
        std::bind(
          &MyBaseClient::onConnect, this,
          amy::placeholders::error,
          std::ref(*transactor),
          stmt
        )
      );

      try {
          IO->run();
      } catch (AMY_SYSTEM_NS::system_error const& e) {
        // report_system_error(e);
        std::cerr
          << "System error: "
          << e.code().value() << " - " << e.what()
          << std::endl;
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
    // string server_port;
    AMY_ASIO_NS::ip::tcp::endpoint url = tcp::endpoint(address_v4::loopback(), port);
    
    shared_ptr<amy::auth_info> authInfo;
    shared_ptr<AMY_ASIO_NS::io_service> IO;
    shared_ptr<amy::connector> transactor;
  };

}
