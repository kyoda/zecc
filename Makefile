CFLAGS=-std=gnu99 -g -fno-common -Wall -Wno-switch -static
# wildcard関数で、testディレクトリの.cを取得
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

TEST_SRCS=$(wildcard test/*.c)
# .cを.oに置き換える
TEST_OBJS=$(TEST_SRCS:.c=.o)
TEST_AS=$(TEST_SRCS:.c=.s)

zecc: $(OBJS)
		$(CC) ${CFLAGS} -o zecc $(OBJS) $(LDFLAGS)

$(OBJS): zecc.h

test/%.o: zecc test/%.c
# -E -P -C で、プリプロセスだけ処理
	$(CC) -o- -E -P -C test/$*.c | ./zecc -o test/$*.s -
# common は、%.cに含まれないように'.c'を外している
	$(CC) -o $@ test/$*.s -xc test/common

zecctest: $(TEST_OBJS)
# $^ は、依存関係（$(TEST_OBJS)）の全てのファイルを表す
# $$i としているのは、Makefileの変数ではなく、シェルの変数として扱うため
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done
	test/driver.sh
	test/test-error.sh zecc

# gccのテスト用
CCTEST_OBJS=$(TEST_SRCS:.c=.out)

test/%.out: test/%.c
	$(CC) ${CFLAGS} -o test/$*.out test/$*.c -xc test/common

cctest: $(CCTEST_OBJS)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done
	test/driver.sh
	test/test-error.sh gcc

test: cctest zecctest

clean:
	rm -f zecc *.o *~ tmp* $(CCTEST_OBJS) $(TEST_OBJS) $(TEST_AS)


# -----------------------------
# セルフコンパイルテスト
# -----------------------------
self: zecc
# 1. gcc で前処理だけする (-E)
	$(CC) -E -P $(SRCS) > zecc_pre.i

# 2. 自作コンパイラでアセンブリ生成
	./zecc -o tmp.s zecc_pre.i
	$(CC) -o zecc-self tmp.s

# 3. 出来た zecc-self でもう一度セルフビルド
	./zecc-self -o tmp2.s zecc_pre.i
	$(CC) -o zecc-self2 tmp2.s

	@echo "セルフコンパイル完了: zecc-self2 が生成されました"

.PHONY: zecc test zecctest cctest self clean

# この書き方だと、すべての .o ファイルがすべての .c ファイルに依存することになる
# $(OBJS): $(SRCS)