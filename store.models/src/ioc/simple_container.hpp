#pragma once

#include <iostream>
#include <memory>
#include <functional>
#include <map>

using namespace std;

namespace store {
  namespace ioc {
    /*
    https://www.codeproject.com/Articles/567981/AnplusIOCplusContainerplususingplusVariadicplusTem
    */
    class IOCContainer {
    private:
      class IHolder {
      public:
        virtual ~IHolder() {}
        virtual void noop() {}
      };

      template<class T>
      class Holder : public IHolder {
      public:
        virtual ~Holder() {}
        std::shared_ptr<T> instance_;
      };

      std::map<std::string, std::function<void*()>> creatorMap_;
      std::map<std::string, std::shared_ptr<IHolder>> instanceMap_;

    public:
      template <class T, typename... Ts>
      void RegisterSingletonClass() {
        std::shared_ptr<Holder<T>> holder(new Holder<T>());
        holder->instance_ = std::shared_ptr<T>(new T(GetInstance<Ts>()...));

        instanceMap_[typeid(T).name()] = holder;
      }

      template <class T>
      void RegisterInstance(std::shared_ptr<T> instance) {
        std::shared_ptr<Holder<T>> holder(new Holder<T>());
        holder->instance_ = instance;

        instanceMap_[typeid(T).name()] = holder;
      }

      template <class T, typename... Ts>
      void register_class() {
        auto createType = [this]() -> T * {
          return new T(GetInstance<Ts>()...);
        };

        creatorMap_[typeid(T).name()] = createType;
      }

      template <class T>
      std::shared_ptr<T> GetInstance() {
        if (instanceMap_.find(typeid(T).name()) != instanceMap_.end()) {
          std::shared_ptr<IHolder> iholder = instanceMap_[typeid(T).name()];

          Holder<T> * holder = dynamic_cast<Holder<T>*>(iholder.get());
          return holder->instance_;
        } else {
          return std::shared_ptr<T>(static_cast<T*>
            (creatorMap_[typeid(T).name()]()));
        }
      }

    };

    class SimpleContainer : public IOCContainer {

    };
  }
}