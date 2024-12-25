#include <serialize.h>
#include <iostream>
#include <array>

struct simple {
    int a; int b;
};

struct empty {};

struct test_struct {
    int i32; double f64; float f32;
    std::array<int, 3> vec;
    std::string string;
    simple s;
    empty e;
    std::optional<simple> opt1;
    std::optional<simple> opt2;
};

using namespace std::literals;

std::string_view json = "{\"d\": -123.0e-6, \"c\": \"abcd\", \"a\": [1, 2, 3], \"e\": {}, \"f\": {\"a\": 3.5, \"b\": [3.5, 3.14]}, \"opt1\": null, \"opt2\": 3, \"flag\": false}";

int main() {
#ifdef DEBUG
    std::cout << "debug active" << std::endl;
#else
    std::cout << "release active" << std::endl;
#endif
    std::string serialized_ts = serializez::serialize(test_struct{
                .i32=0,
                .f64=29.483,
                .f32=.12,
                .vec={123, 124, 248},
                .string="std string",
                .s={15, 16},
                .opt2=std::make_optional<simple>(1, 2)
            }
        );
    std::cout << serialized_ts << std::endl;
    test_struct ts{};
    int err = serializez::deserialize(ts, std::string_view{serialized_ts});
    if (err) {
        std::cout << "Error code: " << err << std::endl;
    } else {
        std::cout << "Success!" << std::endl;
    }
    auto* tokenizer = new serializez::detail::Tokenizer(json);
    tokenizer->next();

#ifdef DEBUG
    using namespace serializez::detail;
    auto parse_result = parse_json(tokenizer);
    if (!parse_result) {
        std::cout << "Parse result is nullptr";
    } else {
        std::cout << "Top level node is " << parse_result->class_name() << std::endl;
        std::cout << "a is node " << As<Members>(parse_result)->get("a")->class_name() << std::endl;
        std::cout << "opt1 is node " << As<Members>(parse_result)->get("opt1")->class_name() << std::endl;
    }
#endif
} 
