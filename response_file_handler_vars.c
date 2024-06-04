#include "response_file_handler.h"

char content_type_array[][30] = {
    "image/jpeg", 
    "image/png", 
    "image/tiff",
    "image/gif",
    "image/svg+xml",
    "image/x-icon",
    "video/mp4",
    "text/html", 
    "text/css", 
    "text/plain",
    "text/javascript",
    "text/csv",
    "text/xml",
    "application/pdf"
};

char http_404_response_pt1[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: 174\r\nExpires: -1\r\nDate: ";
char http_404_response_pt2[] = "\r\nConnection: close\r\n\r\n<!doctype html><html><head><meta http-equiv='content-type' content='text/html;charset=utf-8'><title>404 Not Found</title></head><body>Arquivo nao foi encontrado</body></html>";

char http_ok_response_pt1[] = "HTTP/1.1 200 OK\r\nContent-Type: ";
char http_ok_response_pt2[] = "; charset=UTF-8\r\nContent-Length: ";
char http_ok_response_pt3[] = "\r\nExpires: -1\r\nDate: ";
char http_ok_response_pt4[] = "\r\nConnection: keep-alive\r\n\r\n";