#pragma once

#include <iostream>
#include <type_traits>
#include <typeinfo>
#include <cstdio>
#include <algorithm>
#include <boost/core/demangle.hpp>

using namespace std;
using namespace boost::core;

namespace store {
  namespace extensions {

    template<typename U>
    string resolve_type_to_string(bool lowercase = true) {
      const auto& ti = typeid(U);
      string n = ti.name(), d;
      n = demangle(n.c_str());
      auto l = n.substr(n.find_last_of(':') + 1);
      
      if (!lowercase) return l;

      d.resize(n.size());
      transform(l.begin(), l.end(), d.begin(), ::tolower);
      return d;
    }

    template<typename T, typename U>
    bool is_derived_from() {
      // U derived, T base
      return std::is_base_of<T, U>::value;
    }

    template<typename ... Args>
    std::string string_format(const std::string& format, Args ... args) {
      size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
      std::unique_ptr<char[]> buf(new char[size]);
      snprintf(buf.get(), size, format.c_str(), args ...);
      return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
    }

    bool invalid_char(char c) {  
      return !(c>=0 && c <128);
    }

    string strip_unicode(string str) {
      str.erase(remove_if(str.begin(),str.end(), invalid_char), str.end());
      return move(str);
    }

    string strip_soh(string str) {
      str.erase(remove_if(str.begin(), str.begin() + 1, [](char c) { return c == 1; }), str.begin() + 1);
      return move(str);
    }

  }
}
