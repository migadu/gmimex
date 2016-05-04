#include <glib/gstdio.h>
#include <glib.h>
#include "erl_comm.h"
#include "gmimex.h"
#include "parson.h"
#include "string.h"

#define JSON_NO_MESSAGE_CONTENT 0
#define JSON_PREPARED_MESSAGE_CONTENT 1
#define JSON_RAW_MESSAGE_CONTENT 2

int main(void) {
    int bytes_read;
    gchar buffer[MAX_BUFFER_SIZE];
    JSON_Value *root_value;
    JSON_Object *root_object;
    gchar *json_str;
    gchar *func_name;
    gchar *path;

    while((bytes_read = read_msg(buffer)) > 0) {
    	json_str = g_strndup((const gchar *)buffer, bytes_read);
    	root_value = json_parse_string(json_str);
    	root_object = json_value_get_object(root_value);
    	func_name = (gchar *)json_object_get_string(root_object, "exec");
    	path = (gchar *)json_object_get_string(root_object, "path");
			GString *json_message = NULL;

    	if (!g_ascii_strcasecmp(func_name, "get_preview_json")) {
				json_message = gmimex_get_json(path, JSON_NO_MESSAGE_CONTENT);
  			if (!json_message) {
  				send_err();
  			} else {
					send_msg((gchar *)json_message->str, json_message->len);
			  	g_string_free(json_message, TRUE);
			  }
    	} else if (!g_ascii_strcasecmp(func_name, "get_json")) {
  			gboolean raw = json_object_get_boolean(root_object, "raw");
  			json_message = gmimex_get_json(path, (raw ? JSON_RAW_MESSAGE_CONTENT : JSON_PREPARED_MESSAGE_CONTENT));
  			if (!json_message) {
  				send_err();
  			} else {
					send_msg((gchar *)json_message->str, json_message->len);
			  	g_string_free(json_message, TRUE);
			  }
    	} else if (!g_ascii_strcasecmp(func_name, "get_part")) {
    		int part_id = json_object_get_number(root_object, "partId");
			  GByteArray *part_content = gmimex_get_part(path, part_id);
			  if (!part_content) {
			  	send_err();
			  } else {
    			send_msg((gchar *)part_content->data, part_content->len);
    			g_byte_array_free(part_content, TRUE);
    		}
    	}
    	json_value_free(root_value);
    	g_free(json_str);
    }
    return 0;
}
