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
  auto group_id = "test", group_id2 = "test2";
  string topic = "test_topic_2";

  // https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md
  vector<ConfigurationOption> options = {
    { "metadata.broker.list", brokers },
    { "group.id", group_id },
    // Disable auto commit
    { "enable.auto.commit", false }
  }, options2 = {
    { "metadata.broker.list", brokers },
    { "group.id", group_id2 },
    { "enable.auto.commit", false }
  };

  auto configPtr = make_shared<Configuration>(options);
  auto configPtr2 = make_shared<Configuration>(options2);

  KafkaBaseClient client(configPtr);
  KafkaBaseClient client2(configPtr2);
  
  cout << "\nBrokers:\n";
  auto brokerMetadata = client.getBrokers();
  for (const BrokerMetadata& broker : brokerMetadata) {
    cout << broker.get_id() << " " << broker.get_host() << ":" << broker.get_port() << endl;
  }

  cout << "\nTopics:\n";
  auto topicMetadata = client.getTopics();
  for (const TopicMetadata& topic : topicMetadata) {
    cout << "* " << topic.get_name() << ": " << topic.get_partitions().size()
      << " partitions" << endl;
  }

  size_t cnt = 0;
  // Produce some messages periodically.
  /*
    Keys are mostly useful/necessary if you require strong order for a key 
    and are developing something like a state machine. 
    If you require that messages with the same key (for instance, a unique id) 
    are always seen in the correct order, attaching a key to messages 
    will ensure messages with the same key always go to the same partition in a topic. 
    Kafka guarantees order within a partition, but not across partitions in a topic, 
    so alternatively not providing a key - which will result 
    in round-robin distribution across partitions - will not maintain such order.
  */
  thread th([&](){
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      ostringstream oss;
      auto now = chrono::system_clock::to_time_t(chrono::system_clock::now()); 
      oss << ctime(&now) << "Message #" << cnt << endl; 
      client.produce(topic, "KEY1234", oss.str());
    }
  });

  thread th1([&](){
    client.subscribe(topic, [](const Message& msg) {
      cout << "Message Received by Consumer 1\n";
      if (msg.is_eof())
        return;

      if (msg.get_key()) {
        cout << msg.get_key() << " -> ";
      }
      cout << msg.get_payload() << endl;
    });
  });

  thread th2([&](){
    client2.subscribe(topic, [](const Message& msg) {
      cout << "Message Received by Consumer 2\n";
      if (msg.is_eof())
        return;

      if (msg.get_key()) {
        cout << msg.get_key() << " -> ";
      }
      cout << msg.get_payload() << endl;
    });
  });

  th.join();

  return 0;
}
