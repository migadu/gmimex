#include <stdlib.h>
#include <glib/gprintf.h>
#include "gmimex.h"

int main(int argc, char *argv[]) {

  if (argc < 2) {
    g_printerr ("usage: %s <Mailbox-Path>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  gmimex_init();
  gmimex_index_mailbox(argv[1]);
  gmimex_shutdown();

  return 0;
}


