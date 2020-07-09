#pragma once

#include <iostream>
#include <iomanip>
#include <sstream>
#include <type_traits>
#include <typeinfo>
#include <cstdio>
#include <algorithm>
#include <boost/core/demangle.hpp>
#include "json.hpp"
/*
linux:
Just clone https://github.com/sean-/ossp-uuid.git and do usual install.
Windows:
Clone https://github.com/htaox/ossp_uuid.git and build with VS2015.
*/
// #include <uuid.h>
#include "store.common/src/uuid.hxx"
#include "store.common/src/base64.hpp"

using namespace std;
using namespace boost::core;
using namespace store::common;

using json = nlohmann::json;

namespace store {
  namespace extensions {

    string lower_case(const string& l) {
      string d;
      d.resize(l.size());
      transform(l.begin(), l.end(), d.begin(), ::tolower);
      return move(d);
    }

    string upper_case(const string& l) {
      string d;
      d.resize(l.size());
      transform(l.begin(), l.end(), d.begin(), ::toupper);
      return move(d);
    }

    template<typename U>
    string resolve_type_to_string(bool lowercase = true) {
      const auto& ti = typeid(U);
      string n = ti.name(), d;
      n = demangle(n.c_str());
      auto l = n.substr(n.find_last_of(':') + 1);
      
      if (!lowercase) return l;

      d.resize(n.size());
      transform(l.begin(), l.end(), d.begin(), ::tolower);
      return move(d);
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

    // std::string s=join(array.begin(), array.end(), std::string(","));
    template <class T, class A>
    T join(const A &begin, const A &end, const T &t) {
      T result;
      for (A it = begin;
        it != end;
        it++) {
        if (!result.empty())
          result.append(t);
        result.append(*it);
      }
      return result;
    }

    inline string wrapString(string s, const string& ch = "'") {
      s.insert(0, ch);
      s.insert(s.size(), ch);
      return move(s);
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

    std::string& ltrim(std::string& str, const std::string& chars = "\t\n\v\f\r ") {
      str.erase(0, str.find_first_not_of(chars));
      return str;
    }
 
    std::string& rtrim(std::string& str, const std::string& chars = "\t\n\v\f\r ") {
      str.erase(str.find_last_not_of(chars) + 1);
      return str;
    }
 
    std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ") {
      return ltrim(rtrim(str, chars), chars);
    }

    // find_if(arr.begin(), arr.end(), insensitiveCompare("something")) != arr.end()
    auto insensitiveCompare = [](string a) {
      return [&](string b) {
        return std::equal(a.begin(), a.end(),
          b.begin(), b.end(),
          [](char a, char b) {
            return tolower(a) == tolower(b);
          });
      };
    };

    std::vector<std::string> split(const std::string& s, char delim) {
      std::vector<std::string> tokens;
      std::string tok;
      std::istringstream iss(s);
      while (std::getline(iss, tok, delim))
        tokens.push_back(tok);
      
      return tokens;
    }

    inline string generate_uuid(int vers = UUID_MAKE_V4) {
      uuid id;
      id.make(vers);
      const char* sid = id.string();
      std::string r(sid);
      delete sid;

      return std::move(r);
    }

    inline string generate_uuid_v3(const char* url) {
      uuid id;
      uuid id_ns;

      id_ns.load("ns:URL");
      id.make(UUID_MAKE_V3, &id_ns, url);
      const char* sid = id.string();
      std::string r(sid);
      delete sid;

      return std::move(r);
    }

    template<typename R, typename... T>
    R getPathValueFromJson(const shared_ptr<json>& j, T&&... args) {
      vector<string> params = { args... };
      
      stringstream path;
      for (auto& e : params)
        path << "/" << e;
      json::json_pointer pt(path.str());
      
      R r;
      
      try {
        r = j->at(pt);
      } catch (...) {
        
      }
      return r;
    }

    template<typename R, typename... T>
    R getPathValueFromJson(const json& j, T&&... args) {
      const auto jptr = make_shared<json>(j);

      return getPathValueFromJson<R>(jptr, std::forward<T>(args)...);
    }

    map<string, string> getMapFromJson(const json& j) {
      auto m = map<string, string>{};
      for (auto it = j.begin(); it != j.end(); ++it) {
        m[it.key()] = it.value().get<string>();
      }
      
      return move(m);
    }

    // Author: https://stackoverflow.com/users/433369/user433369
    // https://stackoverflow.com/questions/4689101/how-do-i-convert-a-base64-string-to-hexadecimal-string-in-c
    string str_to_hex(const string& ssir) {
      std::stringstream ss;
      for (int i=0; i<ssir.size(); ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(ssir[i] & 0xff);
      
      return ss.str();
    }

    string base64_to_hex(const string& k) {
      auto decoded = base64_decode(k);
      return upper_case(str_to_hex(decoded));
    }

  }
}
