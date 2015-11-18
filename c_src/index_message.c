#include <stdlib.h>
#include <glib/gprintf.h>
#include "gmimex.h"

int main(int argc, char *argv[]) {

  if (argc < 3) {
    g_printerr ("usage: %s <Index-Path> <Mailbox-Path>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  gmimex_init();
  gmimex_index_message(argv[1], argv[2]);
  gmimex_shutdown();

  return 0;
}


