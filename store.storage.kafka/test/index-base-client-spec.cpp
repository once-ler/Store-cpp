/*
g++ -g3 -std=c++14 -Wall -I ../../../../Store-cpp \
-o testing ../index-base-client-spec.cpp \
-L /usr/lib64 \
-L /usr/lib/x86_64-linux-gnu \
-L/usr/local/lib -lpthread \
-lrdkafka \
-lcppkafka \
-lpthread
*/

#include <cppkafka/cppkafka.h>
#include <thread>
#include <chrono>
#include "store.storage.kafka/src/kafka_base_client.hpp"

using namespace std;
using namespace cppkafka;
using namespace store::storage::kafka;

auto main(int agrc, char* argv[]) -> int {
  auto brokers = "127.0.0.1:9092";
  /*
    Consumers can join a group by using the samegroup.id.
    The maximum parallelism of a group is that the number of consumers in the group ← no of partitions.
    Kafka assigns the partitions of a topic to the consumer in a group, so that each partition is consumed by exactly one consumer in the group.
    Kafka guarantees that a message is only ever read by a single consumer in the group.
    Consumers can see the message in the order they were stored in the log.
  */
  auto group_id = "test";
  string topic = "test_topic";

  vector<ConfigurationOption> options = {
    { "metadata.broker.list", brokers },
    { "group.id", group_id },
    // Disable auto commit
    { "enable.auto.commit", false }
  };

  auto configPtr = make_shared<Configuration>(options);
  KafkaBaseClient client(configPtr);
  // KafkaBaseClient producer(configPtr);

  size_t cnt = 0;
  // Produce some messages periodically.
  thread th([&](){
    std::this_thread::sleep_for(std::chrono::seconds(2));
    ostringstream oss;
    auto now = chrono::system_clock::to_time_t(chrono::system_clock::now()); 
    oss << ctime(&now) << "Message #" << cnt << endl; 
    client.produce(topic, "KEY1234", oss.str());
  });

  client.subscribe(topic, [](const Message& msg) {
    cout << "Message Received\n";
    if (msg.is_eof())
      return;

    if (msg.get_key()) {
      cout << msg.get_key() << " -> ";
    }
    cout << msg.get_payload() << endl;
  });

  return 0;
}
