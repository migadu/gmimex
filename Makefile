CC?=clang
NOOUT=2>&1 >/dev/null
PRIV_DIR=priv
C_SRC_DIR=c_src
NIF_SRC=src/gmimex_nif.c
NIF_LIB=$(PRIV_DIR)/gmimex.so
ERLANG_PATH=$(shell erl -eval 'io:format("~s", [lists:concat([code:root_dir(), "/erts-", erlang:system_info(version), "/include"])])' -s init stop -noshell)
ALL_LIBS=`pkg-config --cflags glib-2.0` `pkg-config --libs glib-2.0 gmime-2.6 gumbo`
NIF_COMPILE_OPTIONS=-I$(ERLANG_PATH) $(ALL_LIBS)
ifeq ($(shell uname),Darwin)
NIF_COMPILE_OPTIONS+= -dynamiclib -undefined dynamic_lookup
endif
CFLAGS=-O3 -fPIC -Wall `pkg-config --cflags glib-2.0 gmime-2.6 gumbo`

all: gmimex

gmimex: version check-c $(NIF_LIB)

version:
	@cat VERSION

$(PRIV_DIR):
	@mkdir -p $@ $(NOOUT)

$(NIF_LIB): $(PRIV_DIR)
	$(CC) $(CFLAGS) -c $(C_SRC_DIR)/parson.c -o $(PRIV_DIR)/parson.o
	$(CC) $(CFLAGS) -c $(C_SRC_DIR)/gmimex.c -o $(PRIV_DIR)/gmimex.o
	$(CC) $(CFLAGS) -c $(NIF_SRC) -o $(PRIV_DIR)/gmimex_nif.o -I$(ERLANG_PATH)
	$(CC) $(CFLAGS) $(NIF_COMPILE_OPTIONS) -shared $(PRIV_DIR)/parson.o $(PRIV_DIR)/gmimex.o $(PRIV_DIR)/gmimex_nif.o -o $@

	$(CC) $(CFLAGS) -c $(C_SRC_DIR)/get_json.c -o $(PRIV_DIR)/get_json.o
	$(CC) $(CFLAGS) -c $(C_SRC_DIR)/get_part.c -o $(PRIV_DIR)/get_part.o
	$(CC) $(CFLAGS) $(ALL_LIBS) $(PRIV_DIR)/parson.o $(PRIV_DIR)/gmimex.o $(PRIV_DIR)/get_json.o -o $(PRIV_DIR)/get_json
	$(CC) $(CFLAGS) $(ALL_LIBS) $(PRIV_DIR)/parson.o $(PRIV_DIR)/gmimex.o $(PRIV_DIR)/get_part.o -o $(PRIV_DIR)/get_part

check-c:
	@hash clang 2>/dev/null || \
	hash gcc 2>/dev/null || ( \
	echo '`clang` or `gcc` seem not to be installed or in your PATH.' && \
	echo 'Maybe you need to install one of it first.' && \
	exit 1)

clean: clean-objects clean-dirs

clean-objects:
	rm -f $(NIF_LIB)

clean-dirs:
	rm -rf $(PRIV_DIR)
