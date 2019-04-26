#pragma once

#include <bsoncxx/stdx/string_view.hpp>
#include <mongocxx/logger.hpp>

class MongoLogger : public mongocxx::logger {
public:
  MongoLogger() : mongocxx::logger() {}

  
};