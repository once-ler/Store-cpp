#pragma once

#include <iostream>
#include <vector>
#include <functional>
#include <map>
#include <boost/any.hpp>

using namespace std;

namespace store {
  namespace ioc {
    struct IDisposable {};
    struct ISimpleContainer : IDisposable {
      virtual boost::any Get(boost::any serviceType) {};
      virtual boost::any GetStore(string typeOfStore) {};
      boost::any GetType(string typeName) {};
      boost::any Get(string typeName) {};
      virtual vector<string> GetRegistrationsByNameKeys() {};
      virtual void Register(string typeName, boost::any instance) {};      
    };

    /// <summary>
    /// 
    /// </summary>
    class SimpleContainer : ISimpleContainer {
      // private readonly Dictionary<Type, Func<object>> _registrations = new Dictionary<Type, Func<object>>();
      // private readonly Dictionary<string, Func<Type>> _registrationsByName = new Dictionary<string, Func<Type>>();
      // private readonly Dictionary<string, Func<object>> _registrationsInstanceByName = new Dictionary<string, Func<object>>();

      map<boost::any, function<boost::any()>> _registrations;
      template<typename T> map<string, function<T()>> _registrationsByName;
      map<string, function<boost::any()>> _registrationsInstanceByName;
      

      template<typename T> T Get() {};
      template<typename TService> void Register() {};
      // template<typename TService> void Register(string typeName, TService type) {};
      // template<typename TService> void Register(function<TService()> instanceCreator) {};
      // template<typename TService, typename TImpl = TService> void Register() {};
      template<typename TService> void Singleton(TService instance) {};
      template<typename TService> void Singleton(Func<TService> instanceCreator) {};

    public:
      /// <summary>
      /// 
      /// </summary>
      /// <typeparam name="TService"></typeparam>
      template<typename TService>
      void Register() {
        Register<TService, TService>();
      }

      /// <summary>
      /// 
      /// </summary>
      /// <param name="typeName"></param>
      /// <param name="type"></param>
      template<typename TService>
      void Register(string typeName, TService type) {
        // function<type()> f1;
        // _registrationsByName.TryGetValue(typeName, out f1);
        auto f1 = _registrationsByName.find(typeName);
        if (f1 == _registrationsByName.end()) _registrationsByName[typeName] = [&type]() { return type; };
      }

      /// <summary>
      /// 
      /// </summary>
      /// <param name="typeName"></param>
      /// <param name="instance"></param>
      void Register(string typeName, boost::any instance) override {
        // Func<object> f1;
        // _registrationsInstanceByName.TryGetValue(typeName, out f1);
        auto f1 = _registrationsInstanceByName.find(typeName);
        if (f1 == _registrationsInstanceByName.end()) _registrationsInstanceByName[typeName] = [&instance]() { return instance; };
      }

      /// <summary>
      /// 
      /// </summary>
      /// <typeparam name="TService"></typeparam>
      /// <typeparam name="TImpl"></typeparam>
      template<typename TService, typename TImpl = TService>
      void Register() {
        // _registrations.Add(typeof(TService), () = > {
        // var implType = typeof(TImpl);
        // return typeof(TService) == implType ? CreateInstance(implType) : Get(implType);
        _registrations[TService] = []() {
          // TODO
          return 0;
        };
      }

      /// <summary>
      /// 
      /// </summary>
      /// <typeparam name="TService"></typeparam>
      /// <param name="instanceCreator"></param>
      template<typename TService>
      void Register(function<boost::any()> instanceCreator) {
        // _registrations.Add(typeof(TService), () = > instanceCreator());
        _registrations[TService] = instanceCreator();
      }

      /// <summary>
      /// 
      /// </summary>
      /// <typeparam name="TService"></typeparam>
      /// <param name="instance"></param>
      public void Singleton<TService>(TService instance) {
        _registrations.Add(typeof(TService), () = > instance);
      }

      /// <summary>
      /// 
      /// </summary>
      /// <typeparam name="TService"></typeparam>
      /// <param name="instanceCreator"></param>
      public void Singleton<TService>(Func<TService> instanceCreator) {
        var lazy = new Lazy<TService>(instanceCreator);
        Register(() = > lazy.Value);
      }

      /// <summary>
      /// 
      /// </summary>
      /// <typeparam name="T"></typeparam>
      /// <returns></returns>
      public T Get<T>() {
        return (T)Get(typeof(T));
      }

      /// <summary>
      /// 
      /// </summary>
      /// <param name="serviceType"></param>
      /// <returns></returns>
      public object Get(Type serviceType) {
        Func<object> creator;
        if (_registrations.TryGetValue(serviceType, out creator)) {
          return creator();
        }

        if (!serviceType.GetTypeInfo().IsAbstract) {
          return CreateInstance(serviceType);
        }

        throw new InvalidOperationException("No registration for " + serviceType);
      }

      /// <summary>
      /// 
      /// </summary>
      /// <param name="typeOfStore"></param>
      /// <returns></returns>
      public dynamic GetStore(string typeOfStore) {
        IList<Type> typesToRegister = new List<Type>();

        var a = _registrations.Where(d = > {
          var ty = d.Key;
          var innerType = ty.IsGenericType;
          if (innerType == true) {
            var b = ty.GetGenericArguments().FirstOrDefault();
            if (b != null && typeOfStore == b.Name) {
              typesToRegister.Add(b);
              return true;
            }
          }
          return false;
        }).ToList();

        // Register the type internally
        foreach(var b in typesToRegister) {
          Func<object> f;
          _registrations.TryGetValue(b, out f);
          if (f == null) _registrations.Add(b, () = > b);
          // Register by type name
          Register(b.Name, b);
        }

        if (a.Count() > 0) return a.FirstOrDefault().Key;

        return null;
      }

      /// <summary>
      /// 
      /// </summary>
      /// <param name="typeName"></param>
      /// <returns></returns>
      public Type GetType(string typeName) {
        Func<Type> f;
        _registrationsByName.TryGetValue(typeName, out f);
        if (f != null) {
          return f();
        }
        return null;
      }

      /// <summary>
      /// 
      /// </summary>
      /// <param name="typeName"></param>
      /// <returns></returns>
      public dynamic Get(string typeName) {
        Func<object> f1;
        _registrationsInstanceByName.TryGetValue(typeName, out f1);
        return f1;
      }

      /// <summary>
      /// Get a list of all type names registered by string as its key.
      /// </summary>
      /// <returns>IEnumerable[string]</returns>
      public IEnumerable<string> GetRegistrationsByNameKeys() {
        return _registrationsByName.Select(d = > d.Key);
      }

      /// <summary>
      /// 
      /// </summary>
      public void Dispose() {
        _registrations.Clear();
      }

      /// <summary>
      /// 
      /// </summary>
      /// <param name="implementationType"></param>
      /// <returns></returns>
      private object CreateInstance(Type implementationType) {
        var ctor = implementationType.GetTypeInfo().GetConstructors().Single();
        var parameterTypes = ctor.GetParameters().Select(p = > p.ParameterType);
        var dependencies = parameterTypes.Select(Get).ToArray();
        return Activator.CreateInstance(implementationType, dependencies);
      }
    }
  }
}
