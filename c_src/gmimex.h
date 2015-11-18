void gmimex_init(void);
void gmimex_shutdown(void);

GString*    gmimex_get_json(gchar *path, gboolean include_content);
GByteArray* gmimex_get_part(gchar *path, guint part_id);

void gmimex_index_message(const gchar *mailbox_path, const gchar *message_path);
void gmimex_index_mailbox(const gchar *mailbox_path);
gchar **gmimex_search_mailbox(const gchar *mailbox_path, const gchar *query, const guint max_results);