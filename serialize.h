#include <reflect>
#include <concepts>
#include <string>
#include <sstream>
#include <type_traits>
#include <ranges>
#include <optional>
#include <utility>

namespace {
    void drop_last_char(std::stringstream& stream) {
        if (stream.tellp() > 1) {
            stream.seekp(-1, std::ios_base::end); 
        }
    }

    template <typename T>
    concept any_string = std::same_as<T, std::string> ||
                         std::same_as<T, std::string_view> ||
                         std::convertible_to<T, std::string>;
 
    
    template <typename T>
    concept sized_forward_range = std::ranges::forward_range<T>
                                  &&
                                  std::ranges::sized_range<T>
                                  && (not any_string<T>);
    
    template <typename T>
    concept numeric = std::integral<T> || std::floating_point<T>;
    
   template <typename T>
    concept numeric_except_bool = numeric<T> && (not std::same_as<T, bool>);
}


namespace serialize {
namespace detail {
    template <typename T>
    struct serializer_impl {
        template <typename U>
        static std::string serialize(U&& obj) {
            std::stringstream serialized_obj;
            serialized_obj << '{';
            reflect::for_each([&](auto I){
                auto member_name = reflect::member_name<I, T>();
                auto& ith_member = reflect::get<I>(obj);
                serialized_obj << '\"' << member_name << '\"'
                               << ':'
                               << serializer_impl<std::remove_cvref_t<decltype(ith_member)>>::serialize(ith_member)
                               << ',';
            }, obj);
            ::drop_last_char(serialized_obj);
            serialized_obj << '}';
            return serialized_obj.str(); 
        }
    };

    template <sized_forward_range T>
    struct serializer_impl<T> {
        template <typename U>
        static std::string serialize(U&& range) {
            std::stringstream serialized_range;
            serialized_range << '[';
            for (auto it = std::ranges::begin(range); it != std::ranges::end(range); ++it) {
                auto& value = *it;
                serialized_range << serializer_impl<std::remove_cvref_t<decltype(value)>>::serialize(value) << ',';
            }
            ::drop_last_char(serialized_range);
            serialized_range << ']';
            return serialized_range.str(); 
        }
    };
    
    template <any_string T>
    struct serializer_impl<T> {
        template <typename U>
        static std::string serialize(U&& str) {
            return "\"" + std::string(str) + '\"';
        }
    };

    template <numeric_except_bool T>
    struct serializer_impl<T> {
        template <typename U>
        static std::string serialize(U&& obj) {
           return std::to_string(obj); 
        }
    };

    template<>
    struct serializer_impl<bool> {
        static std::string serialize(bool obj) {
            return obj ? "true" : "false";
        }
    };

    template<typename T>
    struct serializer_impl<std::optional<T>> {
        template <typename U>
        static std::string serialize(U&& obj) {
            if (obj.has_value()) {
                return serializer_impl<std::remove_cvref_t<decltype(obj.value())>>::serialize(obj.value());
            }
            return "\"null\"";
        }
    };
}

template <typename T>
std::string serialize(T&& obj) {
    return detail::serializer_impl<std::remove_cvref_t<decltype(obj)>>::serialize(std::forward<T>(obj));
}
}
 
