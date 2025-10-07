# ================================ Makefile for High-Concurrency Memory Pool ================================
# 编译器和编译选项
CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra -Iinclude
THREAD_FLAGS = -pthread

# 目录定义
SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
BUILD_DIR = build
DOCS_DIR = docs

# 源文件
CORE_SOURCES = $(SRC_DIR)/ThreadCache.cpp $(SRC_DIR)/CentralCache.cpp $(SRC_DIR)/PageCache.cpp
HEADERS = $(INCLUDE_DIR)/Common.h $(INCLUDE_DIR)/ThreadCache.h $(INCLUDE_DIR)/CentralCache.h $(INCLUDE_DIR)/PageCache.h $(INCLUDE_DIR)/RadixTree.h $(INCLUDE_DIR)/ObjectPool.h $(INCLUDE_DIR)/ConcurrencyAlloc.h

# 目标文件
TARGETS = $(BUILD_DIR)/test $(BUILD_DIR)/benchmark $(BUILD_DIR)/radix_test

# 默认目标
.PHONY: all clean help run-test run-benchmark run-radix-test

all: $(TARGETS)

# ================================ 编译规则 ================================

# 确保build目录存在
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 功能测试程序
$(BUILD_DIR)/test: $(TEST_DIR)/Test.cpp $(CORE_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) $(TEST_DIR)/Test.cpp $(CORE_SOURCES) -o $@

# 性能测试程序
$(BUILD_DIR)/benchmark: $(TEST_DIR)/BenchMark.cpp $(CORE_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) $(TEST_DIR)/BenchMark.cpp $(CORE_SOURCES) -o $@

# 基数树测试程序
$(BUILD_DIR)/radix_test: $(TEST_DIR)/RadixTreeTest.cpp $(INCLUDE_DIR)/RadixTree.h $(INCLUDE_DIR)/ObjectPool.h $(INCLUDE_DIR)/Common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(TEST_DIR)/RadixTreeTest.cpp -o $@

# 兼容性别名
test: $(BUILD_DIR)/test
benchmark: $(BUILD_DIR)/benchmark
radix_test: $(BUILD_DIR)/radix_test

# ================================ 运行规则 ================================

# 运行功能测试
run-test: $(BUILD_DIR)/test
	@echo "=== 运行功能测试 ==="
	./$(BUILD_DIR)/test

# 运行性能测试
run-benchmark: $(BUILD_DIR)/benchmark
	@echo "=== 运行性能测试 ==="
	./$(BUILD_DIR)/benchmark

# 运行基数树测试
run-radix-test: $(BUILD_DIR)/radix_test
	@echo "=== 运行基数树测试 ==="
	./$(BUILD_DIR)/radix_test

# 运行所有测试
run-all: run-test run-benchmark run-radix-test

# ================================ 调试版本 ================================

# 调试版本编译选项
DEBUG_FLAGS = -std=c++11 -g -O0 -Wall -Wextra -DDEBUG

# 调试版本目标
debug-test: $(TEST_DIR)/Test.cpp $(CORE_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(DEBUG_FLAGS) $(THREAD_FLAGS) $(TEST_DIR)/Test.cpp $(CORE_SOURCES) -o $(BUILD_DIR)/test-debug

debug-benchmark: $(TEST_DIR)/BenchMark.cpp $(CORE_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(DEBUG_FLAGS) $(THREAD_FLAGS) $(TEST_DIR)/BenchMark.cpp $(CORE_SOURCES) -o $(BUILD_DIR)/benchmark-debug

debug-radix: $(TEST_DIR)/RadixTreeTest.cpp $(INCLUDE_DIR)/RadixTree.h $(INCLUDE_DIR)/ObjectPool.h $(INCLUDE_DIR)/Common.h | $(BUILD_DIR)
	$(CXX) $(DEBUG_FLAGS) $(TEST_DIR)/RadixTreeTest.cpp -o $(BUILD_DIR)/radix_test-debug

debug: debug-test debug-benchmark debug-radix

# ================================ 内存检查 ================================

# 使用Valgrind进行内存检查
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes

memcheck-test: debug-test
	$(VALGRIND) ./$(BUILD_DIR)/test-debug

memcheck-benchmark: debug-benchmark
	$(VALGRIND) ./$(BUILD_DIR)/benchmark-debug

memcheck-radix: debug-radix
	$(VALGRIND) ./$(BUILD_DIR)/radix_test-debug

memcheck: memcheck-test memcheck-radix

# ================================ 性能分析 ================================

# 使用gprof进行性能分析
PROFILE_FLAGS = -std=c++11 -O2 -pg

profile-benchmark: $(TEST_DIR)/BenchMark.cpp $(CORE_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(PROFILE_FLAGS) $(THREAD_FLAGS) $(TEST_DIR)/BenchMark.cpp $(CORE_SOURCES) -o $(BUILD_DIR)/benchmark-profile

run-profile: profile-benchmark
	./$(BUILD_DIR)/benchmark-profile
	gprof $(BUILD_DIR)/benchmark-profile gmon.out > $(BUILD_DIR)/profile_report.txt
	@echo "性能分析报告已生成: $(BUILD_DIR)/profile_report.txt"

# ================================ 代码检查 ================================

# 使用cppcheck进行静态代码分析
cppcheck:
	@echo "=== 运行静态代码分析 ==="
	cppcheck --enable=all --std=c++11 --suppress=missingIncludeSystem -I$(INCLUDE_DIR) $(SRC_DIR)/*.cpp $(TEST_DIR)/*.cpp $(INCLUDE_DIR)/*.h

# 代码格式化
format:
	@echo "=== 格式化代码 ==="
	@if command -v clang-format >/dev/null 2>&1; then \
		clang-format -i $(SRC_DIR)/*.cpp $(TEST_DIR)/*.cpp $(INCLUDE_DIR)/*.h; \
		echo "代码格式化完成"; \
	else \
		echo "未找到clang-format，请安装后再试"; \
	fi

# ================================ 清理规则 ================================

# 清理编译生成的文件
clean:
	@echo "=== 清理编译文件 ==="
	rm -rf $(BUILD_DIR)
	rm -f gmon.out
	rm -f *.o *.d
	@echo "清理完成"

# 深度清理（包括备份文件等）
distclean: clean
	rm -f *~ *.bak
	rm -f core.*

# ================================ 安装规则 ================================

# 安装到系统目录（需要root权限）
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
INCLUDEDIR = $(PREFIX)/include/memory-pool

install: all
	@echo "=== 安装到系统目录 ==="
	mkdir -p $(BINDIR)
	mkdir -p $(INCLUDEDIR)
	cp $(BUILD_DIR)/benchmark $(BINDIR)/memory-pool-benchmark
	cp $(INCLUDE_DIR)/*.h $(INCLUDEDIR)/
	@echo "安装完成到 $(PREFIX)"

uninstall:
	@echo "=== 卸载系统文件 ==="
	rm -f $(BINDIR)/memory-pool-benchmark
	rm -rf $(INCLUDEDIR)
	@echo "卸载完成"

# ================================ 帮助信息 ================================

help:
	@echo "High-Concurrency Memory Pool Makefile"
	@echo "======================================"
	@echo ""
	@echo "编译目标:"
	@echo "  all              - 编译所有程序"
	@echo "  test             - 编译功能测试程序"
	@echo "  benchmark        - 编译性能测试程序"
	@echo "  radix_test       - 编译基数树测试程序"
	@echo ""
	@echo "运行目标:"
	@echo "  run-test         - 运行功能测试"
	@echo "  run-benchmark    - 运行性能测试"
	@echo "  run-radix-test   - 运行基数树测试"
	@echo "  run-all          - 运行所有测试"
	@echo ""
	@echo "调试目标:"
	@echo "  debug            - 编译调试版本"
	@echo "  debug-test       - 编译调试版功能测试"
	@echo "  debug-benchmark  - 编译调试版性能测试"
	@echo "  debug-radix      - 编译调试版基数树测试"
	@echo ""
	@echo "内存检查:"
	@echo "  memcheck         - 使用Valgrind检查内存泄漏"
	@echo "  memcheck-test    - 检查功能测试的内存"
	@echo "  memcheck-radix   - 检查基数树测试的内存"
	@echo ""
	@echo "性能分析:"
	@echo "  profile-benchmark - 编译性能分析版本"
	@echo "  run-profile      - 运行性能分析并生成报告"
	@echo ""
	@echo "代码质量:"
	@echo "  cppcheck         - 运行静态代码分析"
	@echo "  format           - 格式化代码"
	@echo ""
	@echo "清理目标:"
	@echo "  clean            - 清理编译文件"
	@echo "  distclean        - 深度清理"
	@echo ""
	@echo "安装目标:"
	@echo "  install          - 安装到系统目录"
	@echo "  uninstall        - 从系统目录卸载"
	@echo ""
	@echo "其他:"
	@echo "  help             - 显示此帮助信息"

# ================================ 依赖关系 ================================

# 自动生成依赖关系
DEPS = $(CORE_SOURCES:.cpp=.d)
-include $(DEPS)

$(SRC_DIR)/%.d: $(SRC_DIR)/%.cpp
	@$(CXX) $(CXXFLAGS) -MM $< -MT $(patsubst %.d,%.o,$@) > $@

.PHONY: all clean help run-test run-benchmark run-radix-test run-all debug memcheck cppcheck format install uninstall distclean