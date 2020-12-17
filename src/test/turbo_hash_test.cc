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
#include "util/robin_hood.h"

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


void Record2Test() {
    char* buf = (char*)malloc(1024);
    typedef turbo::unordered_map<int, int> HashTable;

    {
        char key = '9';
        char val = 'b';
        typedef HashTable::Record2<decltype(key), decltype(val)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key, val);
        std::cout << "Record lenght: " << Record::FormatLength(key, val) << ". first: "  << record_ptr->first() << ", second: " << record_ptr->second() << std::endl;
    }

    {
        char key = 'b';
        int16_t val = 101;
        typedef HashTable::Record2<decltype(key), decltype(val)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key, val);
        std::cout << "Record lenght: " << Record::FormatLength(key, val) << ". first: "  << record_ptr->first() << ", second: " << record_ptr->second() << std::endl;
    }

    {
        char key = 'c';
        int32_t val = 202;
        typedef HashTable::Record2<decltype(key), decltype(val)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key, val);
        std::cout << "Record lenght: " << Record::FormatLength(key, val) << ". first: "  << record_ptr->first() << ", second: " << record_ptr->second() << std::endl;
    }

    {
        char key = 'd';
        uint64_t val = 303;
        typedef HashTable::Record2<decltype(key), decltype(val)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key, val);
        std::cout << "Record lenght: " << Record::FormatLength(key, val) << ". first: "  << record_ptr->first() << ", second: " << record_ptr->second() << std::endl;
    }

    {
        char key = 'e';
        double val = 404.404;
        typedef HashTable::Record2<decltype(key), decltype(val)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key, val);
        std::cout << "Record lenght: " << Record::FormatLength(key, val) << ". first: "  << record_ptr->first() << ", second: " << record_ptr->second() << std::endl;
    }

    {
        std::string key = "key101";
        double val = 666.666;
        typedef HashTable::Record2<decltype(key), decltype(val)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key, val);
        std::cout << "Record lenght: " << Record::FormatLength(key, val) << ". first: "  << record_ptr->first() << ", second: " << record_ptr->second() << std::endl;
    }

    {
        double key = 999.999;
        std::string val = "val101";
        typedef HashTable::Record2<decltype(key), decltype(val)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key, val);
        std::cout << "Record lenght: " << Record::FormatLength(key, val) << ". first: "  << record_ptr->first() << ", second: " << record_ptr->second() << std::endl;
    }

    {
        std::string key = "key202";
        std::string val = "val202";
        typedef HashTable::Record2<decltype(key), decltype(val)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key, val);
        std::cout << "Record lenght: " << Record::FormatLength(key, val) << ". first: "  << record_ptr->first() << ", second: " << record_ptr->second() << std::endl;
    }

    {
        int key = 303;
        std::string val = "val303";
        typedef HashTable::Record2<decltype(key), decltype(val)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key, val);
        std::cout << "Record lenght: " << Record::FormatLength(key, val) << ". first: "  << record_ptr->first() << ", second: " << record_ptr->second() << std::endl;
    }

    {
        uint16_t key = 404;
        std::string val = "val404";
        typedef HashTable::Record2<decltype(key), decltype(val)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key, val);
        std::cout << "Record lenght: " << Record::FormatLength(key, val) << ". first: "  << record_ptr->first() << ", second: " << record_ptr->second() << std::endl;
    }

    {
        char key = 'f';
        std::string val = "val505";
        typedef HashTable::Record2<decltype(key), decltype(val)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key, val);
        std::cout << "Record lenght: " << Record::FormatLength(key, val) << ". first: "  << record_ptr->first() << ", second: " << record_ptr->second() << std::endl;
    }

    {
        std::string key = "key606";
        char val = 'g';
        typedef HashTable::Record2<decltype(key), decltype(val)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key, val);
        std::cout << "Record lenght: " << Record::FormatLength(key, val) << ". first: "  << record_ptr->first() << ", second: " << record_ptr->second() << std::endl;
    }

    {
        char key = 'A';
        typedef HashTable::Record1<decltype(key)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key);
        std::cout << "Record first: " << record_ptr->first() << std::endl;
    }

    {
        int16_t key = 1239;
        typedef HashTable::Record1<decltype(key)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key);
        std::cout << "Record first: " << record_ptr->first() << std::endl;
    }

    {
        int32_t key = 987654321;
        typedef HashTable::Record1<decltype(key)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key);
        std::cout << "Record first: " << record_ptr->first() << std::endl;
    }

    {
        int64_t key = 101010101;
        typedef HashTable::Record1<decltype(key)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key);
        std::cout << "Record first: " << record_ptr->first() << std::endl;
    }

    {
        double key = 101.1010101;
        typedef HashTable::Record1<decltype(key)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key);
        std::cout << "Record first: " << record_ptr->first() << std::endl;
    }

    {
        std::string key = "key for record1";
        typedef HashTable::Record1<decltype(key)> Record;
        Record* record_ptr = reinterpret_cast<Record*>(buf);        
        record_ptr->Encode(key);
        std::cout << "Record first: " << record_ptr->first() << std::endl;
    }
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
    Record2Test();
    return 0;
}