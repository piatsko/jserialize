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
    const char* str;
    std::string string;
    simple s;
    empty e;
    std::optional<simple> opt1;
    std::optional<simple> opt2;
};

std::string global_str{"global"};

int main() {
    std::cout << serialize::serialize(
        test_struct{
            .i32=483,
            .f64=.007,
            .f32=.453,
            .vec={1,2,3},
            .str="abc",
            .string={"string"},
            .s={5,6},
            .e={},
            .opt1={},
            .opt2=simple{7,8}
        }
    ) << std::endl;
}
