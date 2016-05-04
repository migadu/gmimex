CC?=clang
NOOUT=2>&1 >/dev/null
PRIV_DIR=priv
C_SRC_DIR=c_src
ALL_LIBS=`pkg-config --cflags glib-2.0` `pkg-config --libs glib-2.0 gmime-2.6 gumbo`
CFLAGS=-O3 -fPIC -Wall `pkg-config --cflags glib-2.0 gmime-2.6 gumbo`

all: gmimex

gmimex: version check-c port

version:
	@cat VERSION

$(PRIV_DIR):
	@mkdir -p $@ $(NOOUT)

port: $(PRIV_DIR)
	$(CC) $(CFLAGS) -c $(C_SRC_DIR)/parson.c -o $(PRIV_DIR)/parson.o
	$(CC) $(CFLAGS) -c $(C_SRC_DIR)/gmimex.c -o $(PRIV_DIR)/gmimex.o
	$(CC) $(CFLAGS) -c $(C_SRC_DIR)/port.c -o $(PRIV_DIR)/port.o
	$(CC) $(CFLAGS) $(ALL_LIBS) $(PRIV_DIR)/parson.o $(PRIV_DIR)/gmimex.o $(PRIV_DIR)/port.o -o $(PRIV_DIR)/port

check-c:
	@hash clang 2>/dev/null || \
	hash gcc 2>/dev/null || ( \
	echo '`clang` or `gcc` seem not to be installed or in your PATH.' && \
	echo 'Maybe you need to install one of it first.' && \
	exit 1)

clean: clean-dir

clean-dir:
	rm -rf $(PRIV_DIR)
