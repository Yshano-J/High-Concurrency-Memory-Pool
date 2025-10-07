# ================================ Makefile for High-Concurrency Memory Pool ================================
# �������ͱ���ѡ��
CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra -Iinclude
THREAD_FLAGS = -pthread

# Ŀ¼����
SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
BUILD_DIR = build
DOCS_DIR = docs

# Դ�ļ�
CORE_SOURCES = $(SRC_DIR)/ThreadCache.cpp $(SRC_DIR)/CentralCache.cpp $(SRC_DIR)/PageCache.cpp
HEADERS = $(INCLUDE_DIR)/Common.h $(INCLUDE_DIR)/ThreadCache.h $(INCLUDE_DIR)/CentralCache.h $(INCLUDE_DIR)/PageCache.h $(INCLUDE_DIR)/RadixTree.h $(INCLUDE_DIR)/ObjectPool.h $(INCLUDE_DIR)/ConcurrencyAlloc.h

# Ŀ���ļ�
TARGETS = $(BUILD_DIR)/test $(BUILD_DIR)/benchmark $(BUILD_DIR)/radix_test

# Ĭ��Ŀ��
.PHONY: all clean help run-test run-benchmark run-radix-test

all: $(TARGETS)

# ================================ ������� ================================

# ȷ��buildĿ¼����
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ���ܲ��Գ���
$(BUILD_DIR)/test: $(TEST_DIR)/Test.cpp $(CORE_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) $(TEST_DIR)/Test.cpp $(CORE_SOURCES) -o $@

# ���ܲ��Գ���
$(BUILD_DIR)/benchmark: $(TEST_DIR)/BenchMark.cpp $(CORE_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) $(TEST_DIR)/BenchMark.cpp $(CORE_SOURCES) -o $@

# ���������Գ���
$(BUILD_DIR)/radix_test: $(TEST_DIR)/RadixTreeTest.cpp $(INCLUDE_DIR)/RadixTree.h $(INCLUDE_DIR)/ObjectPool.h $(INCLUDE_DIR)/Common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(TEST_DIR)/RadixTreeTest.cpp -o $@

# �����Ա���
test: $(BUILD_DIR)/test
benchmark: $(BUILD_DIR)/benchmark
radix_test: $(BUILD_DIR)/radix_test

# ================================ ���й��� ================================

# ���й��ܲ���
run-test: $(BUILD_DIR)/test
	@echo "=== ���й��ܲ��� ==="
	./$(BUILD_DIR)/test

# �������ܲ���
run-benchmark: $(BUILD_DIR)/benchmark
	@echo "=== �������ܲ��� ==="
	./$(BUILD_DIR)/benchmark

# ���л���������
run-radix-test: $(BUILD_DIR)/radix_test
	@echo "=== ���л��������� ==="
	./$(BUILD_DIR)/radix_test

# �������в���
run-all: run-test run-benchmark run-radix-test

# ================================ ���԰汾 ================================

# ���԰汾����ѡ��
DEBUG_FLAGS = -std=c++11 -g -O0 -Wall -Wextra -DDEBUG

# ���԰汾Ŀ��
debug-test: $(TEST_DIR)/Test.cpp $(CORE_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(DEBUG_FLAGS) $(THREAD_FLAGS) $(TEST_DIR)/Test.cpp $(CORE_SOURCES) -o $(BUILD_DIR)/test-debug

debug-benchmark: $(TEST_DIR)/BenchMark.cpp $(CORE_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(DEBUG_FLAGS) $(THREAD_FLAGS) $(TEST_DIR)/BenchMark.cpp $(CORE_SOURCES) -o $(BUILD_DIR)/benchmark-debug

debug-radix: $(TEST_DIR)/RadixTreeTest.cpp $(INCLUDE_DIR)/RadixTree.h $(INCLUDE_DIR)/ObjectPool.h $(INCLUDE_DIR)/Common.h | $(BUILD_DIR)
	$(CXX) $(DEBUG_FLAGS) $(TEST_DIR)/RadixTreeTest.cpp -o $(BUILD_DIR)/radix_test-debug

debug: debug-test debug-benchmark debug-radix

# ================================ �ڴ��� ================================

# ʹ��Valgrind�����ڴ���
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes

memcheck-test: debug-test
	$(VALGRIND) ./$(BUILD_DIR)/test-debug

memcheck-benchmark: debug-benchmark
	$(VALGRIND) ./$(BUILD_DIR)/benchmark-debug

memcheck-radix: debug-radix
	$(VALGRIND) ./$(BUILD_DIR)/radix_test-debug

memcheck: memcheck-test memcheck-radix

# ================================ ���ܷ��� ================================

# ʹ��gprof�������ܷ���
PROFILE_FLAGS = -std=c++11 -O2 -pg

profile-benchmark: $(TEST_DIR)/BenchMark.cpp $(CORE_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(PROFILE_FLAGS) $(THREAD_FLAGS) $(TEST_DIR)/BenchMark.cpp $(CORE_SOURCES) -o $(BUILD_DIR)/benchmark-profile

run-profile: profile-benchmark
	./$(BUILD_DIR)/benchmark-profile
	gprof $(BUILD_DIR)/benchmark-profile gmon.out > $(BUILD_DIR)/profile_report.txt
	@echo "���ܷ�������������: $(BUILD_DIR)/profile_report.txt"

# ================================ ������ ================================

# ʹ��cppcheck���о�̬�������
cppcheck:
	@echo "=== ���о�̬������� ==="
	cppcheck --enable=all --std=c++11 --suppress=missingIncludeSystem -I$(INCLUDE_DIR) $(SRC_DIR)/*.cpp $(TEST_DIR)/*.cpp $(INCLUDE_DIR)/*.h

# �����ʽ��
format:
	@echo "=== ��ʽ������ ==="
	@if command -v clang-format >/dev/null 2>&1; then \
		clang-format -i $(SRC_DIR)/*.cpp $(TEST_DIR)/*.cpp $(INCLUDE_DIR)/*.h; \
		echo "�����ʽ�����"; \
	else \
		echo "δ�ҵ�clang-format���밲װ������"; \
	fi

# ================================ ������� ================================

# ����������ɵ��ļ�
clean:
	@echo "=== ��������ļ� ==="
	rm -rf $(BUILD_DIR)
	rm -f gmon.out
	rm -f *.o *.d
	@echo "�������"

# ����������������ļ��ȣ�
distclean: clean
	rm -f *~ *.bak
	rm -f core.*

# ================================ ��װ���� ================================

# ��װ��ϵͳĿ¼����ҪrootȨ�ޣ�
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
INCLUDEDIR = $(PREFIX)/include/memory-pool

install: all
	@echo "=== ��װ��ϵͳĿ¼ ==="
	mkdir -p $(BINDIR)
	mkdir -p $(INCLUDEDIR)
	cp $(BUILD_DIR)/benchmark $(BINDIR)/memory-pool-benchmark
	cp $(INCLUDE_DIR)/*.h $(INCLUDEDIR)/
	@echo "��װ��ɵ� $(PREFIX)"

uninstall:
	@echo "=== ж��ϵͳ�ļ� ==="
	rm -f $(BINDIR)/memory-pool-benchmark
	rm -rf $(INCLUDEDIR)
	@echo "ж�����"

# ================================ ������Ϣ ================================

help:
	@echo "High-Concurrency Memory Pool Makefile"
	@echo "======================================"
	@echo ""
	@echo "����Ŀ��:"
	@echo "  all              - �������г���"
	@echo "  test             - ���빦�ܲ��Գ���"
	@echo "  benchmark        - �������ܲ��Գ���"
	@echo "  radix_test       - ������������Գ���"
	@echo ""
	@echo "����Ŀ��:"
	@echo "  run-test         - ���й��ܲ���"
	@echo "  run-benchmark    - �������ܲ���"
	@echo "  run-radix-test   - ���л���������"
	@echo "  run-all          - �������в���"
	@echo ""
	@echo "����Ŀ��:"
	@echo "  debug            - ������԰汾"
	@echo "  debug-test       - ������԰湦�ܲ���"
	@echo "  debug-benchmark  - ������԰����ܲ���"
	@echo "  debug-radix      - ������԰����������"
	@echo ""
	@echo "�ڴ���:"
	@echo "  memcheck         - ʹ��Valgrind����ڴ�й©"
	@echo "  memcheck-test    - ��鹦�ܲ��Ե��ڴ�"
	@echo "  memcheck-radix   - �����������Ե��ڴ�"
	@echo ""
	@echo "���ܷ���:"
	@echo "  profile-benchmark - �������ܷ����汾"
	@echo "  run-profile      - �������ܷ��������ɱ���"
	@echo ""
	@echo "��������:"
	@echo "  cppcheck         - ���о�̬�������"
	@echo "  format           - ��ʽ������"
	@echo ""
	@echo "����Ŀ��:"
	@echo "  clean            - ��������ļ�"
	@echo "  distclean        - �������"
	@echo ""
	@echo "��װĿ��:"
	@echo "  install          - ��װ��ϵͳĿ¼"
	@echo "  uninstall        - ��ϵͳĿ¼ж��"
	@echo ""
	@echo "����:"
	@echo "  help             - ��ʾ�˰�����Ϣ"

# ================================ ������ϵ ================================

# �Զ�����������ϵ
DEPS = $(CORE_SOURCES:.cpp=.d)
-include $(DEPS)

$(SRC_DIR)/%.d: $(SRC_DIR)/%.cpp
	@$(CXX) $(CXXFLAGS) -MM $< -MT $(patsubst %.d,%.o,$@) > $@

.PHONY: all clean help run-test run-benchmark run-radix-test run-all debug memcheck cppcheck format install uninstall distclean