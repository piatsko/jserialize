## Serialize and deserialize any aggregate type to JSON
Implemented using [qlibs.reflect](https://github.com/qlibs/reflect)

## Note
Actually, not everything can be serialized and deserialized because of limitations of the technique.
Known issues:
- Can not serialize if an object contains static array, not wrapped in std::array
- Can not deserialize if there is `char*` field in an object.
# Requirements
- C++20
# Build
```
$ mkdir -p build
$ cmake -S . -B build/
$ cmake --build build
```

