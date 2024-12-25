#include <reflect>
#include <concepts>
#include <string>
#include <sstream>
#include <type_traits>
#include <ranges>
#include <optional>
#include <utility>
#include <span>
#include <memory>
#include <vector>
#include <list>
#include <map>

#ifdef DEBUG
#include <iostream>
#endif

namespace {

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


namespace serializez {
namespace detail {

template <typename T>
struct serializer_impl {
    template <typename U, std::size_t N, typename Stream>
    static void serialize(U (&array)[N], Stream& stream) {
        std::span<U, N> arr_view{array};
        serializer_impl<std::remove_cvref_t<decltype(arr_view)>>::serialize(arr_view, stream);
    }

    template <typename U, typename Stream>
    static void serialize(U&& obj, Stream& stream) {
        stream << '{';
        auto pos_before = stream.tellp();
        reflect::for_each([&](auto I){
            auto member_name = reflect::member_name<I, T>();
            auto& ith_member = reflect::get<I>(obj);
            using member_t = std::remove_cvref_t<decltype(ith_member)>;

            stream << '\"' << member_name << '\"' << ':';
            serializer_impl<member_t>::serialize(ith_member, stream);
            stream << ',';
        }, obj);
        if (stream.tellp() != pos_before) {
            stream.seekp(-1, std::ios_base::end); 
        }
        stream << '}';
    }
};

template <sized_forward_range T>
struct serializer_impl<T> {
    template <typename U, typename Stream>
    static void serialize(U&& range, Stream& stream) {
        stream << '[';
        auto pos_before = stream.tellp();
        for (auto it = std::ranges::begin(range); it != std::ranges::end(range); ++it) {
            using value_t = std::remove_cvref_t<decltype(*it)>;
            serializer_impl<value_t>::serialize(*it, stream);
            stream << ',';
        }
        if (stream.tellp() != pos_before) {
            stream.seekp(-1, std::ios_base::end); 
        }
        stream << ']';
    }
};

template <any_string T>
struct serializer_impl<T> {
    template <typename U, typename Stream>
    static void serialize(U&& str, Stream& stream) {
        std::string data{std::forward<U>(str)};
        stream <<  '\"';
        for (auto c : data) {
            if (c == '\"') {
                stream << '\\';
            }
            stream << c;
        }
        stream << '\"';
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
        } else {
            stream << "null";
        }
    }
};
} // namespace detail

template <typename T>
std::string serialize(T&& obj) {
    std::stringstream stream;
    detail::serializer_impl<T>::serialize(std::forward<T>(obj), stream);
    return stream.str();
}

namespace detail {

enum class Token {
    CURLY_OPEN,
    CURLY_CLOSE,
    SQUARE_OPEN,
    SQUARE_CLOSE,
    QUOTE,
    COLON,
    COMMA,
    BOOL_TRUE,
    BOOL_FALSE,
    NUMBER_FLOAT,
    NUMBER_INT,
    STRING,
    NULL_TOKEN
};

class Tokenizer {
public:
    std::string_view sv;
    Token token;

    Tokenizer(std::string_view input) : sv(input) { }
    
    inline bool is_end() {
        skip_whitespaces();
        return token_current == sv.length();
    }

    inline void skip() { update_current_pos(); }

    inline void skip_to_next() { skip(); next(); }

    inline std::string_view get_sv() {
        std::size_t start = token_current;
        update_current_pos();
        return sv.substr(start, token_current - start);
    }
    
    inline double get_float() { return std::strtod(get_sv().data(), nullptr); }

    inline std::int64_t get_int() { return std::strtoll(get_sv().data(), nullptr, 10); }

    void next() {
        skip_whitespaces();
        switch (sv[token_current]) {
            case '{':
               token = Token::CURLY_OPEN;
               return;
            case '}':
               token = Token::CURLY_CLOSE;
               return;
            case '[':
               token = Token::SQUARE_OPEN;
                return;
            case ']':
                token = Token::SQUARE_CLOSE;
                return;
            case ':':
                token = Token::COLON;
                return;
            case '"':
                token = Token::QUOTE;
                return;
            case ',':
                token = Token::COMMA;
                return;
        }
        if (try_read_null() != token_current) {
            token_end = token_current + 4;
            token = Token::NULL_TOKEN;
        } else if (try_read_true() != token_current) {
            token_end = token_current + 4;
            token = Token::BOOL_TRUE;
        } else if (try_read_false() != token_current) {
            token_end = token_current + 5;
            token = Token::BOOL_FALSE;
        } else if ((token_end = try_read_number()) && token_end != token_current) {
            auto number_token = sv.substr(token_current, token_end - token_current);
            if (number_token.find_first_of(".eE") != std::string_view::npos) {
                token = Token::NUMBER_FLOAT;
            } else {
                token = Token::NUMBER_INT;
            }
        } else {
            token_end = match_quote();
            token = Token::STRING;
        } 
        return;
    }

private:
    std::size_t token_current=0ul;
    std::size_t token_end=0ul;

    inline void skip_whitespaces() {
        while (token_current < sv.length() && sv[token_current] == ' ')
            ++token_current;
    }

    inline void update_current_pos() {
        if (token_end <= token_current) { token_end = token_current + 1; }
        token_current = token_end; 
    }

    inline std::size_t try_consume_expected(std::size_t s, std::string_view e) const {
        if (token_current < (sv.length() - s) && sv.substr(token_current, s) == e) {
            return token_current + s;
        }
        return token_current;
    }

    inline std::size_t try_read_true() const { return try_consume_expected(4ul, "true"); }

    inline std::size_t try_read_false() const { return try_consume_expected(5ul, "false"); }

    inline std::size_t try_read_null() const { return try_consume_expected(4ul, "null"); }

    inline std::size_t match_quote() const {
        for (std::size_t it = token_current + 1; it < sv.length(); ++it) {
            if (sv[it] == '"' && sv[it - 1] != '\\') {
                return it;
            }
        }
        return sv.length();
    }

    inline std::size_t try_read_number() const {
        bool has_dot = false,
             has_exp = false,
             has_minus = sv[token_current] == '-';
        std::size_t it = token_current + int(has_minus);
        if (has_minus && it < sv.length() && std::isdigit(it)) {
            return token_current;
        }
        for (; it < sv.length(); ++it) {
            if (std::isdigit(sv[it]) == 0) {
                if (sv[it] == '.') {
                    if (!has_dot) {
                        has_dot = true;
                        bool next_valid = it < (sv.length() - 1) &&
                                            std::isdigit(sv[it + 1]);
                        if (next_valid) {
                            continue;
                        }
                    }
                } else if (sv[it] == 'e' or sv[it] == 'E'){
                   if (!has_exp) {
                        has_exp = true;
                        if (it < sv.length() - 1) {
                            std::size_t exp_start = it;
                            char followed_by = sv[exp_start + 1];
                            if (followed_by == '+' or followed_by == '-') {
                               ++exp_start; 
                            }
                            bool next_valid = exp_start < (sv.length() - 1) &&
                                                std::isdigit(sv[exp_start + 1]);
                            if (next_valid) {
                                it = exp_start;
                                continue;
                            }
                        }
                   }
                }
                break;
            }
        }
        return it;
    }
};

#define DEFINE_CLASS_NAME(className) \
    virtual std::string class_name() const override { return #className; }

class JsonNode {
public:
    virtual ~JsonNode() = default;
    using NodePtr = std::shared_ptr<JsonNode>;
#ifdef DEBUG
    virtual std::string class_name() const = 0; 
#endif
};

class Null: public JsonNode {
public:
#ifdef DEBUG
    DEFINE_CLASS_NAME(Null)
#endif
};

class String: public JsonNode {
public:

#ifdef DEBUG
    DEFINE_CLASS_NAME(String)
#endif

    String(std::string_view sv) : str(sv) { }
    std::string_view value() const { return str; }
private:
    std::string_view str;
};

template <numeric T>
class Number: public JsonNode {
public:
    
#ifdef DEBUG
    DEFINE_CLASS_NAME(Number)
#endif

    Number(T num) : number(num) { }
    T value() const { return number; }
private:
    T number;
};

class Bool: public JsonNode {
public:
   
#ifdef DEBUG
    DEFINE_CLASS_NAME(Bool)
#endif

    Bool (bool b) : flag(b) { }
    bool value() const { return flag; }
private:
    bool flag;
};

class Array: public JsonNode {
public:
    
#ifdef DEBUG
    DEFINE_CLASS_NAME(Array)
#endif

    void append(NodePtr value) { array.push_back(value); }
    auto begin() { return array.begin(); }
    auto end() { return array.end(); }
    std::size_t size() { return array.size(); }
private:
    std::list<NodePtr> array;
};

class Members: public JsonNode {
public:
    
#ifdef DEBUG
    DEFINE_CLASS_NAME(Members)
#endif

    void add(std::string_view key, NodePtr value) {
        members.insert({key, value});
    }
    NodePtr& get(std::string_view key) { return members.at(key); }
private:
    std::map<std::string_view, NodePtr> members{};
};
 
template <class T>
std::shared_ptr<T> As(const std::shared_ptr<JsonNode>& obj) {
    return std::dynamic_pointer_cast<T>(obj);
}

template <class T>
bool Is(const std::shared_ptr<JsonNode>& obj) {
    return std::dynamic_pointer_cast<T>(obj) != nullptr;
}

std::shared_ptr<JsonNode> parse_json(Tokenizer*);
std::shared_ptr<JsonNode> parse_array(Tokenizer*);
std::shared_ptr<JsonNode> parse_string(Tokenizer* tokenizer) {
    if (tokenizer->token != Token::QUOTE) { return nullptr; }
    tokenizer->skip_to_next();
    if (tokenizer->token != Token::STRING) { return nullptr; }
    auto key = std::make_shared<String>(tokenizer->get_sv());
    tokenizer->next();
    if (tokenizer->token != Token::QUOTE) { return nullptr; }
    tokenizer->skip_to_next();
    return key;
}

std::shared_ptr<JsonNode> parse_value(Tokenizer* tokenizer) {
    std::shared_ptr<JsonNode> value;
    if (tokenizer->token == Token::CURLY_OPEN) {
        value = parse_json(tokenizer);
    } else if (tokenizer->token == Token::SQUARE_OPEN) {
        value = parse_array(tokenizer);
    } else if (tokenizer->token == Token::NUMBER_INT) {
        value = std::make_shared<Number<long long>>(tokenizer->get_int());
    } else if (tokenizer->token == Token::NUMBER_FLOAT) {
        value = std::make_shared<Number<double>>(tokenizer->get_float());
    } else if (tokenizer->token == Token::BOOL_TRUE ||
               tokenizer->token == Token::BOOL_FALSE) {
        value = std::make_shared<Bool>(tokenizer->get_sv() == "true");
    } else if (tokenizer->token == Token::NULL_TOKEN) {
        value = std::make_shared<Null>();
        tokenizer->skip();
    } else if (tokenizer->token == Token::QUOTE) {
        value = parse_string(tokenizer); 
    } else { 
        return nullptr;
    }
    tokenizer->next();
    return value;
}

std::shared_ptr<JsonNode> parse_array(Tokenizer* tokenizer) {
    if (tokenizer->token != Token::SQUARE_OPEN) { return nullptr; }
    tokenizer->skip_to_next();
    auto array = std::make_shared<Array>();
    while (!tokenizer->is_end()) {
        std::shared_ptr<JsonNode> value = parse_value(tokenizer);
#ifdef DEBUG
        if (!value) { std::cout << "Could not parse value" << std::endl; }
        else { std::cout << "Array value parsed correctly" << std::endl; }
#endif
        if (!value) { return nullptr; }
        array->append(value);

        if (tokenizer->token == Token::COMMA) {
            tokenizer->skip_to_next();
        } else if (tokenizer->token == Token::SQUARE_CLOSE) {
            tokenizer->skip_to_next();
            break;
        } else {
#ifdef DEBUG
            std::cout << "No comma or closing square bracket" << std::endl;
#endif
            return nullptr;
        }
    }
    return array;
}

std::shared_ptr<JsonNode> parse_json(Tokenizer* tokenizer) {
    if (tokenizer->token != Token::CURLY_OPEN) { return nullptr; }
    tokenizer->skip_to_next();
    auto members = std::make_shared<Members>(); 
    if (tokenizer->token == Token::CURLY_CLOSE) { 
        tokenizer->skip_to_next();
        return members; 
    }
    while (!tokenizer->is_end()) {
        std::shared_ptr<JsonNode> name = parse_string(tokenizer);
#ifdef DEBUG
        if (!name) { std::cout << "Could not parse name" << std::endl; }
        else { std::cout << "Name parsed correctly: " << As<String>(name)->value() << std::endl; }
#endif  
        if (!name) {
            return nullptr;
        }

        if (tokenizer->token != Token::COLON) {
#ifdef DEBUG
            std::cout << "No colon after name" << std::endl;
#endif
            return nullptr;
        }
        tokenizer->skip_to_next();

        std::shared_ptr<JsonNode> value = parse_value(tokenizer);
#ifdef DEBUG
        if (!value) { std::cout << "Could not parse value" << std::endl; }
        else { std::cout << "Value parsed correctly" << std::endl; }
#endif  
        if (!value) { return nullptr; }

        members->add(As<String>(name)->value(), value);
#ifdef DEBUG
        std::cout << "Add new member" << std::endl;
#endif
        if (tokenizer->token == Token::COMMA) {
            tokenizer->skip_to_next();
        } else if (tokenizer->token == Token::CURLY_CLOSE) {
            tokenizer->skip_to_next();
            break;
        } else {
            return nullptr;
        }
    }
    return members;
}

template <typename To>
struct deserializer_impl {
    static int deserialize(To& obj, std::shared_ptr<JsonNode>& member_node) {
        if (!member_node || !Is<Members>(member_node)) { return 1; }
        int err = 0;
        reflect::for_each([&](auto I) {
            auto member_name = reflect::member_name<I, To>();
            auto& ith_member = reflect::get<I>(obj);
            auto& value = As<Members>(member_node)->get(member_name);
            using member_t = std::remove_cvref_t<decltype(ith_member)>;
            int err = deserializer_impl<member_t>::deserialize(ith_member, value);
            err = std::max(0, err);
        }, obj);
        return err;
    }
};   

template <sized_forward_range To>
struct deserializer_impl<To> {
    static int deserialize(To& obj, std::shared_ptr<JsonNode>& array_node) {
        using value_t = std::ranges::range_value_t<To>;
        if (!array_node || !Is<Array>(array_node)) { return 1; }
        std::vector<value_t> array_deserialized;
        array_deserialized.reserve(As<Array>(array_node)->size());
        for (auto& it : *As<Array>(array_node)) {
            value_t item{};
            int err = deserializer_impl<value_t>::deserialize(item, it);
            if (err) { return err; }
            array_deserialized.push_back(item);
        }
        std::move(std::ranges::begin(array_deserialized),
                  std::ranges::end(array_deserialized),
                  std::ranges::begin(obj));
        return 0;
    }
};

template <numeric_except_bool To>
struct deserializer_impl<To> {
    static int deserialize(To& obj, std::shared_ptr<JsonNode>& number_node) {
        if (!number_node || (!Is<Number<long long>>(number_node) && !Is<Number<double>>(number_node))) {
            return 1;
        }
        if constexpr (std::is_integral_v<std::remove_cvref_t<To>>) {
            obj = As<Number<long long>>(number_node)->value();
        } else {
            obj = As<Number<double>>(number_node)->value();
        }
        return 0;
    }
};

template <>
struct deserializer_impl<bool> {
    static int deserialize(bool& obj, std::shared_ptr<JsonNode>& bool_node) {
        if (!bool_node || !Is<Bool>(bool_node)) {
            return 1;
        }
        obj = As<Bool>(bool_node)->value();
        return 0;
    }
};

template <any_string To>
struct deserializer_impl<To> {
    static int deserialize(To& obj, std::shared_ptr<JsonNode>& string_node) {
        if (!string_node || !Is<String>(string_node)) { return 1; }
        obj = As<String>(string_node)->value();
        return 0;
    }
};

template <typename To>
struct deserializer_impl<std::optional<To>> {
    static int deserialize(std::optional<To>& to, std::shared_ptr<JsonNode>& opt) {
        if (!opt) { return 1; }
        if (Is<Null>(opt)) { to.reset(); return 0;}
        To& temp = *to;
        int err = deserializer_impl<To>::deserialize(temp, opt);
        if (err) { return err; }
        if (!to) { to = temp; }
        return 0;
    }
};
} //namespace detail

template <typename To>
int deserialize(To& to, std::string_view json) {
    auto* tokenizer = new detail::Tokenizer(json);
    tokenizer->next();
    std::shared_ptr<detail::JsonNode> json_node = parse_json(tokenizer);
    if (!tokenizer->is_end()) { return 2; }
    delete tokenizer;
    return detail::deserializer_impl<To>::deserialize(to, json_node);
}
} // namespace serializez

