#pragma once

#include <iostream>
#include <functional>
#include "store.storage.cassandra/src/ca_resource_modified.hpp"

using namespace std;

namespace store::storage::cassandra {
  using HandleCaResourceModifiedFunc = std::function<shared_ptr<ca_resource_modified>(shared_ptr<ca_resource_modified> const &)>;

  class ca_resource_purpose_handler {
  public:
    string store;
    string dataType;
    string purpose;
    HandleCaResourceModifiedFunc handler;
  };
}
