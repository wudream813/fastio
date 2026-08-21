CXX      := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -Iinclude

.PHONY: all test bench benchlib benchvar clean

all: example demo correctness benchmark bench_lib variants_test variants_bench

example: src/example.cpp include/fastio.hpp
	$(CXX) $(CXXFLAGS) -o example src/example.cpp

demo: src/demo.cpp include/fastio.hpp
	$(CXX) $(CXXFLAGS) -o demo src/demo.cpp

correctness: src/correctness.cpp include/fastio.hpp
	$(CXX) $(CXXFLAGS) -o correctness src/correctness.cpp

bench_lib: src/bench_lib.cpp include/fastio.hpp
	$(CXX) $(CXXFLAGS) -o bench_lib src/bench_lib.cpp

variants_test: src/variants_test.cpp include/fastio_mmap.hpp include/fastio_ultra.hpp include/fastio_fread.hpp include/fastio_streambuf.hpp
	$(CXX) $(CXXFLAGS) -o variants_test src/variants_test.cpp

variants_bench: src/variants_bench.cpp include/fastio.hpp include/fastio_mmap.hpp include/fastio_ultra.hpp include/fastio_fread.hpp include/fastio_streambuf.hpp
	$(CXX) $(CXXFLAGS) -o variants_bench src/variants_bench.cpp

benchmark: src/benchmark.cpp include/fastio.hpp
	$(CXX) $(CXXFLAGS) -o benchmark src/benchmark.cpp

# 正确性 + 最小示例 + 四种独立写法自检
test: correctness example variants_test
	./correctness
	./variants_test
	printf '5\n1 -2 3 4 5\n' | ./example

# 库 vs cin/cout，默认 100MiB
benchlib: bench_lib
	./bench_lib 100

# 四种独立写法横向对比（100MiB）
benchvar: variants_bench
	./variants_bench 100

# 全档位对照（11 档读 + 8 档写），输出 report.md / report.html / results.json
bench: benchmark
	./benchmark 100 3

clean:
	rm -f example demo correctness benchmark bench_lib variants_test variants_bench
