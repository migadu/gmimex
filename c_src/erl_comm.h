//Code mostly based on: http://www.erlang.org/doc/tutorial/c_port.html
#include <string.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 65535

#define STDIN  0
#define STDOUT 1

/*
 * Helper function to read data from Erlang/Elixir from stdin.
 * Returns the number of bytes read (-1 on error), fills buffer with data.
 */
static int read_input(gchar* buffer, int length) {
    int bytes_read = read(STDIN, buffer, length);
    if (bytes_read != length)
      return -1;
    return bytes_read;
}

int read_msg(gchar* buffer) {
    gchar bytes[4]; //first 4 bytes contain length of the message.
    guint32 length;

    if(read_input(bytes, 4) != 4)
      return -1;

    length = (bytes[0] << 24) + (bytes[1] << 16) + (bytes[2] << 8) + bytes[3] & 0xff;
    return read_input(buffer, length);
}

void send_msg(gchar* buffer, int length) {
    gchar len[4]; //first 4 bytes contain length of the message.
    len[0] = (length >> 24) & 0xff;
    len[1] = (length >> 16) & 0xff;
    len[2] = (length >> 8) & 0xff;
    len[3] = length & 0xff;
    write(STDOUT, len, 4);
    write(STDOUT, buffer, length);
}

void send_err(void) {
  send_msg("err", 3);
}
