#include <erl_nif.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "../c_src/gmimex.h"


/*
 *
 *
 */
static char* get_char_argument(ErlNifEnv* env, const ERL_NIF_TERM *arg) {
  ErlNifBinary bin;

  if(!enif_inspect_binary(env, *arg, &bin))
    return NULL;

  gchar *result = g_strndup((const gchar *) bin.data, bin.size);
  enif_release_binary(&bin);

  return result;
}


/*
 *
 *
 */
static ERL_NIF_TERM nif_get_json(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {

  gchar *path = get_char_argument(env, &argv[0]);
  if (!path)
    return enif_make_badarg(env);

  gchar include_content_atom[10];
  guint content_option = 0;
  if (argc == 2 && enif_get_atom(env, argv[1], include_content_atom, sizeof(include_content_atom), ERL_NIF_LATIN1)) {
    if (!g_ascii_strcasecmp(include_content_atom, "true"))
      content_option = 1;
    else if (!g_ascii_strcasecmp(include_content_atom, "raw"))
      content_option = 2;
  }

  GString *json_str = gmimex_get_json(path, content_option);
  g_free(path);

  if (!json_str)
    return enif_make_badarg(env);

  ErlNifBinary result_binary = {0};
  enif_alloc_binary(json_str->len, &result_binary);
  (void)memcpy(result_binary.data, json_str->str, json_str->len);

  g_string_free(json_str, TRUE);

  return enif_make_binary(env, &result_binary);

}


/*
 *
 *
 */
static ERL_NIF_TERM nif_get_part(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {

  gchar *path = get_char_argument(env, &argv[0]);
  if (!path)
    return enif_make_badarg(env);

  int part_id;
  if(!enif_get_int(env, argv[1], &part_id)){
    g_free(path);
    return enif_make_badarg(env);
  }

  GByteArray *attachment = gmimex_get_part(path, part_id);
  g_free(path);

  if (!attachment)
    return enif_make_badarg(env);

  ErlNifBinary result_binary = {0};
  enif_alloc_binary(attachment->len, &result_binary);
  (void)memcpy(result_binary.data, attachment->data, attachment->len);
  g_byte_array_free(attachment, TRUE);

  return enif_make_binary(env, &result_binary);
}



/*
 *
 *
 */
static int load(ErlNifEnv* env, void** priv, ERL_NIF_TERM info) {
  return 0;
}


/*
 *
 *
 */
static void unload(ErlNifEnv* env, void* priv) {
}


/*
 *
 *
 */
static ErlNifFunc nif_funcs[] = {
  { "get_json",       2, nif_get_json       },
  { "get_part",       2, nif_get_part       }
};


ERL_NIF_INIT(Elixir.GmimexNif, nif_funcs, &load, NULL, NULL, &unload);

