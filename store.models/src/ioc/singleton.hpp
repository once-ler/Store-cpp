#pragma once

#include <iostream>
#include <map>
#include <functional>
#include <memory>
#include <stddef.h>  // defines NULL

namespace store {
  namespace common {
    template <class T>
    class Singleton {
    public:
      static T* Instance() {
        if (!m_pInstance) m_pInstance = new T;
        assert(m_pInstance != NULL);
        return m_pInstance;
      }
    protected:
      Singleton() {};
      ~Singleton() {};
    private:
      Singleton(Singleton const&);
      Singleton& operator=(Singleton const&) {};
      static T* m_pInstance;
    };

    template <class T> T* Singleton<T>::m_pInstance = NULL;
  }
}

namespace store {
  namespace ioc0 {
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
      void RegisterClass() {
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

  }
}


namespace store {
  namespace ioc1 {
    // https://www.codeproject.com/Articles/1029836/A-miniature-IOC-Container-in-Cplusplus
    class FactoryRoot {
    public:
      virtual ~FactoryRoot() {}
    };

    //todo: consider sorted vector
    std::map<int, std::shared_ptr<FactoryRoot>> m_factories;

    template<typename T>
    class CFactory : public FactoryRoot {
      std::function<std::shared_ptr<T>()> m_functor;
    public:
      ~CFactory() {}

      CFactory(std::function<std::shared_ptr<T>()> functor)
        :m_functor(functor) {
      }

      std::shared_ptr<T> GetObject() {
        return m_functor();
      }
    };

    class IOCContainer {
      static int s_nextTypeId;
    public:
      //one typeid per type
      template<typename T>
      static int GetTypeID() {
        static int typeId = s_nextTypeId++;
        return typeId;
      }

      template<typename T>
      std::shared_ptr<T> GetObject() {
        auto typeId = GetTypeID<T>();
        auto factoryBase = m_factories[typeId];
        auto factory = std::static_pointer_cast<CFactory<T>>(factoryBase);
        return factory->GetObject();
      }

      //Most basic implementation - register a functor
      template<typename TInterface, typename ...TS>
      void RegisterFunctor(std::function<std::shared_ptr<TInterface>(std::shared_ptr<TS> ...ts)> functor) {
        m_factories[GetTypeID<TInterface>()] = std::make_shared<CFactory<TInterface>>([=] {return functor(GetObject<TS>()...); });
      }

      //Register one instance of an object
      template<typename TInterface>
      void RegisterInstance(std::shared_ptr<TInterface> t) {
        m_factories[GetTypeID<TInterface>()] = std::make_shared<CFactory<TInterface>>([=] {return t; });
      }

      //Supply a function pointer
      template<typename TInterface, typename ...TS>
      void RegisterFunctor(std::shared_ptr<TInterface>(*functor)(std::shared_ptr<TS> ...ts)) {
        RegisterFunctor(std::function<std::shared_ptr<TInterface>(std::shared_ptr<TS> ...ts)>(functor));
      }

      //A factory that will call the constructor, per instance required
      template<typename TInterface, typename TConcrete, typename ...TArguments>
      void RegisterFactory() {
        RegisterFunctor(
          std::function<std::shared_ptr<TInterface>(std::shared_ptr<TArguments> ...ts)>(
            [](std::shared_ptr<TArguments>...arguments) -> std::shared_ptr<TInterface>
        {
          return std::make_shared<TConcrete>(std::forward<std::shared_ptr<TArguments>>(arguments)...);
        }));
      }

      //A factory that will return one instance for every request
      template<typename TInterface, typename TConcrete, typename ...TArguments>
      void RegisterInstance() {
        RegisterInstance<TInterface>(std::make_shared<TConcrete>(GetObject<TArguments>()...));
      }
    };

  }
}
