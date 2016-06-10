/* Feather, a bare bones http web service
 * Expect very little in features.
 *
 * Copyright 2016, Guido Witmond <guido@witmond.nl>
 * License: GPL v3.0 or later.
 */

struct state {
  int fd;           // the fd with the input stream
  char *buffer;     // buffer for the socket data
  ssize_t buf_size; // size of the buffer
  ssize_t length;   // length of data in buffer
  int finished;     // whether or not the final state has reached, ie, parsed correctly
  int err;          // any error code is reported here
  char *err_mesg;   // static allocated string with message
  char *err_pos;    // last good position in buffer on state machine error
  char *host;       // pointer to the start contents of the host header;
  char *host_end;   // pointer to the char after the host header; set to \0 before use state.host
  char *url;        // pointer to the start the url requested;
  char *url_end;    // pointer to the char after the url; set to \0 before use state.url
};
