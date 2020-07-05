#pragma once

#include <csignal>
#include <iostream>
#include <sstream>
// #include <librdkafka/rdkafkacpp.h>
#include <cppkafka/cppkafka.h>
#include "store.common/src/logger.hpp"

using namespace std;
using namespace cppkafka;

namespace store::storage::kafka {
  function<void()> on_signal;
  using OnMessageReceivedFunc = function<void(const Message&)>;

  OnMessageReceivedFunc defaultMessageHandler = [](const Message& msg) {
    if (msg.is_eof())
      return;

    if (msg.get_key()) {
      cout << msg.get_key() << " -> ";
    }
    cout << msg.get_payload() << endl;
  };

  class KafkaBaseClient {
  public:
    const string version = "0.1.0";
    KafkaBaseClient(shared_ptr<cppkafka::Configuration> config_, shared_ptr<store::common::ILogger> logger_ = nullptr): config(config_), logger(logger_) {
      if (config) {
        producer = make_shared<cppkafka::Producer>(*config);
        consumer = make_shared<cppkafka::Consumer>(*config);
        dispatcher = make_shared<cppkafka::ConsumerDispatcher>(*consumer);
      }
      if (!logger) {
        logger = make_shared<store::common::ILogger>();
      }
    }

    void createTopic(const string& topic) {
      // When consumer subscribes to topic, topic <SHOULD> be created if it doesn't exist.
      auto handle = producer->get_handle();
      char errstr[512];

      rd_kafka_topic_conf_t* kafka_topic_conf = rd_kafka_topic_conf_new();
      // rd_kafka_topic_conf_set(kafka_topic_conf, "", "", errstr, sizeof(errstr));
      rd_kafka_topic_t* rkt = rd_kafka_topic_new(handle, topic.c_str(), kafka_topic_conf);      
    }

    void produce(const string& topic, const string& key, const string& message, int partitionId = 0) {
      producer->produce(MessageBuilder(topic).partition(partitionId).key(key).payload(message));
      producer->flush();
    }

    vector<cppkafka::BrokerMetadata> getBrokers() {
      cppkafka::Metadata metadata = producer->get_metadata();
      return metadata.get_brokers();
    }

    vector<cppkafka::TopicMetadata> getTopics() {
      cppkafka::Metadata metadata = producer->get_metadata();
      return metadata.get_topics();
    }

    void subscribe(const string& topic, OnMessageReceivedFunc callback = defaultMessageHandler) {
      consumer->subscribe({ topic });
      
      consumer->set_assignment_callback([this](const TopicPartitionList& partitions) {
        ostringstream oss;
        oss << "Got assigned: " << partitions << endl;
        logger->info(oss.str().c_str());
      });

      consumer->set_revocation_callback([this](const TopicPartitionList& partitions) {
        ostringstream oss;
        oss << "Got revoked: " << partitions << endl;
        logger->info(oss.str().c_str());
      });
      
      createDispatcher(callback);
    }

  private:
    shared_ptr<cppkafka::Configuration> config = nullptr;
    shared_ptr<cppkafka::Producer> producer = nullptr;
    shared_ptr<cppkafka::Consumer> consumer = nullptr;
    shared_ptr<cppkafka::ConsumerDispatcher> dispatcher = nullptr;
    shared_ptr<store::common::ILogger> logger = nullptr;
    
    void ensureTopicExists(const string& topicName) {
      auto it = getTopics();
      auto found = std::find_if(it.begin(), it.end(), [&topicName](const TopicMetadata& topic) {
        return topic.get_name() == topicName;
      });
      
      if (found == it.end()) {
        cout << "Creating topic " << topicName << endl; 
        createTopic(topicName);
      }
    }

    static void signal_handler(int) {
      on_signal();
    }
    
    void createDispatcher(OnMessageReceivedFunc func) {
      // Stop processing on SIGINT
      on_signal = [this]() {
        cout << "Dispatcher exiting.\n";
        dispatcher->stop();
      };
      
      signal(SIGINT, signal_handler);

      dispatcher->run(
        // Callback executed whenever a new message is consumed
        [this, &func](Message msg) {
          func(msg);

          consumer->commit(msg);
        },
        // Whenever there's an error (other than the EOF soft error)
        [this](Error error) {
          ostringstream oss;
          oss << "Received error: " << error << endl;
          logger->error(oss.str().c_str());
        },
        // Whenever EOF is reached on a partition, print this
        [this](ConsumerDispatcher::EndOfFile, const TopicPartition& topic_partition) {
          ostringstream oss;
          oss << "Reached EOF on partition " << topic_partition << endl;
          logger->error(oss.str().c_str());
        }
      );
    }

  };

}
