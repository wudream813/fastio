CXX      := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -Iinclude
HDRS     := $(wildcard include/*.hpp)

.PHONY: all test test-options bench benchlib benchvar benchopt clean

all: example demo correctness benchmark bench_lib variants_test variants_bench options_test options_bench

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

# --- 减分支/激进选项：同一源文件用不同 -D 组合各编译一遍 ---------------------
OPT_COMBOS := options_test opt_no_eof opt_unsigned opt_steps opt_cincout opt_combo \
              opt_inmax opt_outmax opt_iomax opt_nows opt_nows_u opt_nows_c

options_test: src/options_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $<
opt_no_eof: src/options_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DFASTIO_NO_EOF_CHECK -o $@ $<
opt_unsigned: src/options_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DFASTIO_ASSUME_UNSIGNED -o $@ $<
opt_steps: src/options_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DFASTIO_PAIR_STEPS_INT=3 -DFASTIO_PAIR_STEPS_LL=4 -o $@ $<
opt_cincout: src/options_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DFASTIO_REPLACE_CIN_COUT -o $@ $<
opt_combo: src/options_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DFASTIO_NO_EOF_CHECK -DFASTIO_ASSUME_UNSIGNED -DFASTIO_PAIR_STEPS_INT=3 -DFASTIO_PAIR_STEPS_LL=4 -o $@ $<
opt_inmax: src/options_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DFASTIO_INPUT_MAX=4194304 -o $@ $<
opt_outmax: src/options_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DFASTIO_OUTPUT_MAX=2097152 -o $@ $<
opt_iomax: src/options_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DFASTIO_INPUT_MAX=4194304 -DFASTIO_OUTPUT_MAX=2097152 -DFASTIO_NO_EOF_CHECK -o $@ $<
opt_nows: src/options_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DFASTIO_NO_WS_SKIP -o $@ $<
opt_nows_u: src/options_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DFASTIO_NO_WS_SKIP -DFASTIO_ASSUME_UNSIGNED -o $@ $<
opt_nows_c: src/options_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DFASTIO_NO_WS_SKIP -DFASTIO_NO_EOF_CHECK -DFASTIO_PAIR_STEPS_INT=3 -DFASTIO_PAIR_STEPS_LL=4 -o $@ $<

# 选项收益对照（100MiB，同一数据两个二进制先后跑）
options_bench: src/options_bench.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $<
options_bench_fast: src/options_bench.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DFASTIO_NO_EOF_CHECK -DFASTIO_ASSUME_UNSIGNED -DFASTIO_PAIR_STEPS_INT=4 -DFASTIO_PAIR_STEPS_LL=9 -o $@ $<

# 正确性 + 最小示例 + 全部独立写法自检
test: correctness example variants_test test-options
	./correctness
	./variants_test
	printf '5\n1 -2 3 4 5\n' | ./example

# 9 种编译期选项组合逐一自检
# （< /dev/null：INPUT_MAX 会把 stdin 一口气读到 EOF，防交互终端挂住）
test-options: $(OPT_COMBOS)
	./options_test < /dev/null
	./opt_no_eof < /dev/null
	./opt_unsigned < /dev/null
	./opt_steps < /dev/null
	./opt_cincout < /dev/null
	./opt_combo < /dev/null
	./opt_inmax < /dev/null
	./opt_outmax < /dev/null
	./opt_iomax < /dev/null
	./opt_nows < /dev/null
	./opt_nows_u < /dev/null
	./opt_nows_c < /dev/null

# 库 vs cin/cout，默认 100MiB
benchlib: bench_lib
	./bench_lib 100

# 四种独立写法横向对比（100MiB）
benchvar: variants_bench
	./variants_bench 100

# 减分支选项收益对照（100MiB：默认配置 vs 全选项）
benchopt: options_bench options_bench_fast
	./options_bench 100
	./options_bench_fast 100

# 全档位对照（11 档读 + 8 档写），输出 report.md / report.html / results.json
bench: benchmark
	./benchmark 100 3

clean:
	rm -f example demo correctness benchmark bench_lib variants_test variants_bench \
	      $(OPT_COMBOS) options_bench options_bench_fast
