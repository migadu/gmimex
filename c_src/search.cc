#include <xapian.h>
#include <iostream>
#include <cstring>
#include "search.h"

extern "C" {

  void xapian_index_message(const char *index_path, IndexingMessage *pm) {
    try {
      Xapian::WritableDatabase database(index_path, Xapian::DB_CREATE_OR_OPEN);

      Xapian::Document doc;
      Xapian::TermGenerator indexer;
      Xapian::Stem stemmer("english");
      indexer.set_stemmer(stemmer);
      indexer.set_document(doc);

      // Unique ids: http://trac.xapian.org/wiki/FAQ/UniqueIds
      doc.set_data(pm->path);

      std::string id_term = "Q";
      id_term += pm->i_message_id;

      doc.add_term(id_term);

      indexer.index_text(pm->i_from, 1, "F");
      indexer.index_text(pm->i_to, 1, "T");

      if (pm->i_attachments)
        indexer.index_text(pm->i_attachments, 1, "A");

      if (pm->i_subject)
        indexer.index_text(pm->i_subject);

      if (pm->i_content)
        indexer.index_text(pm->i_content);

      database.replace_document(id_term, doc);
      database.commit();

    } catch (const Xapian::Error & error) {
      std::cout << "Exception: " << error.get_msg() << std::endl;
    }
  }


  char *xapian_search(const char *index_path, const char *query_str, const unsigned int max_results) {
    try {
      Xapian::Database db(index_path);
      Xapian::Enquire enquire(db);

      Xapian::QueryParser qp;
      Xapian::Stem stemmer("english");

      qp.set_stemmer(stemmer);
      qp.set_database(db);
      qp.set_stemming_strategy(Xapian::QueryParser::STEM_SOME);

      unsigned int flags = Xapian::QueryParser::FLAG_BOOLEAN        |
                         Xapian::QueryParser::FLAG_PHRASE           |
                         Xapian::QueryParser::FLAG_LOVEHATE         |
                         Xapian::QueryParser::FLAG_BOOLEAN_ANY_CASE |
                         Xapian::QueryParser::FLAG_WILDCARD         |
                         Xapian::QueryParser::FLAG_PURE_NOT;

      Xapian::Query query = qp.parse_query(query_str, flags);
      enquire.set_query(query);
      enquire.set_weighting_scheme (Xapian::BoolWeight());
      Xapian::MSet matches = enquire.get_mset(0, max_results);

      std::string results = "";
      int counter = 0;
      for (Xapian::MSetIterator i = matches.begin(); i != matches.end(); ++i, counter++) {
        if (counter > 0)
          results += "\n";
        results += i.get_document().get_data();
      }

      char *cstr = new char[results.length() + 1];
      std::strcpy(cstr, results.c_str());
      return cstr;
    } catch (const Xapian::Error & error) {
      std::cout << "Exception: " << error.get_msg() << std::endl;
      return NULL;
    }
  }

}