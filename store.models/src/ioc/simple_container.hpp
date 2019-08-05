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

      using KeyCreator = std::map<std::string, std::function<void*()>>;
      using KeyInstance = std::map<std::string, std::shared_ptr<IHolder>>;
      
      template<class T>
      class Holder : public IHolder {
      public:
        virtual ~Holder() {}
        std::shared_ptr<T> instance_;
      };

      std::map<std::string, std::function<void*()>> creatorMap_;
      std::map<std::string, std::shared_ptr<IHolder>> instanceMap_;

      std::map<std::string, KeyCreator> creatorMapWithKey_;
      std::map<std::string, KeyInstance> instanceMapWithKey_;
      
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

      /*
        Same type, multiple instances with different keys.
      */
      template <class T>
      void RegisterInstanceWithKey(std::string key, std::shared_ptr<T> instance) {
        std::shared_ptr<Holder<T>> holder(new Holder<T>());
        holder->instance_ = instance;

        instanceMapWithKey_[typeid(T).name()][key] = holder;
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
          if (creatorMap_.find(typeid(T).name()) != creatorMap_.end())
            return std::shared_ptr<T>(static_cast<T*>
              (creatorMap_[typeid(T).name()]()));

            return nullptr;
        }
      }

      template <class T>
      std::shared_ptr<T> GetInstanceWithKey(std::string key) {
        auto it = instanceMapWithKey_.find(typeid(T).name());
        if (it != instanceMapWithKey_.end()) {
          
          KeyInstance& inner = it->second;
          KeyInstance::iterator it2 = it->second.find(key);
          if (it2 != inner.end()) {
            std::shared_ptr<IHolder> iholder = inner[key];
            Holder<T> * holder = dynamic_cast<Holder<T>*>(iholder.get());
            return holder->instance_;
          } else {
            if (creatorMapWithKey_.find(typeid(T).name()) != creatorMapWithKey_.end())
              return std::shared_ptr<T>(static_cast<T*>
                (creatorMapWithKey_[typeid(T).name()][key]()));
            
            return nullptr;
          }          
        } else {
          if (creatorMapWithKey_.find(typeid(T).name()) != creatorMapWithKey_.end())
            return std::shared_ptr<T>(static_cast<T*>
              (creatorMapWithKey_[typeid(T).name()][key]()));

          return nullptr;
        }
      }

      template <class T>
      bool InstanceExist() {
        return instanceMap_.find(typeid(T).name()) != instanceMap_.end();
      }

      template <class T>
      bool InstanceWithKeyExist(std::string key) {
        auto it = instanceMapWithKey_.find(typeid(T).name());
        if (it != instanceMapWithKey_.end()) {
          KeyInstance& inner = it->second;
          KeyInstance::iterator it2 = it->second.find(key);
          return it2 != inner.end();
        }

        return false;
      }

    };

    class SimpleContainer : public IOCContainer {

    };
  }
}
