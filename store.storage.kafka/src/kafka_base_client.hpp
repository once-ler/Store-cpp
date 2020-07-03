#pragma once

#include <csignal>
#include <iostream>
#include <cppkafka/cppkafka.h>

using namespace std;
using namespace cppkafka;

namespace store::storage::kafka {
  function<void()> on_signal;
  using OnMessageReceivedFunc = function<void(const Message&)>;

  OnMessageReceivedFunc defaultMessageHandler = [](const Message& msg) {
    // Print the key (if any)
    if (msg.get_key()) {
      cout << msg.get_key() << " -> ";
    }
    // Print the payload
    cout << msg.get_payload() << endl;
  };

  class KafkaBaseClient {
  public:
    const string version = "0.1.0";
    KafkaBaseClient(shared_ptr<cppkafka::Configuration> config_): config(config_) {
      if (config) {
        producer = make_shared<cppkafka::Producer>(*config);
        consumer = make_shared<cppkafka::Consumer>(*config);
      }
    }

    void produce(const string& topic, const string& message, int partitionId = 0) {
      producer->produce(MessageBuilder(topic).partition(partitionId).payload(message));
      producer->flush();
    }

    void subscribe(const string& topic, OnMessageReceivedFunc callback = defaultMessageHandler) {
      consumer->subscribe({ topic });
      
      consumer->set_assignment_callback([](const TopicPartitionList& partitions) {
        cout << "Got assigned: " << partitions << endl;
      });

      consumer->set_revocation_callback([](const TopicPartitionList& partitions) {
        cout << "Got revoked: " << partitions << endl;
      });
      
      createDispatcher(callback);
    }

  private:
    shared_ptr<cppkafka::Configuration> config = nullptr;
    shared_ptr<cppkafka::Producer> producer = nullptr;
    shared_ptr<cppkafka::Consumer> consumer = nullptr;
    shared_ptr<cppkafka::ConsumerDispatcher> dispatcher = nullptr;
    
    static void signal_handler(int) {
      on_signal();
    }
    
    void createDispatcher(OnMessageReceivedFunc func) {
      // Stop processing on SIGINT
      on_signal = [this]() {
        dispatcher->stop();
      };
      
      signal(SIGINT, signal_handler);

      dispatcher->run(
        // Callback executed whenever a new message is consumed
        [this, &func](Message msg) {
          func(msg);

          // Now commit the message
          consumer->commit(msg);
        },
        // Whenever there's an error (other than the EOF soft error)
        [](Error error) {
          cout << "[+] Received error notification: " << error << endl;
        },
        // Whenever EOF is reached on a partition, print this
        [](ConsumerDispatcher::EndOfFile, const TopicPartition& topic_partition) {
          cout << "Reached EOF on partition " << topic_partition << endl;
        }
      );
    }

  };

}
