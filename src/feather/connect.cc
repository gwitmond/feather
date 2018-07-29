/* Feather, a bare bones http web service
 * Expect very little in features.
 *
 * Copyright 2016, Guido Witmond <guido@witmond.nl>
 * License: GPL v3.0 or later.
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

#pragma GCC diagnostic ignored "-Wwrite-strings"

// make the compiler happy
extern "C" void http_parse(struct state* st);
const char* determine_mimetype(char* path);
void strappend(char*, size_t, const char*);
void copy_contents(int fd, FILE* fh);
void write_400(int);
void write_404(int);
void write_500(int);

// the main server socket.
int s;

int main() {

  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_port = htons(80);
  sa.sin_family = AF_INET;
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s == -1) {
    printf("failed to open socket: %s\n", strerror(errno));
    exit(1);
  }

  int b = bind(s, (struct sockaddr *)&sa, sizeof(sa));
  if (b == -1) {
    printf("failed to bind socket: %s\n", strerror(errno));
    close(s);
    exit(2);
  }

  int l = listen(s, 5);
  if (l == -1) {
    printf("failed to listen: %s\n", strerror(errno));
    close(s);
    exit(3);
  }

  while (1) {
    struct state st;
    char buffer[1024];
next:
    memset (&st, 0, sizeof(st));
    st.buffer = buffer;
    st.buf_size = sizeof(buffer);

    printf("waiting for connection...\n");

    struct sockaddr caddr;
    socklen_t l_caddr = (socklen_t)sizeof(caddr);
    st.fd = accept(s, &caddr, &l_caddr);
    if (st.fd == -1) {
      printf("failed to accept: %s\n", strerror(errno));
      close(s);
      exit(4);
    }

    // printf("received connection\n");

    memset(st.buffer, 0, st.buf_size);
    st.length = read(st.fd, st.buffer, st.buf_size -1); // leave last char \0 in case of too large request.
    if (st.length == -1) {
      printf("error reading client socket: %s\n", strerror(errno));
      write_500(st.fd);
      close(st.fd);
      goto next;
    }
    // TODO: read requests longer than st.buf_size

    // printf("read %d bytes from socket.\n", st.length);

    http_parse(&st);
    // printf("result = %d\n", st.finished );

    // first check for explicit errors
    if (st.err != 0) {
      printf("error %d - %s\n", st.err, st.err_mesg);
      char p;  // capture char that gave the parse error
      if (st.err_pos == NULL ) {  // wrong http-method at pos 0
	st.err_pos = st.buffer;
        p = *st.err_pos;
      } else {
	p = *++st.err_pos;
      }
      *st.err_pos = '\0';   // set to 0 to end correctly parsed part

      // Show from start of line
      char* start = strrchr(st.buffer, '\n');
      if (start == NULL) {
	start = st.buffer;
      } else {
	start++;  // skip the \n found at strrchr
      }

      // until the end of the line
      char* end = strchr(st.err_pos + 1, '\r');
      if (end != NULL) { *end = '\0'; }
      printf("Culprit: %s>>HERE>>%c%s\n", start, p, st.err_pos + 1);
      write_400(st.fd);
      close(st.fd);
      goto next;
    }

    // Make host and url C-type strings before use.
    if (st.host_end) { *st.host_end = '\0'; }
    if (st.url_end)  { *st.url_end  = '\0'; }

    // if (st.host) { printf("host: >%s<\n", st.host); }
    if (st.url)  { printf("url: >%s<\n", st.url); }

    // Prevent treewalk attack when hostname is not specified.
    // instead return a 404 at stat(2).
    if (st.host == NULL) {
      st.host = "unknown.host";
    }

    // Lame virtual hosting
    if      (strcmp(st.host, "www.eccentric-authentication.org") == 0) { st.host = "eccentric-authentication.org"; }
    else if (strcmp(st.host,     "eccentric-authentication.nl")  == 0) { st.host = "eccentric-authentication.org"; }
    else if (strcmp(st.host, "www.eccentric-authentication.nl")  == 0) { st.host = "eccentric-authentication.org"; }
    else if (strcmp(st.host, "www.eccentric-authentication.com") == 0) { st.host = "eccentric-authentication.org"; }
    else if (strcmp(st.host,     "eccentric-authentication.com") == 0) { st.host = "eccentric-authentication.org"; }
    else if (strcmp(st.host, "www.groeninkoptiek.nl") == 0)            { st.host = "groeninkoptiek.nl"; }

    // Fetch file from disk and serve its contents.
    // Files are stored in /websites/<hostname>/<url>
    char path[1024];
    *path = '\0';
    const char *docroot = "/websites/";
    strappend(path, sizeof(path), docroot);
    strappend(path, sizeof(path), st.host);
    strappend(path, sizeof(path), st.url);
    path[sizeof(path) -1] = '\0';

    // remove trailing slashes
    while (*(path + strlen(path) -1 ) == '/') {
      *(path + strlen(path) -1) = '\0';
    }

    // printf("checking path: >%s<\n", path);
    struct stat stt;
    if (stat(path, &stt) == -1) {
      printf("failed to stat file: %s\n", strerror(errno));
      write_404(st.fd);
      close(st.fd);
      goto next;
    }

    if (S_ISDIR(stt.st_mode)) {
      // if we request a directory, show the index.html
      strappend(path, sizeof(path), "/index.html");
    }

    // printf("fetching file: >%s<\n", path);
    FILE* fh = fopen(path, "r");
    if (fh == NULL) {
      write_404(st.fd);
      close(st.fd);
      goto next;
    }

    const char *mimetype;
    mimetype = determine_mimetype(path);

    // We have data for the request: give it to the caller
    char message[1024];
    *message = '\0';
    strappend(message, sizeof(message), "HTTP/1.1 200 OK\r\n");
    strappend(message, sizeof(message), "Mime-type: ");
    strappend(message, sizeof(message), mimetype);
    strappend(message, sizeof(message), "\r\n\r\n");
    write(st.fd, message, strlen(message));

    copy_contents(st.fd, fh);

    fclose(fh);
    close(st.fd);
  }
  // NOTREACHED
}

void write_400(int fd) {
  const char *message =
    "HTTP/1.1 400 Bad Request\r\n"
    "\r\n"
    "Whadda ya think!\r\n";
  write(fd, message, strlen(message));
}

void write_404(int fd) {
  const char *message =
    "HTTP/1.1 404 Not Found\r\n"
    "\r\n"
    "I said 'snot here, captain.\r\n";
  write(fd, message, strlen(message));
}

void write_500(int fd) {
  const char *message =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "\r\n"
    "Something went horribly wrong.\r\n";
  write(fd, message, strlen(message));
}


void copy_contents(int fd, FILE* fh) {
  // copy the contents of the file
  char buffer[65536];
  int done = 0;
  while (!done) {
    size_t size = fread((void*)buffer, 1, sizeof(buffer), fh);
    if (size > 0) {
      write(fd, buffer, size);
    } else {
      done = 1;
    }
  }
}

void strappend(char *buffer, size_t size, const char* addendum) {
  if (addendum == NULL) {
    printf("adding NULL string to %s; ignoring\n", buffer);
    return;
  }
  size_t length = strlen(buffer);
  strncpy(buffer + length, addendum, size -1 - length - strlen(addendum));
  return;
}

const char* determine_mimetype(char* path) {
  char* ext;
  ext = strrchr(path, '.');
  // printf("ext is: %s\n", ext);
  if (ext == NULL) {                      return "text/plain";
  } else if (strcmp(ext, ".html") == 0) { return "text/html";
  } else if (strcmp(ext, ".png")  == 0) { return "image/png";
  } else if (strcmp(ext, ".css")  == 0) { return "text/css";
  } else if (strcmp(ext, ".jpg")  == 0) { return "image/jpeg";
  } else if (strcmp(ext, ".jpeg") == 0) { return "image/jpeg";
  } else if (strcmp(ext, ".gif")  == 0) { return "image/gif";
  }
  // etc etc.

  return "text/plain"; // by default
}
