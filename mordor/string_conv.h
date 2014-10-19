#ifndef MORDOR_STRING_CONV_H_
#define MORDOR_STRING_CONV_H_

namespace Mordor{

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

template <typename T>
struct StringNativeConv : public StringNativeConvBase<T>
{
    static inline T fromString(const std::string& str){
        throw std::invalid_argument(str);
    }
};

template <>
struct StringNativeConv<std::string> : public StringNativeConvBase<std::string>
{
    static inline std::string fromString(const std::string& str){
        return str;
    }
};

template <>
struct StringNativeConv<int> : public StringNativeConvBase<int>
{
    static inline int fromString(const std::string& str){
        return std::stoi(str);
    }
};

template <>
struct StringNativeConv<long> : public StringNativeConvBase<long>
{
    static inline long fromString(const std::string& str){
        return std::stol(str);
    }
};

template <>
struct StringNativeConv<unsigned long> : public StringNativeConvBase<unsigned long>
{
    static inline unsigned long fromString(const std::string& str){
        return std::stoul(str);
    }
};

template <>
struct StringNativeConv<long long> : public StringNativeConvBase<long long>
{
    static inline long long fromString(const std::string& str){
        return std::stoll(str);
    }
};

template <>
struct StringNativeConv<unsigned long long> : public StringNativeConvBase<unsigned long long>
{
    static inline unsigned long long fromString(const std::string& str){
        return std::stoull(str);
    }
};

template <>
struct StringNativeConv<float> : public StringNativeConvBase<float>
{
    static inline float fromString(const std::string& str){
        return std::stof(str);
    }
};

template <>
struct StringNativeConv<double> : public StringNativeConvBase<double>
{
    static inline double fromString(const std::string& str){
        return std::stod(str);
    }
};

template <>
struct StringNativeConv<long double> : public StringNativeConvBase<long double>
{
    static inline long double fromString(const std::string& str){
        return std::stold(str);
    }
};

template <>
struct StringNativeConv<bool> : public StringNativeConvBase<bool>
{
    static inline bool fromString(const std::string& str){
        return (str == "true" || str == "True" || str == "TRUE") ? true: false;
    }
};

} // namespace Mordor

#endif // MORDOR_STRING_CONV_H_
