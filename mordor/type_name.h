#ifndef __MORDOR_TYPE_NAME_H__
#define __MORDOR_TYPE_NAME_H__

#include <string>
#include <typeinfo>

namespace Mordor{

std::string demangle(const char* name);

template <class T>
std::string type_name(const T& t) {

    return demangle(typeid(t).name());
}

}

#endif // __MORDOR_TYPE_NAME_H__
