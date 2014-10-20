#ifndef MORDOR_STRING_CONV_H_
#define MORDOR_STRING_CONV_H_

namespace Mordor{

namespace Internal{
template <typename T> static inline T fromString(const std::string& str);
template <typename T> struct StringNativeConvBase;
}

template <typename T, template <typename> class Base = Internal::StringNativeConvBase>
struct StringNativeConv : public Base<T>
{
    using Base<T>::toString;
    static inline T fromString(const std::string& str) throw(std::invalid_argument){
        return Internal::fromString<T>(str);
    }
};

namespace Internal{

template <typename T>
static inline T fromString(const std::string& str){
    throw std::invalid_argument(str);
}

template <>
inline std::string fromString<std::string>(const std::string& str){
    return str;
}

template <>
inline bool fromString<bool>(const std::string& str){
    return (str == "true" || str == "True" || str == "TRUE") ? true: false;
}

template <>
inline int fromString<int>(const std::string& str){
    return std::stoi(str);
}

template <>
inline long fromString<long>(const std::string& str){
    return std::stol(str);
}

template <>
inline unsigned long fromString<unsigned long>(const std::string& str){
    return std::stoul(str);
}

template <>
inline long long fromString<long long>(const std::string& str){
    return std::stoll(str);
}

template <>
inline unsigned long long fromString<unsigned long long>(const std::string& str){
    return std::stoull(str);
}

template <>
inline float fromString<float>(const std::string& str){
    return std::stof(str);
}

template <>
inline double fromString<double>(const std::string& str){
    return std::stod(str);
}

template <>
inline long double fromString<long double>(const std::string& str){
    return std::stold(str);
}

template <typename T>
struct StringNativeConvBase
{
    static inline std::string toString(const T& val){
        return std::to_string(val);
    }
};

template <>
struct StringNativeConvBase<std::string>
{
    static inline std::string toString(const std::string& val){
        return val;
    }
};

template <>
struct StringNativeConvBase<bool>
{
    static inline std::string toString(const bool& val){
        return val ? "true" : "false";
    }
};

} // namespace Internal

} // namespace Mordor

#endif // MORDOR_STRING_CONV_H_
