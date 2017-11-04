#pragma once

#include <iostream>
#include <map>
#include <functional>
#include <memory>
#include <stddef.h>  // defines NULL
#include <assert.h>

namespace store {
  namespace common {
    template <class T>
    class Singleton {
    public:
      static T* Instance() {
        if (!_instance) _instance = new T;
        assert(_instance != NULL);  // abort?
        return _instance;
      }
    protected:
      Singleton() {};
      ~Singleton() {};
    private:
      Singleton(Singleton const&);
      Singleton& operator=(Singleton const&) {};
      static T* _instance;
    };

    template <class T> T* Singleton<T>::_instance = NULL;
  }
}
