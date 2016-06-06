/* Feather, a bare bones http web service
 * Expect very little in features.
 *
 * Copyright 2016, Guido Witmond <guido@witmond.nl>
 * License: AGPL v3.0 or later.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "feather/state.h"


// Parse requests like:
//     "GET /index-of_files.html?arg=1&foo=bar&bla=%32baz HTTP/1.1\r\n"
//     "User-Agent: Wget/1.16 (linux-gnu)\r\n"
//     "Accept: */*\r\n"
//     "Host: eccentric-authentication.org\r\n"
//     "Connection: Keep-Alive\r\n"
//     "\r\n"

// Paramaters:
//   state.buffer: a preallocated buffer to receive data from the network
//   state.buf_size: size in bytes of this buffer

// Returns:
//   state.finished: <bool> whether the parser finished the complete header
//   state.err:  code with http-error (400, 404, 500, etc), 0 on success
//   state.err_mesg: reason, such as 'malloc failed, cannot bind'
//   state.host & state.host_end: contents of host-header, eg: eccentric-authentication.org
//   state.url & state.url_end:   url of the GET/HEAD request,eg: /index.html

// Ignore:
// - almost all headers;
// - query parameters;
// - almost all methods, only GET/HEAD
// - almost all of the http protocol


%%{
  machine http;

  # debugging and tracing actions
  # action START     { printf("starting\n"); }
  # action PR        { printf("char %c\n", fc); }
  # action HH        { printf("host "); }
  # action AH        { printf("anyh "); }
  # action DEB       { printf("char: %c; curr: %d; target: %d; p: %x\n", fc, fcurs, ftargs, p); }

  # non-debugging variants
  action START     { }
  action PR        { st->err_pos = fpc; } # set at every advance, points to parse error at abort
  action HH        { }
  action AH        { }
  action DEB       { }

  # data gathering actions start with TAG and end with GRAB_x
  action TAG       { tag_start = fpc;
		     // printf("tagging at %x\n", p);
		   }
  action GRAB_HOST {
		     st->host = tag_start;
		     st->host_end = fpc;
                   }
  action GRAB_PATH {
		     st->url =tag_start;
		     st->url_end = fpc;
                   }

  crlf = 13 10;
  sp   = 32;
  ht   =  9;
  lws  = crlf (sp | ht)+ ; # header line continuation

  # Be restrictive in path characters, no encoded characters.
  # At least one character between slashes.
  # Be lenient in query characters, we ignore query parts for now anyway.
  pchars = [a-zA-Z0-9\.\-_] ;
  path   = '/' (pchars+ ( '/' | '/' pchars+ )* )* ;
  query  = [a-zA-Z0-9\.\-_=@%&]* ;
  url    = path >TAG %GRAB_PATH ( '?' query )? ;

  method = "GET" | "HEAD";
  version = "HTTP/" ( "1.0" | "1.1" ) ;

  hostname =  [a-zA-Z0-9\-\.]+ >TAG %GRAB_HOST ;

  # ignore the port number for host headers
  host_header = "Host:" sp hostname $HH ( ':' digit+ )? crlf ;
  keepalive_header = "Connection: " ( [Kk]"eep-" [Aa]"live" | [Cc]"lose" ) crlf;
  any_header = (( alpha ( alnum | '-' )* ) - ('Host'|'Connection')) $AH ':' 32..127* crlf;

  headers = ( host_header | keepalive_header | any_header)* ;

  # GET $URL HTTP1.1
  main := (
    method >START sp url sp version crlf
    headers
    crlf # end of header
    # 'body' 0
    # any* $PR  # eat body
  ) $PR ;
  }%%

  %% write data;

void http_parse(struct state *st)
{

  //# internal state machine variables (non thread-safe)
  int cs = 0;
  char *p = st->buffer;
  char *pe = st->buffer + st->length;

  //# string tag variables (non thread-safe)
  char *tag_start = 0;

  // start Ragel state machine
  %% write init;
  %% write exec;

  // determine if the state machine is finished.
  st->finished = p == pe;

  if (!st->finished) {
    st->err = 400;
    st->err_mesg = "State machine did not finish: parse error.";
  }

  // ignore pipelined requests and keepalives, as caller closes fd.
  return; // state st
}
