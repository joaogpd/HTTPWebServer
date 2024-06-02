#include "response_file_handler.h"

char content_type_array[][30] = {
    "image/jpeg", 
    "image/png", 
    "text/html", 
    "text/css", 
    "text/plain"
};

char http_404_response[] = "HTTP/1.1 404 Not Found\r\n \
    Content-Type: text/html; charset=UTF-8\r\n \
    Content-Length: 174\r\n \
    Connection: close\r\n \
    \r\n\r\n \
    <!doctype html> \
    <html> \
    <head> \
        <meta http-equiv='content-type' content='text/html;charset=utf-8'> \
        <title>404 Not Found</title> \
    </head> \
    <body> \
        Arquivo não foi encontrado \
    </body> \
    </html>";

char http_ok_response_pt1[] = "HTTP/1.1 200 OK\r\n \
    Content-Type: ";
char http_ok_response_pt2[] = "; charset=UTF-8\r\n \
    Content-Length: ";
char http_ok_response_pt3[] = "\r\nConnection: close\r\n\\r\n\r\n";