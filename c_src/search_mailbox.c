#include <stdlib.h>
#include <glib/gprintf.h>
#include "gmimex.h"

int main(int argc, char *argv[]) {

  if (argc < 3) {
    g_printerr ("usage: %s <Mailbox-Path> \"<Query-String>\"\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  gmimex_init();

  gchar **results = gmimex_search_mailbox(argv[1], argv[2], 1000);
  if (results) {
    int i = 0;

    while (results[i])
      g_printf("%s\n", results[i++]);

    g_strfreev(results);
  }

  gmimex_shutdown();

  return 0;
}


