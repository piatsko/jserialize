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
        template <typename U, typename Stream>
        static void serialize(U&& obj, Stream& stream) {
            stream << '{';
            reflect::for_each([&](auto I){
                auto member_name = reflect::member_name<I, T>();
                auto& ith_member = reflect::get<I>(obj);
                using member_t = std::remove_cvref_t<decltype(ith_member)>;

                stream << '\"' << member_name << '\"' << ':';
                serializer_impl<member_t>::serialize(ith_member, stream);
                stream << ',';
            }, obj);
            ::drop_last_char(stream);
            stream << '}';
        }
    };

    template <sized_forward_range T>
    struct serializer_impl<T> {
        template <typename U, typename Stream>
        static void serialize(U&& range, Stream& stream) {
            stream << '[';
            for (auto it = std::ranges::begin(range); it != std::ranges::end(range); ++it) {
                using value_t = std::remove_cvref_t<decltype(*it)>;
                serializer_impl<value_t>::serialize(*it, stream);
                stream << ',';
            }
            ::drop_last_char(stream);
            stream << ']';
        }
    };
    
    template <any_string T>
    struct serializer_impl<T> {
        template <typename U, typename Stream>
        static void serialize(U&& str, Stream& stream) {
            stream <<  '\"' << std::string(str) << '\"';
        }
    };

    template <numeric_except_bool T>
    struct serializer_impl<T> {
        template <typename U, typename Stream>
        static void serialize(U&& obj, Stream& stream) {
           stream << std::to_string(obj); 
        }
    };

    template<>
    struct serializer_impl<bool> {
        template <typename Stream>
        static void serialize(bool obj, Stream& stream) {
            stream << (obj ? "true" : "false");
        }
    };

    template<typename T>
    struct serializer_impl<std::optional<T>> {
        template <typename U, typename Stream>
        static void serialize(U&& obj, Stream& stream) {
            if (obj.has_value()) {
                serializer_impl<T>::serialize(std::forward<T>(*obj), stream);
            }
            stream << "\"null\"";
        }
    };
}

template <typename T>
std::string serialize(T&& obj) {
    std::stringstream stream;
    detail::serializer_impl<T>::serialize(std::forward<T>(obj), stream);
    return stream.str();
}
}
 
