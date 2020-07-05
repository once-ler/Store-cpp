/*
g++ -g3 -std=c++14 -Wall \
-I ../../../../spdlog/include \
-I ../../../../tdspp \
-I ../../../../Store-cpp \
-I ../../../../connection-pool \
-I ../../../../json/single_include/nlohmann \
-o testing ../index-sql-notifier-spec.cpp \
-L /usr/lib64 \
-L /usr/lib/x86_64-linux-gnu \
-L/usr/local/lib -lpthread \
-lrdkafka \
-lcppkafka \
-luuid \
-ltdsclient -lsybdb \
-lpugixml \
-lpthread -lboost_system -lboost_filesystem
*/

#include <cppkafka/cppkafka.h>
#include <thread>
#include <chrono>
#include "pugixml.hpp"
#include "store.common/src/time.hpp"
#include "store.models/src/extensions.hpp"
#include "store.storage.kafka/src/kafka_base_client.hpp"
#include "store.models/src/ioc/service_provider.hpp"
#include "store.storage.sqlserver/src/mssql_dblib_base_client.hpp"
#include "store.storage.sqlserver/src/mssql_notifier.hpp"

using namespace std;
using namespace cppkafka;
using namespace store::extensions;
using namespace store::storage::kafka;

using MsSqlDbLibBaseClient = store::storage::mssql::MsSqlDbLibBaseClient;
using Notifier = store::storage::mssql::Notifier;
  
void createSqlTable(shared_ptr<Notifier> notifier) {
  string sql = R"__(
    if not exists(select 1 from sys.objects where object_id = object_id('dbo._Resource') and type in (N'u') )
    create table dbo._Resource(oid binary(16), id varchar(255), dateModified float, type varchar(255));
    if not exists (select 1 from sys.indexes where object_id = object_id(N'dbo._Resource') and name = N'idx_clustered_resource')
    create unique clustered index idx_clustered_resource on dbo._Resource (id);
    if not exists(select 1 from dbo._Resource where id = '1')
    insert dbo._Resource(oid, id, dateModified, type) values (0xB430C86DA710E4449D4C05F5FE84C683,'1',1000.0*datediff(ss,convert(datetime,'1970-01-01',121),current_timestamp),'FooType');
  )__";
  notifier->executeNonQuery(sql);
}

shared_ptr<MsSqlDbLibBaseClient> getDbLibClient() {
  DBContext db_ctx("testing", "localhost", 1433, "test", "admin", "12345678", 30);  
  return make_shared<MsSqlDbLibBaseClient>(db_ctx, "test:pool");
}

shared_ptr<Notifier> getSqlNotifier(shared_ptr<MsSqlDbLibBaseClient> client) {
  string databaseName = "test", schemaName = "dbo", tableName = "_Resource";
  return make_shared<Notifier>(client, databaseName, schemaName, tableName);
}

shared_ptr<KafkaBaseClient> getKafkaBaseClient() {
  vector<ConfigurationOption> options = {
    { "metadata.broker.list", "127.0.0.1:9092" }, { "client.id", "sql-notifier" },
    { "group.id", "test" }, { "enable.auto.commit", false }
  };
  auto configPtr = make_shared<Configuration>(options);
  return make_shared<KafkaBaseClient>(configPtr);
}

/*
struct xml_string_writer: pugi::xml_writer {
  std::string result;

  virtual void write(const void* data, size_t size) {
    result.append(static_cast<const char*>(data), size);
  }
};

auto pugixmlToString = [](pugi::xml_document& doc) -> std::string {
    xml_string_writer writer;
      
    doc.print(writer);
    return writer.result;
  };

pugi::xml_node appendMetadata(pugi::xml_document& doc, const string& environment, const string& store, const string& type, const string& id) {
  auto metaNode = doc.child("meta");
  std::map<string, string> metaValues = { 
    {"environment", environment}, 
    {"store", store}, 
    {"type", type},
    {"id", id} 
  };
  
  for (const auto& e : metaValues)
    metaNode.attribute(e.first.c_str()).set_value(e.second.c_str());  
}
*/

auto main(int agrc, char* argv[]) -> int {
  auto sqlClient = getDbLibClient();
  auto sqlNotifier = getSqlNotifier(sqlClient);
  auto kafkaClient = getKafkaBaseClient();

  string environment = "development", store = "LOB_A";

  string topic = fmt::format("{}.{}.resource-modified", environment, store);

  // Create the sql table.
  createSqlTable(sqlNotifier);

  // Check topics.
  cout << "\nTopics:\n";
  auto topicMetadata = kafkaClient->getTopics();
  for (const TopicMetadata& topic : topicMetadata) {
    cout << "* " << topic.get_name() << ": " << topic.get_partitions().size()
      << " partitions" << endl;
  }

  // Start the kafka consumer.
  thread th([&](){
    kafkaClient->subscribe(topic, [](const Message& msg) {
      cout << "Message Received by Consumer 1\n";
      if (msg.is_eof())
        return;

      if (msg.get_key()) {
        cout << msg.get_key() << " -> ";
      }
      ostringstream oss;
      oss << msg.get_payload();
      auto data = oss.str();
      cout << data  << endl;
      oss.clear();
      oss << msg.get_offset();
      cout << "Offset: " << oss.str() << endl;
      // Parse the xml.
      pugi::xml_document doc;
      auto res = doc.load_string(data.c_str());
      if (res) {
        cout << res.description() << endl;
        // pugi::xml_node root = doc.document_element();
        auto row = doc.select_node("/root/inserted/row").node();
        cout << row.name() << endl;
        cout << row.child("oid").child_value() << endl
          << row.child("id").child_value() << endl
          << row.child("type").child_value() << endl;
      }
    });
  });

  // Start the sql listener.
  sqlNotifier->start([&](string msg) {
    cout << "Sql Notifier received [+]\n\n" << msg << endl;
    
    if (msg.size() == 0)
      return;

    pugi::xml_document doc;
    auto res = doc.load_string(msg.c_str());
    
    if (!res)
      return;

    /*
    auto row = doc.select_node("/root/inserted/row").node();
    auto oid = row.child("oid").child_value();
    auto type = row.child("type").child_value();
    auto id = row.child("id").child_value();
    */

    auto oid = doc.select_node("/root/inserted/row/oid").node().child_value();
    cout << oid << endl;

    auto hx = base64_to_hex(oid);
    cout << hx << endl;

    // appendMetadata(doc, environment, store, type, id);
    // auto nextMsg = pugixmlToString(doc);

    kafkaClient->produce(topic, hx, msg);    
  });

  // 
  size_t cnt = 5;
  do {
    this_thread::sleep_for(chrono::milliseconds(2000));
    // Send an update to trigger a message.
    auto now = store::common::getCurrentTimeMilliseconds();
    sqlNotifier->executeNonQuery(fmt::format("update dbo._resource set [dateModified] = {} where id = '1'", now));
    cnt -= 1;
  } while (cnt > 0);

  th.join();
}
