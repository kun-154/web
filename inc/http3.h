/*************************************************************************
	> File Name: http3.h
	> Author: 
	> Mail: 
	> Created Time: 2021年12月08日 星期三 14时40分46秒
 ************************************************************************/

#ifndef _HTTP3_H
#define _HTTP3_H

void send_error(int cfd, int status, char *title, char *text);

void http_request(int cfd, const char* request);

void send_dir(int cfd, const char* dirname);

void send_respond_head(int cfd, int no, const char* desp, const char* type, long len);


void send_file(int cfd, const char* filename);

int get_line(int sock, char *buf, int size);

int hexit(char c);

void decode_str(char *to, char *from);

const char *get_file_type(const char *name);




#endif
