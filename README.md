## Serialize any aggregate type to JSON
Implemented using [qlibs.reflect](https://github.com/qlibs/reflect)
# Requirements
- C++20
# Build
```
$ mkdir -p build
$ cmake -S . -B build/
$ cmake --build build
```
# Run example
```
$ ./build/example
{"i32":483,"f64":0.007000,"f32":0.453000,"vec":[1,2,3],"str":"abc","s":{"a":5,"b":6},"e":{},"opt1":"null","opt2":{"a":7,"b":8}}
```
