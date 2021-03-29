#include <type_traits>
#include <typeinfo>
#ifndef _MSC_VER
#   include <cxxabi.h>
#endif
#include <memory>
#include <string>
#include <cstdlib>
#include <iostream>

#include "turbo/turbo_hash.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;


template <class T>
std::string
type_name()
{
    typedef typename std::remove_reference<T>::type TR;
    std::unique_ptr<char, void(*)(void*)> own
           (
#ifndef _MSC_VER
                abi::__cxa_demangle(typeid(TR).name(), nullptr,
                                           nullptr, nullptr),
#else
                nullptr,
#endif
                std::free
           );
    std::string r = own != nullptr ? own.get() : typeid(TR).name();
    if (std::is_const<TR>::value)
        r += " const";
    if (std::is_volatile<TR>::value)
        r += " volatile";
    if (std::is_lvalue_reference<T>::value)
        r += "&";
    else if (std::is_rvalue_reference<T>::value)
        r += "&&";
    return r;
}

template<typename Key, typename T>
void TestPair() {
    Key k;
    T   v;
    printf("size of std::pair<%s, %s> is: %lu\n", type_name<decltype(k)>().c_str(), type_name<decltype(v)>().c_str(), sizeof(std::pair<Key, T>));
    
}


int main() {
    TestPair<bool, bool>();
    TestPair<char, char>();
    TestPair<uint8_t, uint8_t>();
    TestPair<uint16_t, uint16_t>();
    TestPair<uint32_t, uint32_t>();
    TestPair<uint64_t, uint64_t>();
    TestPair<float,    float>();
    TestPair<double,   double>();
    TestPair<std::string, std::string>();
    return 0;
}