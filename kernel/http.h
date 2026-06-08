#ifndef HTTP_H
#define HTTP_H

#define HTTP_MAX_URL   256
#define HTTP_MAX_BODY  512

int agent_http_fetch(const char *url, char *buf, int buflen);

#endif
