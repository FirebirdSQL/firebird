# Modern C++ features

We define here as "modern C++" features the ones introduced since C++11. These features should be used in a "controlled" form in Firebird code, as not all compilers fully support them.

Only ones mentioned in this document could be used, but as necessities appears, discussion should be started in the devel list.

## Allowed features

### C++11

- auto [v0.9](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n1984.pdf), [v1.0](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2546.htm)
- lambda expressions [v0.9](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2550.pdf), [v1.0](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2658.pdf), [v1.1](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2009/n2927.pdf)
- [nullptr](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2431.pdf)
- [range-based for loop](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2009/n2930.html)
- [initializer lists](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2672.htm)
- [non-static data member initializers](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2756.htm)
- override [v0.8](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2009/n2928.htm), [v0.9](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2010/n3206.htm), [v1.0](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2011/n3272.htm)
- [static_assert](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2004/n1720.html)
- [polymorphic function wrappers](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2002/n1402.html)
- [function object binders](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2003/n1455.htm)
- [function template mem_fn](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2003/n1432.htm)
- [rvalue references](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n2027.html)
- [strongly-typed enum](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2347.pdf)
- [atomic types and operations](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2427.html)
- [decltype](https://en.cppreference.com/w/cpp/language/decltype)
- [std::is_convertible](https://en.cppreference.com/w/cpp/types/is_convertible)
- [final specifier](https://en.cppreference.com/w/cpp/language/final)
- [constexpr](https://en.cppreference.com/w/cpp/language/constexpr)
