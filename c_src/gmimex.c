#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gmime/gmime.h>
#include <gumbo.h>

#include "parson.h"
#include "gmimex.h"

#define UTF8_CHARSET "UTF-8"
#define RECURSION_LIMIT 30
#define CITATION_COLOUR 4537548
#define MAX_CID_SIZE 65536
#define MIN_DATA_URI_IMAGE "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7"
#define VIEWPORT "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no\"/>"


#define COLLECT_RAW_CONTENT 2


/*
 * Address
 *
 * Address is a simple struct that represents an email address with
 * name and address. Name is not required but address is.
 *
 */
typedef struct Address {
  gchar *name;
  gchar *address;
} Address;


/*
 * AddressesList
 *
 * AddressesList is an array of Addresses. As such we can reuse GPtrArray.
 */

typedef GPtrArray AddressesList;


/*
 * MessageBody
 *
 * Structure to keep the body (text or html) within the MessageData with its
 * sanitized (HTML) content.
 *
 */
typedef struct MessageBody {
  gchar   *content_type;
  GString *content;
  guint   size;
} MessageBody;



/*
 * MessageAttachment
 *
 * Structure to keep the downloadable attachment information, without content.
 *
 */

typedef struct MessageAttachment {
  guint part_id;
  gchar *content_type;
  gchar *filename;
  guint size;
} MessageAttachment;


/*
 * MessageAttachmentsList
 *
 * A list of MessageAttachment, as a wrap around GPtrArray
 *
 */
typedef GPtrArray MessageAttachmentsList;


/*
 * MessageData
 *
 * Intermediate structure in which to keep the message data, already
 * cleaned up, sanitized and normalized; the bodies and attachments have been
 * already detected, and inline content has been injected.
 *
 */
typedef struct MessageData {
  gchar                  *message_id;
  AddressesList          *from;
  AddressesList          *reply_to;
  AddressesList          *to;
  AddressesList          *cc;
  AddressesList          *bcc;
  gchar                  *subject;
  gchar                  *date;
  GMimeReferences        *in_reply_to;
  GMimeReferences        *references;
  MessageBody            *text;
  MessageBody            *html;
  MessageAttachmentsList *attachments;
} MessageData;


/*
 * Utils
 */
static gboolean gc_contains_c(const gchar *str, const gchar c) {
  guint i;
  for(i = 0; i < strlen(str); i++)
    if (str[i] == c)
      return TRUE;
  return FALSE;
}

// The stripping functions from glib do not remove tabs, newlines etc.,
// so we define our owns that remove all whitespace.
static gchar *gc_lstrip(const gchar* text) {
  GRegex *regex = g_regex_new ("\\A\\s+", 0, 0, NULL);
  gchar *stripped = g_regex_replace_literal(regex, text, -1, 0, "", 0, NULL);
  g_regex_unref(regex);
  return stripped;
}


static gchar *gc_rstrip(const gchar* text) {
  GRegex *regex = g_regex_new ("\\s+$", 0, 0, NULL);
  gchar *stripped = g_regex_replace_literal(regex, text, -1, 0, "", 0, NULL);
  g_regex_unref(regex);
  return stripped;
}


static gchar *gc_strip(const gchar *text) {
  gchar *lstripped = gc_lstrip(text);
  gchar *stripped = gc_rstrip(lstripped);
  g_free(lstripped);
  return stripped;
}


static GString *gstr_strip(GString *text) {
  gchar *stripped_str = gc_strip(text->str);
  g_string_assign(text, stripped_str);
  g_free(stripped_str);
  return text;
}


static GString *gstr_replace_all(GString *text, const gchar* old_str, const gchar *new_str) {
  gchar *escaped_s1 = g_regex_escape_string (old_str, -1);
  GRegex *regex = g_regex_new (escaped_s1, 0, 0, NULL);
  gchar *new_string =  g_regex_replace_literal(regex, text->str, -1, 0, new_str, 0, NULL);
  g_regex_unref(regex);
  g_free(escaped_s1);
  g_string_assign(text, new_string);
  g_free(new_string);
  return text;
}


static GString *gstr_substitute_xml_entities_into_text(const gchar *text) {
  GString *result = g_string_new(text);
  gstr_replace_all(result, "&", "&amp;"); // replacing of & must come first
  gstr_replace_all(result, "<", "&lt;");
  gstr_replace_all(result, ">", "&gt;");
  return result;
}


static GString *gstr_substitute_xml_entities_into_attributes(const gchar quote, const gchar *text) {
  GString *result = gstr_substitute_xml_entities_into_text(text);
  if (quote == '"') {
    gstr_replace_all(result, "\"", "&quot;");
  } else if (quote == '\'') {
    gstr_replace_all(result, "'", "&apos;");
  }
  return result;
}



static Address *new_address(const gchar *address, const gchar *name) {
  g_return_val_if_fail(address != NULL, NULL);

  Address *addr = g_malloc(sizeof(Address));
  addr->name = NULL;
  addr->address = g_strdup(address);

  if (name)
    addr->name = g_strdup(name);

  return addr;
}


static void free_address(gpointer addr_ptr) {
  g_return_if_fail(addr_ptr != NULL);

  Address *addr = (Address *) addr_ptr;

  if (addr->name)
    g_free(addr->name);

  g_free(addr->address);
  g_free(addr);
}



static AddressesList *new_addresses_list(void) {
  return g_ptr_array_new_with_free_func((GDestroyNotify) free_address);
}


static void addresses_list_add(AddressesList *list, Address *address) {
  g_return_if_fail(list != NULL);
  g_ptr_array_add((GPtrArray *) list, address);
}


static Address *addresses_list_get(AddressesList *list, guint i) {
  g_return_val_if_fail(list != NULL, NULL);
  return (Address *) g_ptr_array_index((GPtrArray *) list, i);
}


static void free_addresses_list(AddressesList *addresses_list) {
  g_return_if_fail(addresses_list != NULL);
  g_ptr_array_free(addresses_list, TRUE);
}


static MessageBody *new_message_body(void) {
  MessageBody *mb = g_malloc(sizeof(MessageBody));
  mb->content_type = NULL;
  mb->content = NULL;
  mb->size = 0;
  return mb;
}


static void free_message_body(MessageBody *mbody) {
  g_return_if_fail(mbody != NULL);

  if (mbody->content_type)
    g_free(mbody->content_type);

  if (mbody->content)
    g_string_free(mbody->content, TRUE);

  g_free(mbody);
}



static MessageAttachment *new_message_attachment(guint part_id) {
  MessageAttachment *att = g_malloc(sizeof(MessageAttachment));
  att->part_id = part_id;
  att->content_type = NULL;
  att->filename = NULL;
  return att;
}


static void free_message_attachment(gpointer att) {
  g_return_if_fail(att != NULL);
  MessageAttachment *matt = (MessageAttachment *) att;
  g_free(matt->content_type);
  g_free(matt->filename);
  g_free(matt);
}


static MessageAttachmentsList *new_message_attachments_list(void) {
  return g_ptr_array_new_with_free_func((GDestroyNotify) free_message_attachment);
}


static void message_attachments_list_add(MessageAttachmentsList *att_list, MessageAttachment *att) {
  g_return_if_fail(att_list != NULL);
  g_ptr_array_add((GPtrArray *) att_list, att);
}


static MessageAttachment *message_attachments_list_get(MessageAttachmentsList *att_list, guint i) {
  g_return_val_if_fail(att_list != NULL, NULL);
  return (MessageAttachment *) g_ptr_array_index((GPtrArray *) att_list, i);
}


static void free_message_attachments_list(MessageAttachmentsList *att_list) {
  g_return_if_fail(att_list != NULL);
  g_ptr_array_free(att_list, TRUE);
}



static MessageData *new_message_data(void) {
  MessageData *mdata = g_malloc(sizeof(MessageData));
  mdata->message_id = NULL;
  mdata->from = NULL;
  mdata->reply_to = NULL;
  mdata->to = NULL;
  mdata->cc = NULL;
  mdata->bcc = NULL;
  mdata->subject = NULL;
  mdata->date = NULL;
  mdata->in_reply_to = NULL;
  mdata->references = NULL;
  mdata->text = NULL;
  mdata->html = NULL;
  mdata->attachments = NULL;
  return mdata;
}


static void free_message_data(MessageData *mdata) {
  g_return_if_fail(mdata != NULL);

  if (mdata->message_id)
    g_free(mdata->message_id);

  if (mdata->from)
    free_addresses_list(mdata->from);

  if (mdata->reply_to)
    free_addresses_list(mdata->reply_to);

  if (mdata->to)
    free_addresses_list(mdata->to);

  if (mdata->cc)
    free_addresses_list(mdata->cc);

  if (mdata->bcc)
    free_addresses_list(mdata->bcc);

  if (mdata->subject)
    g_free(mdata->subject);

  if (mdata->date)
    g_free(mdata->date);

  if (mdata->in_reply_to)
     g_mime_references_free(mdata->in_reply_to);

  if (mdata->references)
     g_mime_references_free(mdata->references);

  if (mdata->text)
    free_message_body(mdata->text);

  if (mdata->html)
    free_message_body(mdata->html);

  if (mdata->attachments)
    free_message_attachments_list(mdata->attachments);

  g_free(mdata);
}



/*
 * CollectedPart
 */
typedef struct CollectedPart {
  guint      part_id;        // the depth within the message where this part is located
  gchar      *content_type;  // content type (text/html, text/plan etc.)
  GByteArray *content;       // content data
  gchar      *content_id;    // for inline content
  gchar      *filename;      // for attachments, inlines and body parts that define filename
  gchar      *disposition;   // for attachments and inlines
} CollectedPart;


/*
 * CollectedPart
 *
 * When iterating over parts of the message, each one is kept in this structure,
 * whether a body part, alternative content, inline or attachment.
 *
 * All parts together are in PartCollectorData.
 */
static CollectedPart* new_collected_part(guint part_id) {
  CollectedPart *part = g_malloc(sizeof(CollectedPart));

  part->part_id      = part_id;
  part->content_type = NULL;
  part->content      = NULL;
  part->content_id   = NULL;
  part->filename     = NULL;
  part->disposition  = NULL;

  return part;
}


static void free_collected_part(gpointer part) {
  g_return_if_fail(part != NULL);

  CollectedPart *cpart = (CollectedPart *) part;

  if (cpart->content_type)
    g_free(cpart->content_type);

  if (cpart->content)
     g_byte_array_free(cpart->content, TRUE);

  if (cpart->content_id)
    g_free(cpart->content_id);

  if (cpart->filename)
    g_free(cpart->filename);

  if (cpart->disposition)
    g_free(cpart->disposition);

  g_free(cpart);
}


/*
 * PartCollectorData
 *
 * All parts of the message, organized and divided. This is "almost" a message
 * data but not yet sanitized, normalized etc. It's simply raw data as is in the
 * message, but parts of interest are organized for further processing.
 *
 */
typedef struct PartCollectorData {
  gboolean      raw;
  guint         recursion_depth;  // We keep track of explicit recursions, and limit them (RECURSION_LIMIT)
  guint         part_id;          // We keep track of the depth within message parts to identify parts later
  CollectedPart *html_part;
  CollectedPart *text_part;

  GPtrArray     *alternative_bodies;  // of CollectedParts
  GPtrArray     *inlines;             // of CollectedParts
  GPtrArray     *attachments;         // of CollectedParts
} PartCollectorData;


static PartCollectorData* new_part_collector_data(guint content_option) {
  PartCollectorData *pcd = g_malloc(sizeof(PartCollectorData));

  pcd->raw = (content_option == COLLECT_RAW_CONTENT);

  pcd->recursion_depth = 0;
  pcd->part_id         = 0;

  pcd->text_part = NULL;
  pcd->html_part = NULL;

  pcd->alternative_bodies = g_ptr_array_new_with_free_func((GDestroyNotify) free_collected_part);
  pcd->attachments        = g_ptr_array_new_with_free_func((GDestroyNotify) free_collected_part);
  pcd->inlines            = g_ptr_array_new_with_free_func((GDestroyNotify) free_collected_part);

  return pcd;
}


static void free_part_collector_data(PartCollectorData *pcdata) {
  g_return_if_fail(pcdata != NULL);

  if (pcdata->html_part)
    free_collected_part(pcdata->html_part);

  if (pcdata->text_part)
    free_collected_part(pcdata->text_part);

  if (pcdata->alternative_bodies)
    g_ptr_array_free(pcdata->alternative_bodies, TRUE);

  if (pcdata->inlines)
    g_ptr_array_free(pcdata->inlines, TRUE);

  if (pcdata->attachments)
    g_ptr_array_free(pcdata->attachments, TRUE);

  g_free(pcdata);
}


/*
 * PartExtractorData
 *
 * We use part extractor data when we need to find a specific part within the
 * message. The part_id argument is the part number we want extracted.
 */
typedef struct PartExtractorData {
  guint      recursion_depth;
  guint      part_id;
  gchar      *content_type;
  GByteArray *content;
} PartExtractorData;


static PartExtractorData *new_part_extractor_data(guint part_id) {
  PartExtractorData *ped = g_malloc(sizeof(PartExtractorData));
  ped->recursion_depth = 0;
  ped->part_id = part_id;
  ped->content_type = NULL;
  ped->content = NULL;
  return ped;
}


static void free_part_extractor_data(PartExtractorData *ped, gboolean release_content) {
  g_return_if_fail(ped != NULL);

  if (ped->content_type)
    g_free(ped->content_type);

  if (ped->content && release_content)
    g_byte_array_free(ped->content, TRUE);

  g_free(ped);
}


/*
 * SANITIZER
 *
 */


static gchar* permitted_tags            = "|a|abbr|acronym|address|area|b|bdo|body|big|blockquote|br|button|caption|center|cite|code|col|colgroup|dd|del|dfn|dir|div|dl|dt|em|fieldset|font|form|h1|h2|h3|h4|h5|h6|hr|i|img|input|ins|kbd|label|legend|li|map|menu|ol|optgroup|option|p|pre|q|s|samp|select|small|span|strike|strong|sub|sup|table|tbody|td|textarea|tfoot|th|thead|u|tr|tt|u|ul|var|";
static gchar* permitted_attributes      = "|href|src|action|style|color|bgcolor|width|height|colspan|rowspan|cellspacing|cellpadding|border|align|valign|dir|type|";
static gchar* protocol_attributes       = "|href|src|action|";
static gchar* protocol_separators_regex = ":|(&#0*58)|(&#x70)|(&#x0*3a)|(%|&#37;)3A";
static gchar* permitted_protocols       = "||ftp|http|https|cid|data|irc|mailto|news|gopher|nntp|telnet|webcal|xmpp|callto|feed|";
static gchar* empty_tags                = "|area|br|col|hr|img|input|";
static gchar* special_handling          = "|html|body|";
static gchar* no_entity_sub             = "|pre|";

// Forward declaration
static GString* sanitize(GumboNode* node, GPtrArray* inlines_ary);


static GString *handle_unknown_tag(GumboStringPiece *text) {
  if (text->data == NULL)
    return g_string_new(NULL);

  // work with copy GumboStringPiece to prevent asserts
  // if try to read same unknown tag name more than once
  GumboStringPiece gsp = *text;
  gumbo_tag_from_original_text(&gsp);
  return g_string_new_len(gsp.data, gsp.length);
}


static GString *get_tag_name(GumboNode *node) {
  // work around lack of proper name for document node
  if (node->type == GUMBO_NODE_DOCUMENT)
    return g_string_new("document");

  const gchar *n_tagname = gumbo_normalized_tagname(node->v.element.tag);
  GString *tagname = g_string_new(n_tagname);

  if (!tagname->len) {
    g_string_free(tagname, TRUE);
    return handle_unknown_tag(&node->v.element.original_tag);
  }

  return tagname;
}


static GString *build_attributes(GumboNode* node, GumboAttribute *at, gboolean no_entities, GPtrArray *inlines_ary) {
  gchar *key = g_strjoin(NULL, "|", at->name, "|", NULL);
  gchar *key_pattern = g_regex_escape_string(key, -1);
  g_free(key);

  gboolean is_permitted_attribute = g_regex_match_simple(key_pattern, permitted_attributes, G_REGEX_CASELESS, 0);
  gboolean is_protocol_attribute  = g_regex_match_simple(key_pattern, protocol_attributes, G_REGEX_CASELESS, 0);
  gchar *cid_content_id = NULL;

  g_free(key_pattern);

  if (!is_permitted_attribute)
    return g_string_new(NULL);

  GString *attr_value = g_string_new(at->value);
  gstr_strip(attr_value);

  if (is_protocol_attribute) {
    gchar **protocol_parts = g_regex_split_simple(protocol_separators_regex, attr_value->str, G_REGEX_CASELESS, 0);
    guint pparts_length = 0;

    while (protocol_parts[pparts_length])
      pparts_length++;

    gboolean is_permitted_protocol = FALSE;

    if (pparts_length) {
      static gchar* protocol_join_str = ":";
      gchar* new_joined = g_strjoinv(protocol_join_str, protocol_parts);
      g_string_assign(attr_value, new_joined);
      g_free(new_joined);

      gchar *attr_protocol = g_strjoin(NULL, "|", protocol_parts[0], "|", NULL);
      gchar *attr_prot_pattern = g_regex_escape_string(attr_protocol, -1);
      g_free(attr_protocol);
      is_permitted_protocol = g_regex_match_simple(attr_prot_pattern, permitted_protocols, G_REGEX_CASELESS, 0);
      g_free(attr_prot_pattern);

      if (is_permitted_protocol && !g_ascii_strcasecmp(protocol_parts[0], "cid"))
        cid_content_id = g_strdup(protocol_parts[1]);
    }
    g_strfreev(protocol_parts);

    if (!is_permitted_protocol) {
      g_string_free(attr_value, TRUE);
      return g_string_new(NULL);
    }
  }

  gboolean cid_replaced = FALSE;
  if (cid_content_id) {
    if (inlines_ary && inlines_ary->len) {
      guint i;
      for (i = 0; i < inlines_ary->len; i++) {
        CollectedPart *inline_body = g_ptr_array_index(inlines_ary, i);
        if (inline_body->content_id && !g_ascii_strcasecmp(inline_body->content_id, cid_content_id)) {
          if (inline_body->content->len < MAX_CID_SIZE) {
            gchar *base64_data = g_base64_encode((const guchar *) inline_body->content->data, inline_body->content->len);
            gchar *new_attr_value = g_strjoin(NULL, "data:", inline_body->content_type, ";base64,", base64_data, NULL);
            g_string_assign(attr_value, new_attr_value);
            g_free(base64_data);
            g_free(new_attr_value);
            cid_replaced = TRUE;
          }
        }
      }
    }

    // `cid` is not a valid URI schema, so if it was not replaced by the inline content,
    // we replace it with a 1x1 image which should hide it. If there is content and we missed
    // it due to the wrong contentId given, it will be avaialable as a downloadable attachment.
    if (!cid_replaced)
      g_string_assign(attr_value, MIN_DATA_URI_IMAGE);

    g_free(cid_content_id);
  }

  GString *atts = g_string_new(" ");

  if (node->type == GUMBO_NODE_ELEMENT)
    if (((node->v.element.tag == GUMBO_TAG_IMG) &&
          !g_ascii_strcasecmp(at->name, "src")) ||
        (!g_ascii_strcasecmp(at->name, "style") &&
          g_regex_match_simple("url", attr_value->str, G_REGEX_CASELESS, 0)))
      g_string_append(atts, "data-proxy-");

  g_string_append(atts, at->name);

  // how do we want to handle attributes with empty values
  // <input type="checkbox" checked />  or <input type="checkbox" checked="" />

  gchar quote = at->original_value.data[0];

  if (attr_value->len || (quote == '"') || (quote == '\'')) {

    gchar *qs = "";
    if (quote == '\'')
      qs = "'";

    if (quote == '"')
      qs = "\"";

    g_string_append(atts, "=");
    g_string_append(atts, qs);

    if (no_entities) {
      g_string_append(atts, attr_value->str);
    } else {
      GString *subd = gstr_substitute_xml_entities_into_attributes(quote, attr_value->str);
      g_string_append(atts, subd->str);
      g_string_free(subd, TRUE);
    }
    g_string_append(atts, qs);
  }

  g_string_free(attr_value, TRUE);
  return atts;
}



static GString *sanitize_contents(GumboNode* node, GPtrArray *inlines_ary) {
  GString *contents = g_string_new(NULL);
  GString *tagname  = get_tag_name(node);

  gchar *key = g_strjoin(NULL, "|", tagname->str, "|", NULL);
  g_string_free(tagname, TRUE);

  // Since we include pipes (|) we have to escape the regex string
  gchar *key_pattern = g_regex_escape_string(key, -1);
  g_free(key);

  gboolean no_entity_substitution = g_regex_match_simple(key_pattern, no_entity_sub, G_REGEX_CASELESS, 0);
  g_free(key_pattern);

  // build up result for each child, recursively if need be
  GumboVector* children = &node->v.element.children;
  guint i;
  for (i = 0; i < children->length; ++i) {
    GumboNode* child = (GumboNode*) (children->data[i]);

    if (child->type == GUMBO_NODE_TEXT) {
      if (no_entity_substitution) {
        g_string_append(contents, child->v.text.text);
      } else {
        GString *subd = gstr_substitute_xml_entities_into_text(child->v.text.text);
        g_string_append(contents, subd->str);
        g_string_free(subd, TRUE);
      }

    } else if (child->type == GUMBO_NODE_ELEMENT ||
               child->type == GUMBO_NODE_TEMPLATE) {

      GString *child_ser = sanitize(child, inlines_ary);
      g_string_append(contents, child_ser->str);
      g_string_free(child_ser, TRUE);

    } else if (child->type == GUMBO_NODE_WHITESPACE) {
      // keep all whitespace to keep as close to original as possible
      g_string_append(contents, child->v.text.text);
    } else if (child->type != GUMBO_NODE_COMMENT) {
      // Does this actually exist: (child->type == GUMBO_NODE_CDATA)
      fprintf(stderr, "unknown element of type: %d\n", child->type);
    }
  }
  return contents;
}


static GString *sanitize(GumboNode* node, GPtrArray* inlines_ary) {
  // special case the document node
  if (node->type == GUMBO_NODE_DOCUMENT) {
    GString *results = g_string_new("<!DOCTYPE html>\n");
    GString *node_ser = sanitize_contents(node, inlines_ary);
    g_string_append(results, node_ser->str);
    g_string_free(node_ser, TRUE);
    return results;
  }

  if ((node->type == GUMBO_NODE_ELEMENT) &&
      (node->v.element.tag == GUMBO_TAG_HEAD)) {
    GString *results = g_string_new("<head>\n");
    g_string_append(results, VIEWPORT);
    g_string_append(results, "</head>\n");
    return results;
  }

  GString *tagname = get_tag_name(node);
  gchar *key = g_strjoin(NULL, "|", tagname->str, "|", NULL);

  gchar *key_pattern = g_regex_escape_string(key, -1);
  g_free(key);


  gboolean need_special_handling     = g_regex_match_simple(key_pattern, special_handling,   G_REGEX_CASELESS, 0);
  gboolean is_empty_tag              = g_regex_match_simple(key_pattern, empty_tags,         G_REGEX_CASELESS, 0);
  gboolean no_entity_substitution    = g_regex_match_simple(key_pattern, no_entity_sub,      G_REGEX_CASELESS, 0);
  gboolean tag_permitted             = g_regex_match_simple(key_pattern, permitted_tags,     G_REGEX_CASELESS, 0);

  g_free(key_pattern);

  if (!need_special_handling && !tag_permitted) {
    g_string_free(tagname, TRUE);
    return g_string_new(NULL);
  }

  GString *close = g_string_new(NULL);
  GString *closeTag = g_string_new(NULL);
  GString *atts = g_string_new(NULL);

  const GumboVector *attribs = &node->v.element.attributes;
  guint i;
  for (i = 0; i < attribs->length; ++i) {
    GumboAttribute* at = (GumboAttribute*)(attribs->data[i]);
    GString *attsstr = build_attributes(node, at, no_entity_substitution, inlines_ary);
    g_string_append(atts, attsstr->str);
    g_string_free(attsstr, TRUE);
  }

  if (is_empty_tag) {
    g_string_append_c(close, '/');
  } else {
    g_string_append_printf(closeTag, "</%s>", tagname->str);
  }

  GString *contents = sanitize_contents(node, inlines_ary);

  if (need_special_handling) {
    gstr_strip(contents);
    g_string_append_c(contents, '\n');
  }

  GString *results = g_string_new(NULL);
  g_string_append_printf(results, "<%s%s%s>", tagname->str, atts->str, close->str);
  g_string_free(atts, TRUE);

  g_string_free(tagname, TRUE);

  if (need_special_handling)
    g_string_append_c(results, '\n');

  g_string_append(results, contents->str);
  g_string_free(contents, TRUE);

  g_string_append(results, closeTag->str);

  if (need_special_handling)
    g_string_append_c(results, '\n');

  g_string_free(close, TRUE);
  g_string_free(closeTag, TRUE);

  return results;
}




/*
 * Textizer -> fetches text content out of HTML
 *
static GString *textize(const GumboNode* node) {
  if (node->type == GUMBO_NODE_TEXT) {
    return g_string_new(node->v.text.text);
  } else if (node->type == GUMBO_NODE_ELEMENT &&
             node->v.element.tag != GUMBO_TAG_SCRIPT &&
             node->v.element.tag != GUMBO_TAG_STYLE) {
    const GumboVector* children = &node->v.element.children;
    GString *contents = g_string_new(NULL);
    guint i;
    for (i = 0; i < children->length; ++i) {
      GString *text = textize((GumboNode*) children->data[i]);
      gstr_strip(text);
      if (i && text->len && contents->len)
        g_string_append_c(contents, ' ');
      g_string_append(contents, text->str);
      g_string_free(text, TRUE);
    }
    return contents;
  } else {
    return g_string_new(NULL);
  }
}
*/


/*
 *
 *
 */
static GMimeMessage* gmime_message_from_stream(GMimeStream *stream) {
  g_return_val_if_fail(stream != NULL, NULL);

  GMimeParser *parser = g_mime_parser_new_with_stream(stream);
  if (!parser) {
    g_printerr("failed to create parser\r\n");
    return NULL;
  }

  GMimeMessage *message = g_mime_parser_construct_message(parser);
  g_object_unref (parser);
  if (!message) {
    g_printerr("failed to construct message\r\n");
    return NULL;
  }

  return message;
}


/*
 *
 *
 */
static GMimeMessage *gmime_message_from_file(FILE *file) {
  g_return_val_if_fail(file != NULL, NULL);

  GMimeStream *stream = g_mime_stream_file_new(file);

  if (!stream) {
    g_printerr("file stream could not be opened\r\n");
    fclose(file);
    return NULL;
  }

  // Being owner of the stream will automatically close the file when released
  g_mime_stream_file_set_owner(GMIME_STREAM_FILE(stream), TRUE);

  GMimeMessage *message = gmime_message_from_stream(stream);
  g_object_unref (stream);
  if (!message) {
    g_printerr("message could not be constructed from stream\r\n");
    return NULL;
  }

  return message;
}


/*
 *
 *
 */
static GMimeMessage *gmime_message_from_path(const gchar *path) {
  g_return_val_if_fail(path != NULL, NULL);

  // Note: we don't need to worry about closing the file, as it will be closed by the
  // stream within message_from_file.
  FILE *file = fopen (path, "r");

  if (!file) {
    g_printerr("cannot open file '%s': %s\r\n", path, g_strerror(errno));
    return NULL;
  }

  GMimeMessage *message = gmime_message_from_file(file);
  if (!message) {
    g_printerr("message could not be constructed from file '%s': %s\r\n", path, g_strerror(errno));
    return NULL;
  }

  return message;
}



/*
 *
 *
 */
static void collect_part(GMimeObject *part, PartCollectorData *fdata, gboolean multipart_parent) {
  GMimeContentType        *content_type = g_mime_object_get_content_type(part);
  GMimeContentDisposition *disposition  = g_mime_object_get_content_disposition(part);

  if (!content_type)
    return;

  GMimeDataWrapper *wrapper = g_mime_part_get_content_object(GMIME_PART(part));
  if (!wrapper)
    return;

  // All the information will be collected in the CollectedPart
  CollectedPart *c_part = new_collected_part(fdata->part_id);

  gboolean is_attachment = FALSE;
  if (disposition) {
    c_part->disposition = g_ascii_strdown(disposition->disposition, -1);
    is_attachment = !g_ascii_strcasecmp(disposition->disposition, GMIME_DISPOSITION_ATTACHMENT);
  }

  // If a filename is given, collect it always
  const gchar *filename = g_mime_part_get_filename(GMIME_PART(part));
  if (filename)
    c_part->filename = g_strdup(filename);

  // If a contentID is given, collect it always
  const char* content_id = g_mime_part_get_content_id (GMIME_PART(part));
  if (content_id)
    c_part->content_id = g_strdup(content_id);

  // Get the contentType in lowercase
  gchar *content_type_str = g_mime_content_type_to_string(content_type);
  c_part->content_type = g_ascii_strdown(content_type_str, -1);
  g_free(content_type_str);

  // To qualify as a message body, a MIME entity MUST NOT have a Content-Disposition header with the value "attachment".
  if (!is_attachment && g_mime_content_type_is_type(content_type, "text", "*")) {
    gboolean is_text_plain    = g_mime_content_type_is_type(content_type, "text", "plain");
    gboolean is_text_html     = g_mime_content_type_is_type(content_type, "text", "html");
    gboolean is_text_rtf      = g_mime_content_type_is_type(content_type, "text", "rtf");
    gboolean is_text_enriched = g_mime_content_type_is_type(content_type, "text", "enriched");

    gboolean is_new_text = !fdata->text_part && is_text_plain;
    gboolean is_new_html = !fdata->html_part && (is_text_html || is_text_enriched || is_text_rtf);

    GMimeStream *mem_stream = g_mime_stream_mem_new();
    g_mime_stream_mem_set_owner(GMIME_STREAM_MEM(mem_stream), FALSE);
    GMimeStream *filtered_mem_stream = g_mime_stream_filter_new(mem_stream);

    const gchar *charset = g_mime_object_get_content_type_parameter(part, "charset");
    if (charset && g_ascii_strcasecmp(charset, UTF8_CHARSET)) {
      GMimeFilter *utf8_charset_filter = g_mime_filter_charset_new(charset, UTF8_CHARSET);
      g_mime_stream_filter_add(GMIME_STREAM_FILTER(filtered_mem_stream), utf8_charset_filter);
      g_object_unref(utf8_charset_filter);
    }

    if (!fdata->raw && is_new_text) {
      GMimeFilter *strip_filter = g_mime_filter_strip_new();
      g_mime_stream_filter_add(GMIME_STREAM_FILTER(filtered_mem_stream), strip_filter);
      g_object_unref(strip_filter);

      GMimeFilter *crlf_filter = g_mime_filter_crlf_new(FALSE, FALSE);
      g_mime_stream_filter_add(GMIME_STREAM_FILTER(filtered_mem_stream), crlf_filter);
      g_object_unref(crlf_filter);

      GMimeFilter *html_filter = g_mime_filter_html_new(
         GMIME_FILTER_HTML_CONVERT_NL        |
         GMIME_FILTER_HTML_CONVERT_SPACES    |
         GMIME_FILTER_HTML_CONVERT_URLS      |
         GMIME_FILTER_HTML_MARK_CITATION     |
         GMIME_FILTER_HTML_CONVERT_ADDRESSES |
         GMIME_FILTER_HTML_CITE, CITATION_COLOUR);
      g_mime_stream_filter_add(GMIME_STREAM_FILTER(filtered_mem_stream), html_filter);
      g_object_unref(html_filter);
    }

    if (!fdata->raw && (is_new_text || is_new_html)) {
      GMimeFilter *from_filter = g_mime_filter_from_new(GMIME_FILTER_FROM_MODE_ESCAPE);
      g_mime_stream_filter_add(GMIME_STREAM_FILTER(filtered_mem_stream), from_filter);
      g_object_unref(from_filter);
    }

    // Add Enriched/RTF filter for this content
    if (!fdata->raw && (is_new_html && (is_text_enriched || is_text_rtf))) {
      guint flags = 0;
      if (is_text_rtf)
        flags = GMIME_FILTER_ENRICHED_IS_RICHTEXT;

      GMimeFilter *enriched_filter = g_mime_filter_enriched_new(flags);
      g_mime_stream_filter_add(GMIME_STREAM_FILTER(filtered_mem_stream), enriched_filter);
      g_object_unref(enriched_filter);
    }
    g_mime_data_wrapper_write_to_stream(wrapper, filtered_mem_stream);

    // Very important! Flush the the stream and get all content through.
    g_mime_stream_flush(filtered_mem_stream);

    // Freed by the mem_stream on its own (owner) [transfer none]
    c_part->content = g_mime_stream_mem_get_byte_array(GMIME_STREAM_MEM(mem_stream));

    // After we unref the mem_stream, part_content is NOT available anymore
    g_object_unref(filtered_mem_stream);
    g_object_unref(mem_stream);

    // Without content, the collected body part is of no use, so we ignore it.
    if (c_part->content->len == 0) {
      free_collected_part(c_part);
      return;
    }

    // We accept only the first text and first html content, everything
    // else is considered an alternative body
    if (is_new_text) {
      fdata->text_part = c_part;
    } else if (is_new_html) {
      fdata->html_part = c_part;
    } else {
      g_ptr_array_add(fdata->alternative_bodies, c_part);
    }

  } else {
    GMimeStream *attachment_mem_stream = g_mime_stream_mem_new();
    g_mime_stream_mem_set_owner (GMIME_STREAM_MEM(attachment_mem_stream), FALSE);
    g_mime_data_wrapper_write_to_stream(wrapper, attachment_mem_stream);
    g_mime_stream_flush(attachment_mem_stream);

    c_part->content = g_mime_stream_mem_get_byte_array(GMIME_STREAM_MEM(attachment_mem_stream));
    g_object_unref(attachment_mem_stream);

    // Some content may not have disposition defined so we need to determine better what it is
    if ((disposition && !g_ascii_strcasecmp(disposition->disposition, GMIME_DISPOSITION_INLINE)) ||
        g_mime_part_get_content_id(GMIME_PART(part))) {
      g_ptr_array_add(fdata->inlines, c_part);
    } else {
      // All other disposition should be kept within attachments
      g_ptr_array_add(fdata->attachments, c_part);
    }

  }
}


/*
 *
 *
 */
static void collector_foreach_callback(GMimeObject *parent, GMimeObject *part, gpointer user_data) {
  g_return_if_fail(user_data != NULL);

  PartCollectorData *fdata = (PartCollectorData *) user_data;

  if (GMIME_IS_MESSAGE_PART(part)) {

    if (fdata->recursion_depth++ < RECURSION_LIMIT) {
      GMimeMessage *message = g_mime_message_part_get_message((GMimeMessagePart *) part); // transfer none
      if (message)
        g_mime_message_foreach(message, collector_foreach_callback, user_data);

    } else {
      g_printerr("endless recursion detected: %d\r\n", fdata->recursion_depth);
      return;
    }

  } else if (GMIME_IS_MESSAGE_PARTIAL(part)) {
    // Save into an array ? Todo: Look into the specs
  } else if (GMIME_IS_MULTIPART(part)) {
    // Nothing special needed on multipart, let descend further
  } else if (GMIME_IS_PART(part)) {
    collect_part(part, fdata, GMIME_IS_MULTIPART(parent));
    fdata->part_id++;
  } else {
    g_assert_not_reached();
  }
}


static PartCollectorData *collect_parts(GMimeMessage *message, guint content_option) {
  PartCollectorData *pc = new_part_collector_data(content_option);
  g_mime_message_foreach(message, collector_foreach_callback, pc);
  return pc;
}




/*
 *
 *
 */
static void extract_part(GMimeObject *part, PartExtractorData *a_data) {
  GMimeDataWrapper *attachment_wrapper = g_mime_part_get_content_object(GMIME_PART(part));
  GMimeStream *attachment_mem_stream = g_mime_stream_mem_new();
  g_mime_stream_mem_set_owner(GMIME_STREAM_MEM(attachment_mem_stream), FALSE);
  g_mime_data_wrapper_write_to_stream(attachment_wrapper, attachment_mem_stream);
  g_mime_stream_flush(attachment_mem_stream);
  a_data->content = g_mime_stream_mem_get_byte_array(GMIME_STREAM_MEM(attachment_mem_stream));
  g_object_unref(attachment_mem_stream);
}


/*
 *
 *
 */
static void part_extractor_foreach_callback(GMimeObject *parent, GMimeObject *part, gpointer user_data) {
  PartExtractorData *a_data = (PartExtractorData *) user_data;

  if (GMIME_IS_MESSAGE_PART(part)) {

    if (a_data->recursion_depth < RECURSION_LIMIT) {
      GMimeMessage *message = g_mime_message_part_get_message((GMimeMessagePart *) part); // transfer none
      if (message)
        g_mime_message_foreach(message, part_extractor_foreach_callback, a_data);

    } else {
      g_printerr("endless recursion detected: %d\r\n", a_data->recursion_depth);
      return;
    }

  } else if (GMIME_IS_MESSAGE_PARTIAL (part)) {
    // Save into an array ? Todo: Look into the specs
  } else if (GMIME_IS_MULTIPART (part)) {
    // Nothing special needed on multipart, let descend further
  } else if (GMIME_IS_PART (part)) {

    // We are interested only in the part 0 (counting down by same logic)
    if (a_data->part_id == 0)
      extract_part(part, a_data);

    a_data->part_id--;

  } else {
    g_assert_not_reached();
  }
}


/*
 *
 *
 */
static GByteArray *gmime_message_get_part_data(GMimeMessage* message, guint part_id) {
  g_return_val_if_fail(message != NULL, NULL);

  PartExtractorData *a_data = new_part_extractor_data(part_id);
  g_mime_message_foreach(message, part_extractor_foreach_callback, a_data);
  GByteArray *content = a_data->content;
  free_part_extractor_data(a_data, FALSE);

  if (!content)
    g_printerr("could not locate partId %d\r\n", part_id);

  return content;
}





/*
 *
 *
 */
static void collect_addresses_into(InternetAddressList *ilist, AddressesList *addr_list, guint size) {
  g_return_if_fail(ilist != NULL);
  g_return_if_fail(addr_list != NULL);
  g_return_if_fail(size != 0);

  guint i;
  for (i = 0; i < size; i++) {
    InternetAddress *address = internet_address_list_get_address(ilist, i); // transfer none

    if (INTERNET_ADDRESS_IS_GROUP(address)) {
      InternetAddressGroup *group = INTERNET_ADDRESS_GROUP(address);
      InternetAddressList *group_list = internet_address_group_get_members(group); // transer none

      if (group_list) {
        guint gsize = internet_address_list_length(group_list);
        if (gsize)
          collect_addresses_into(group_list, addr_list, gsize);
      }

    } else if (INTERNET_ADDRESS_IS_MAILBOX(address)) {
      InternetAddressMailbox *mailbox = INTERNET_ADDRESS_MAILBOX(address);
      const gchar *name    = internet_address_get_name(address);
      const gchar *address = internet_address_mailbox_get_addr(mailbox);
      Address *addr = new_address(address, name);
      addresses_list_add(addr_list, addr);
    }
  }
}



static AddressesList *collect_str_addresses(const gchar* addresses_list_str) {
  g_return_val_if_fail(addresses_list_str != NULL, NULL);

  AddressesList* result = NULL;
  InternetAddressList *addresses = internet_address_list_parse_string(addresses_list_str); // transfer-FULL

  if (addresses) {
    guint addresses_length = internet_address_list_length(addresses);
    if (addresses_length) {
      result = new_addresses_list();
      collect_addresses_into(addresses, result, addresses_length);
    }
    g_object_unref(addresses);
  }
  return result;
}


static AddressesList *collect_addresses(InternetAddressList *list) {
  g_return_val_if_fail(list != NULL, NULL);

  AddressesList* result = NULL;
  guint size = internet_address_list_length(list);
  if (size) {
    result = new_addresses_list();
    collect_addresses_into(list, result, size);
  }
  return result;
}


static AddressesList *get_from_addresses(GMimeMessage *message) {
  const gchar *from_str = g_mime_message_get_sender(message); // transfer-none
  if (from_str)
    return collect_str_addresses(from_str);
  return NULL;
}


static AddressesList *get_reply_to_addresses(GMimeMessage *message) {
  const gchar *reply_to_string = g_mime_message_get_reply_to(message); // transfer-none
  if (reply_to_string)
    return collect_str_addresses(reply_to_string);
  return NULL;
}


static AddressesList *get_to_addresses(GMimeMessage *message) {
  InternetAddressList *recipients_to = g_mime_message_get_recipients(message, GMIME_RECIPIENT_TYPE_TO); // transfer-none
  if (recipients_to)
    return collect_addresses(recipients_to);
  return NULL;
}


static AddressesList *get_cc_addresses(GMimeMessage *message) {
  InternetAddressList *recipients_cc = g_mime_message_get_recipients(message, GMIME_RECIPIENT_TYPE_CC); // transfer-none
  if (recipients_cc)
    return collect_addresses(recipients_cc);
  return NULL;
}


static AddressesList *get_bcc_addresses(GMimeMessage *message) {
  InternetAddressList *recipients_bcc = g_mime_message_get_recipients(message, GMIME_RECIPIENT_TYPE_BCC); // transfer-none
  if (recipients_bcc)
    return collect_addresses(recipients_bcc);
  return NULL;
}


static MessageBody* get_body(CollectedPart *body_part, gboolean sanitize_body, GPtrArray *inlines) {
  g_return_val_if_fail(body_part != NULL, NULL);

  MessageBody *mb = new_message_body();

  // We keep the raw size intentionally
  mb->size = body_part->content->len;

  mb->content_type = g_strdup(body_part->content_type);

  if (sanitize_body) {
    // Parse any HTML tags
    GString *raw_content = g_string_new_len((const gchar*) body_part->content->data, body_part->content->len);
    GumboOutput* output = gumbo_parse_with_options(&kGumboDefaultOptions, raw_content->str, raw_content->len);

    // Remove unallowed HTML tags (like scripts, bad href etc..)
    GString *sanitized_content = sanitize(output->document, inlines);
    mb->content = sanitized_content;

    gumbo_destroy_output(&kGumboDefaultOptions, output);
    g_string_free(raw_content, TRUE);
  } else {
    mb->content = g_string_new_len((const gchar*) body_part->content->data, body_part->content->len);
  }

  return mb;
}


static gchar *guess_content_type_extension(const gchar *content_type) {
  g_return_val_if_fail(content_type != NULL, NULL);

  gchar *extension = "txt";
  if (!g_ascii_strcasecmp(content_type, "text/plain")) {
    extension = "txt";
  } else if (!g_ascii_strcasecmp(content_type, "text/html")) {
    extension = "html";
  } else if (!g_ascii_strcasecmp(content_type, "text/rtf")) {
    extension = "rtf";
  } else if (!g_ascii_strcasecmp(content_type, "text/enriched")) {
    extension = "etf";
  } else if (!g_ascii_strcasecmp(content_type, "text/calendar")) {
    extension = "ics";
  } else if (!g_ascii_strcasecmp(content_type, "image/jpeg") ||
             !g_ascii_strcasecmp(content_type, "image/jpg")) {
    extension = "jpg";
  } else if (!g_ascii_strcasecmp(content_type, "image/pjpeg")) {
    extension = "pjpg";
  } else if (!g_ascii_strcasecmp(content_type, "image/gif")) {
    extension = "gif";
  } else if (!g_ascii_strcasecmp(content_type, "image/png") ||
             !g_ascii_strcasecmp(content_type, "image/x-png")) {
    extension = "png";
  } else if (!g_ascii_strcasecmp(content_type, "image/bmp")) {
    extension = "bmp";
  }
  return extension;
}


static gchar *filename_for(CollectedPart *part) {
  if (part->filename)
    return g_strdup(part->filename);

  if (part->content_id) {
    if (gc_contains_c(part->content_id, '.'))
      return g_strjoin(NULL, "_", part->content_id, NULL);
    return g_strjoin(NULL, "_", part->content_id, ".", guess_content_type_extension(part->content_type), NULL);
  }
  return g_strjoin(NULL, "_unnamed", ".", guess_content_type_extension(part->content_type), NULL);
}


static void add_attachments_from_parts(MessageAttachmentsList *list, GPtrArray *att_parts) {
  g_return_if_fail((att_parts != NULL) && (list != NULL));
  g_return_if_fail(att_parts->len > 0);

  guint i;
  for (i = 0; i < att_parts->len; i++) {
    CollectedPart *att_part = g_ptr_array_index(att_parts, i);
    MessageAttachment *attachment = new_message_attachment(att_part->part_id);
    attachment->content_type = g_strdup(att_part->content_type);
    attachment->size = att_part->content->len;
    attachment->filename = filename_for(att_part);
    message_attachments_list_add(list, attachment);
  }
}



static MessageAttachmentsList *get_attachments(PartCollectorData *pdata) {
  g_return_val_if_fail(pdata != NULL, NULL);
  g_return_val_if_fail((pdata->attachments != NULL) ||
                       (pdata->inlines != NULL) ||
                       (pdata->alternative_bodies != NULL), NULL);

  MessageAttachmentsList *att_list = new_message_attachments_list();

  if (pdata->alternative_bodies && pdata->alternative_bodies->len > 0)
    add_attachments_from_parts(att_list, pdata->alternative_bodies);

  if (pdata->attachments && pdata->attachments->len > 0)
    add_attachments_from_parts(att_list, pdata->attachments);

  if (pdata->inlines && pdata->inlines->len > 0)
    add_attachments_from_parts(att_list, pdata->inlines);

  if (att_list->len)
    return att_list;

  free_message_attachments_list(att_list);
  return NULL;
}


static MessageData *convert_message(GMimeMessage *message, guint content_option) {
  if (!message)
    return NULL;

  MessageData *md = new_message_data();

  const gchar *message_id = g_mime_message_get_message_id(message);
  if (message_id)
    md->message_id = g_strdup(message_id);

  md->from     = get_from_addresses(message);
  md->reply_to = get_reply_to_addresses(message);
  md->to       = get_to_addresses(message);
  md->cc       = get_cc_addresses(message);
  md->bcc      = get_bcc_addresses(message);

  const gchar *subject = g_mime_message_get_subject(message);
  if (subject)
    md->subject = g_strdup(subject);

  md->date = g_mime_message_get_date_as_string(message);

  const gchar *in_reply_to = g_mime_object_get_header(GMIME_OBJECT (message), "In-Reply-To");
  if (in_reply_to) {
    gchar *in_reply_to_str = g_mime_utils_header_decode_text(in_reply_to);
    md->in_reply_to = g_mime_references_decode(in_reply_to_str);
    g_free(in_reply_to_str);
  }

  const gchar *references = g_mime_object_get_header(GMIME_OBJECT (message), "References");
  if (references) {
    gchar *references_str = g_mime_utils_header_decode_text(references);
    md->references = g_mime_references_decode(references_str);
    g_free(references_str);
  }

  if (content_option) {
    PartCollectorData *pc = collect_parts(message, content_option);

    if (pc->text_part)
      md->text = get_body(pc->text_part, (content_option != COLLECT_RAW_CONTENT), NULL);

    if (pc->html_part)
      md->html = get_body(pc->html_part, (content_option != COLLECT_RAW_CONTENT), pc->inlines);

    md->attachments = get_attachments(pc);

    free_part_collector_data(pc);
  }

  return md;
}



static JSON_Value *address_to_json(Address *addr) {
  if (!addr)
    return NULL;

  JSON_Value *address_value = json_value_init_object();
  JSON_Object *address_object = json_value_get_object(address_value);

  json_object_set_string(address_object, "name",    addr->name);
  json_object_set_string(address_object, "address", addr->address);

  return address_value;
}


static JSON_Value *addresses_list_to_json(AddressesList *addr_list) {
  if (!addr_list)
    return NULL;

  JSON_Value *addreses_value = json_value_init_array();
  JSON_Array *addresses_ary = json_value_get_array(addreses_value);

  guint i;
  for (i = 0; i < addr_list->len; i++) {
    Address *addr = addresses_list_get(addr_list, i);
    JSON_Value *jaddr = address_to_json(addr);
    json_array_append_value(addresses_ary, jaddr);
  }

  return addreses_value;
}


static JSON_Value *message_body_to_json(MessageBody *mbody) {
  if (!mbody)
    return NULL;

  JSON_Value *body_value = json_value_init_object();
  JSON_Object *body_object = json_value_get_object(body_value);

  json_object_set_string(body_object, "type",    mbody->content_type);
  json_object_set_string(body_object, "content", mbody->content->str);
  json_object_set_number(body_object, "size",    mbody->size);

  return body_value;
}



static JSON_Value *message_attachments_list_to_json(MessageAttachmentsList *matts) {
  if (!matts)
    return NULL;

  JSON_Value *attachments_value = json_value_init_array();
  JSON_Array *attachments_array = json_value_get_array(attachments_value);
  guint i;
  for (i = 0; i < matts->len; i++) {
    MessageAttachment *att = message_attachments_list_get(matts, i);
    JSON_Value  *attachment_value = json_value_init_object();
    JSON_Object *attachment_object = json_value_get_object(attachment_value);
    json_object_set_number(attachment_object, "partId",   att->part_id);
    json_object_set_string(attachment_object, "type",     att->content_type);
    json_object_set_string(attachment_object, "filename", att->filename);
    json_object_set_number(attachment_object, "size",     att->size);
    json_array_append_value(attachments_array, attachment_value);
  }
  return attachments_value;
}


static JSON_Value *references_to_json(GMimeReferences *references) {
  if (!references)
    return NULL;

  const char *msgid;
  const GMimeReferences *cur;
  JSON_Value *references_value = json_value_init_array();
  JSON_Array *references_array = json_value_get_array(references_value);

  for (cur = references; cur; cur = g_mime_references_get_next(cur)) {
    msgid = g_mime_references_get_message_id (cur);
    json_array_append_string(references_array, msgid);
  }

  return references_value;
}


static GString *gmime_message_to_json(GMimeMessage *message, guint content_option) {
  MessageData *mdata = convert_message(message, content_option);


  JSON_Value *root_value = json_value_init_object();
  JSON_Object *root_object = json_value_get_object(root_value);

  json_object_set_value(root_object,  "from",        addresses_list_to_json(mdata->from));
  json_object_set_value(root_object,  "to",          addresses_list_to_json(mdata->to));
  json_object_set_value(root_object,  "replyTo",     addresses_list_to_json(mdata->reply_to));
  json_object_set_value(root_object,  "cc",          addresses_list_to_json(mdata->cc));
  json_object_set_value(root_object,  "bcc",         addresses_list_to_json(mdata->bcc));
  json_object_set_string(root_object, "messageId",   mdata->message_id);
  json_object_set_string(root_object, "subject",     mdata->subject);
  json_object_set_string(root_object, "date",        mdata->date);

  json_object_set_value(root_object, "inReplyTo",   references_to_json(mdata->in_reply_to));
  json_object_set_value(root_object, "references",  references_to_json(mdata->references));

  json_object_set_value(root_object,  "text",        message_body_to_json(mdata->text));
  json_object_set_value(root_object,  "html",        message_body_to_json(mdata->html));
  json_object_set_value(root_object,  "attachments", message_attachments_list_to_json(mdata->attachments));

  free_message_data(mdata);
  gchar *serialized_string = json_serialize_to_string(root_value);
  json_value_free(root_value);

  GString *json_string = g_string_new(serialized_string);
  g_free(serialized_string);

  return json_string;
}



/*
 *
 *
 */
GString *gmimex_get_json(gchar *path, guint content_option) {
  g_mime_init(GMIME_ENABLE_RFC2047_WORKAROUNDS);

  GMimeMessage *message = gmime_message_from_path(path);
  if (!message)
    return NULL;

  GString *json_message = gmime_message_to_json(message, content_option);
  g_object_unref(message);

  g_mime_shutdown();
  return json_message;
}


/*
 *
 *
 */
GByteArray *gmimex_get_part(gchar *path, guint part_id) {
  g_mime_init(GMIME_ENABLE_RFC2047_WORKAROUNDS);

  GMimeMessage *message = gmime_message_from_path(path);
  if (!message)
    return NULL;

  GByteArray *attachment = gmime_message_get_part_data(message, part_id);
  g_object_unref(message);

  g_mime_shutdown();
  return attachment;
}
