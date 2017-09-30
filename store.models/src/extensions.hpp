#pragma once

#include <iostream>
#include <type_traits>

namespace store {
  namespace extensions {
    /// <summary>
    /// 
    /// </summary>
    /// <param name="t">Type of TypeInfo : Type</param>
    /// <returns></returns>
    /*
      Probably need something like this:
      class B {};

      class A : public B
      {
      public:
        typedef B Base;
      };
    */
    template<typename Type>
    vector<Type> InheritsFrom(Type t) {

    }

    template<typename T, typename U>
    bool isDerivedFrom(U derived, T base) {
      return std::is_base_of<base, derived>::value;
    }
  }
}
