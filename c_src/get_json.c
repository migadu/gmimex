#include <stdlib.h>
#include <glib/gprintf.h>
#include "gmimex.h"

int main(int argc, char *argv[]) {

  if (argc < 2) {
    g_printerr ("usage: %s <MIME-Message-path>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  gmimex_init();

  GString *json_message = NULL;
  json_message = gmimex_get_json(argv[1], TRUE);
  if (!json_message)
    exit(EXIT_FAILURE);

  setbuf(stdout, NULL);
  g_printf("%s\n", json_message->str);
  g_string_free(json_message, TRUE);

  gmimex_shutdown();

  return 0;
}
