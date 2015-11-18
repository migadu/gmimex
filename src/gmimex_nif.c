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
  gboolean include_content = TRUE;
  if (argc == 2 && enif_get_atom(env, argv[1], include_content_atom, sizeof(include_content_atom), ERL_NIF_LATIN1))
    include_content = !g_ascii_strcasecmp(include_content_atom, "true");

  GString *json_str = gmimex_get_json(path, include_content);
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
static ERL_NIF_TERM nif_index_mailbox(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  gchar *mailbox_path = get_char_argument(env, &argv[0]);
  if (!mailbox_path)
    return enif_make_badarg(env);

  gmimex_index_mailbox(mailbox_path);
  g_free(mailbox_path);

  return enif_make_atom(env, "ok");
}


/*
 *
 *
 */
static ERL_NIF_TERM nif_index_message(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  gchar *mailbox_path = get_char_argument(env, &argv[0]);
  if (!mailbox_path)
    return enif_make_badarg(env);

  gchar *message_path = get_char_argument(env, &argv[1]);
  if (!message_path)
    return enif_make_badarg(env);

  gmimex_index_message(mailbox_path, message_path);
  g_free(message_path);
  g_free(mailbox_path);

  return enif_make_atom(env, "ok");
}



/*
 *
 *
 */
static ERL_NIF_TERM nif_search_mailbox(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  gchar *mailbox_path = get_char_argument(env, &argv[0]);
  if (!mailbox_path)
    return enif_make_badarg(env);

  gchar *query_str = get_char_argument(env, &argv[1]);
  if (!query_str)
    return enif_make_badarg(env);

  int max_results;
  if(!enif_get_int(env, argv[2], &max_results)){
    g_free(mailbox_path);
    g_free(query_str);
    return enif_make_badarg(env);
  }

  gchar **results = gmimex_search_mailbox(mailbox_path, query_str, max_results);
  g_free(mailbox_path);
  g_free(query_str);

  if (!results)
    return enif_make_badarg(env);

  guint results_count = 0;
  while (results[results_count])
    results_count++;

  ERL_NIF_TERM *nif_arr = g_malloc(sizeof(ERL_NIF_TERM) * results_count);

  guint i;
  for (i = 0; i < results_count; i++) {
    ErlNifBinary bin = {0};
    GString *m_path = g_string_new(results[i]);
    enif_alloc_binary(m_path->len, &bin);
    (void)memcpy(bin.data, m_path->str, m_path->len);
    nif_arr[i] = enif_make_binary(env, &bin);
    g_string_free(m_path, TRUE);
  }

  g_strfreev(results);
  return enif_make_list_from_array(env, nif_arr, results_count);
}



/*
 *
 *
 */
static int load(ErlNifEnv* env, void** priv, ERL_NIF_TERM info) {
  gmimex_init();
  return 0;
}


/*
 *
 *
 */
static void unload(ErlNifEnv* env, void* priv) {
  gmimex_shutdown();
}


/*
 *
 *
 */
static ErlNifFunc nif_funcs[] = {
  { "get_json",       2, nif_get_json       },
  { "get_part",       2, nif_get_part       },
  { "index_mailbox",  1, nif_index_mailbox  },
  { "index_message",  2, nif_index_message  },
  { "search_mailbox", 3, nif_search_mailbox },
};


ERL_NIF_INIT(Elixir.GmimexNif, nif_funcs, &load, NULL, NULL, &unload);

