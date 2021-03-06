cmake_minimum_required(VERSION 3.10)
project(lru_benchmark)

set(CMAKE_CXX_STANDARD 17)

# find_package(Folly)
find_package(OpenMP REQUIRED)

add_subdirectory(thirdparty/tbb)

include_directories(src)
add_definitions(-DTBB_PREVIEW_CONCURRENT_LRU_CACHE=1)
include_directories(thirdparty/tbb/include)
include_directories(thirdparty/hhvm)
include_directories(thirdparty/args/include)

add_executable(lru_benchmark src/main.cpp src/key_generator.cpp src/benchmark.cpp)
#target_compile_options(lru_benchmark PRIVATE -fsanitize=thread)
#target_link_libraries(lru_benchmark PRIVATE tsan)
target_link_libraries(lru_benchmark PRIVATE tbb_static glog OpenMP::OpenMP_CXX)

add_executable(lru_trace_benchmark src/trace_main.cpp src/key_generator.cpp src/benchmark.cpp)
target_link_libraries(lru_trace_benchmark PRIVATE tbb_static glog OpenMP::OpenMP_CXX)

add_executable(increment_test src/concurrent_increment_test.cpp)
target_link_libraries(increment_test PRIVATE OpenMP::OpenMP_CXX)
