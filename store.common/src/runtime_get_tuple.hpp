#pragma once

/*
 * reference:
 * https://stackoverflow.com/questions/21062864/optimal-way-to-access-stdtuple-element-in-runtime-by-index
 * 
 * usage: 
    int main() {
      std::tuple<int, double, char, double> t{1, 2.3, 4, 5.6};
      for(int i = 0; i < 4; ++i) apply(t, i, Func{});
    } 
 */
#include <iostream>
#include <tuple>
#include <typeinfo>
#include <sstream>

using namespace std;

namespace store::common::tuples {

  template<int... Is> struct seq {};
  template<int N, int... Is> struct gen_seq : gen_seq<N-1, N-1, Is...> {};
  template<int... Is> struct gen_seq<0, Is...> : seq<Is...> {};

  template<int N, class T, class F>
  void apply_one(T& p, F func) {
      func( std::get<N>(p) );
  }

  template<class T, class F, int... Is>
  void apply(T& p, int index, F func, seq<Is...>) {
      using FT = void(T&, F);
      static constexpr FT* arr[] = { &apply_one<Is, T, F>... };
      arr[index](p, func);
  }

  template<class T, class F>
  void apply(T& p, int index, F func) {
      apply(p, index, func, gen_seq<std::tuple_size<T>::value>{});
  }

  struct TupleFormatFunc {
    TupleFormatFunc(stringstream* ss_) : ss(ss_) {};

    template<class U>
    void operator()(U p) {
      auto ty = string(typeid(p).name()).back();
      if (ty == 'c' || ty == 's') {
        *ss << "'" << p << "'";
      } else {
        *ss << p;
      }
      // std::cout << __PRETTY_FUNCTION__ << " : " << p << "\n";
    }

    stringstream* ss;
  };

}
