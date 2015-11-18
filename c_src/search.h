#ifndef __INDEXER_H
#define __INDEXER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IndexingMessage {
  char *path;
  char *i_message_id;
  char *i_subject;
  char *i_content;
  char *i_from;
  char *i_to;
  char *i_attachments;
} IndexingMessage;


void xapian_index_message(const char *index_path, IndexingMessage *pm);
char *xapian_search(const char *index_path, const char *query_str, const unsigned int max_results);

#ifdef __cplusplus
}
#endif
#endif