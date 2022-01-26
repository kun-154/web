/*************************************************************************
	> File Name: http.c
	> Author:liu 
	> Mail: 
	> Created Time: Tue 30 Nov 2021 05:05:36 PM CST
 ************************************************************************/

#include "head.h"
#include <sys/stat.h>
#include <DBG.h>

#define MAXSIZE 2048

void disconnect(int fd, int epfd) {
    DBG("disconnect\n");
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    if (ret < 0) {
        perror("epoll_ctl error\n");
        exit(1);
    }
    close(fd);
}

const char *get_file_type(const char *name) {
    char *dot;
    dot = strrchr(name, ',');
    // strchr  
    // strrchr
    if (dot == NULL) {
        return "text/plain; charset=utf-8";
    }else if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) {
        return "text/html; charset=utf-8";
    }else if (strcmp(dot, ".jpg" == 0) || strcmp(dot, ".jpeg") == 0) {
        return "image/jpeg";
    }else if (strcmp(dot, "png") == 0) {
        return "image/png";
    }else if (strcmp(dot, ".mov") == 0) {
        return "video/quicktime";
    }else if (strcmp(dot, "./mp3") == 0) {
        return "audio/mpeg";
    }else if (strcmp(dot, "gif") == 0) {
        return "image/gif";
    }

    return "text/plain; charset=utf-8";
}

int do_accept(int lfd, int epfd) {
    DBG("do_accept\n");
    struct sockaddr_in cli_addr;
    struct epoll_event ev;
    memset(&cli_addr, 0, sizeof(cli_addr));
    int size = sizeof(cli_addr);
    int cfd = accept(lfd, (struct sockaddr *)&cli_addr, &size);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = cfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev) < 0) {
        perror("epoll_ctl error");
        exit(1);
    }
    return cfd;
}

void error_send(int cfd, int status, char *title, char *text) {
    char buf[4096] = {0};

    sprintf(buf, "%s %d %s\r\n", "HTTP/1.1", status, title);
    sprintf(buf + strlen(buf), "Content-Type:%s\r\n", "text/html");
    sprintf(buf + strlen(buf), "Content-Length:%d\r\n", -1);
    sprintf(buf + strlen(buf), "Connection: close\r\n");
    send(cfd, buf, strlen(buf), 0);
    send(cfd,"\r\n", 2, 0);

    memset(buf, 0, sizeof(buf));

    sprintf(buf, "<html><head><title>%d %s</title></head>", status, title);
    sprintf(buf + strlen(buf), "<body bgcolor=\"#cc99cc\"><h4 align=\"center\">%d %s</h4>\n", status, title);
    sprintf(buf + strlen(buf), "%s\n", text);
    sprintf(buf + strlen(buf), "<hr>\n<\body>\n</html>\n");
    send(cfd, buf, strlen(buf), 0);
    return ;
}

void send_dir(const char *dirname, int cfd) {
    DBG("send dir\n");
    int i, ret;
    char buf[4096] = {0};

    sprintf(buf, "<html><head><title> dirname: %s</title></head>", dirname);
    sprintf(buf + strlen(buf), "<body><h1> current dirname: %s</h1><table>", dirname);

    char enstr[1024] = {0};
    char path[1024] = {0};

    struct dirent **ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort);

    for (int i = 0; i < num; ++i) {
        char *name = ptr[i]->d_name;

        sprintf(path, "%s/%s",dirname, name);
        DBG("path = %s\n", path);
        struct stat st;
        stat(path, *st);
        encode_str(enstr, sizeof(enstr), name);

        if (S_ISREG(st.st_mode)) {
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s\">%s</a></td><td> %ld</td></tr>", enstr, name, long(st.st_size));
        }else if (S_ISDIR(st.st_mode)) {
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s/\">%s</a></td><td> %ld</td></tr>", enstr, name, long(st.st_size));
        }
        ret = send(cfd, buf, strlen(buf), 0);
    }
}




void send_file(const char *file, int cfd) {
    DBG("send_file\n");
    int fd = open(file, O_RDONLY);
    int n = 0;
    char buf[4096] = {0};
    if (fd == -1) {
        // 404 错误页面
        perror("open");
        exit(1);
    }
    while ((n = read(fd, buf, sizeof(buf)) > 0)) {
        int ret = send(cfd, buf, strlen(buf), 0);
        if (ret == -1) {
            if (errno == EAGAIN) {
                DBG("send EAGAIN\n");
                continue;
            }else if (errno == EINTR) {
                DBG("send EINTR\n");
                continue;
            }else {
                perror("send error");
                exit(1);
            }
        }
        DBG("%s", buf);
        if (ret < 4096) {
            DBG("send ret: %d\n", ret);
        }
    }
    close(fd);
}



void send_respond_head(int cfd, int no, const char *disp, const char *type, int len) {
    DBG("send_respond\n");
    char buf[1024] = {0};
    sprintf(buf, "HTTP/1.1 %d %s\r\n", no, disp);
    sprintf(buf + strlen(buf), "Content-Type: %s\r\n", type);
    sprintf(buf + strlen(buf), "Content-Length:%d\r\n", len);
    send(cfd, "\r\n", 2, 0);
    send(cfd, buf, strlen(buf), 0);

}


// 处理http 请求  判断文件是否存在， 再回复
void http_request(const char *file, int cfd) {
    DBG("deel http_request\n");

    struct stat sbuf;
    int ret = stat(file, &sbuf);
    if (ret != 0) {
        // 回发浏览器 404 错误页面
        DBG("client request error\n");
        exit(1);
    }
    if (S_ISDIR(sbuf.st_mode)) {
        send_respond_head(cfd, 200, "OK", get_file_type(".html"), -1);
        send_dirfile(file. cfd);
    }else if (S_ISREG(sbuf.st_mode)) {
        // 是一个普通文件 打开文件 回复给客户端
        // 编辑http 协议应答
        send_respond_head(cfd, 200, "OK", get_file_type(file), sbuf.st_size);
        //send_respond_head(cfd, 200, "OK", "Content-Type: text/plain; charset=iso-8859-1", sbuf.st_size);
        // 回发文件内容
        send_file(file, cfd);
        
    }
}

/*
void do_read(int cfd, int epfd) {
    char line[1024] = {0};
    int len = get_line(cfd, line, sizeof(line));
    // 读取http协议 首行 : GET xxx http/1.1
    if (len == 0) {
        DBG("client close...\n");
        disconnect(cfd, epfd);
    }else {
        char method[16], path[256], protocol[16];
        sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);

        DBG("method: %s path: %s protocol: %s\n", method, path, protocol);
        while (1) {
            char buf[1024] = {0};
            len = get_line(cfd, buf, sizeof(buf));
            if (len == 1 || len == -1 || len == 0) break;
            DBG("buf :  %d %s \n", len, buf);
        }
        DBG("read finish\n");
        if (strncasecmp("GET", method, 3) == 0) {
            char *file = path + 1; // 取出 客户端要访问的文件名
            http_request(file, cfd);
            disconnect(cfd, epfd);
        }
    }

}
*/

void do_read(int cfd, int epfd) {
    char line[1024] = {0};
    int len = get_line(cfd, line, sizeof(line));
    // 读取http协议 首行 : GET xxx http/1.1
    if (len == 0) {
        DBG("client close...\n");
        disconnect(cfd, epfd);
    }else {
        char method[16], path[256], protocol[16];
        sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);

        DBG("method: %s path: %s protocol: %s\n", method, path, protocol);
        while (1) {
            char buf[1024] = {0};
            len = get_line(cfd, buf, sizeof(buf));
            if (len == 1 || len == -1 || len == 0) break;
            DBG("buf :  %d %s \n", len, buf);
        }
        DBG("read finish\n");
        if (strncasecmp("GET", method, 3) == 0) {
            char *file = path + 1; // 取出 客户端要访问的文件名
            http_request(file, cfd);
            disconnect(cfd, epfd);
        }
    }

}


int init_listen_fd(int port, int epfd) {
    int lfd = socket_create(port);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev) < 0){
        perror("epoll_ctl error");
        exit(1);
    }
    return lfd;
}

void epoll_run(int port) {
    int epfd = epoll_create(MAXSIZE);
    if (epfd == -1) {
        perror("epoll_create error");
    }
    int lfd = init_listen_fd(port, epfd);
    
    struct epoll_event all_events[MAXSIZE];

    while (1) {
        int ret = epoll_wait(epfd, all_events, MAXSIZE, -1);
        if (ret == -1) {
            perror("epoll_wait error");
            exit(1);
        }
        for (int i = 0; i < ret; ++i) {
            if (!(all_events[i].events & EPOLLIN)) continue;
            int fd = all_events[i].data.fd;
            if (fd == lfd) {
                int cfd = do_accept(lfd, epfd);
            }else {
                do_read(fd, epfd);
            }
        }
    }

    return ;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage port path\n");
        return 0;
    }

    int port = atoi(argv[1]);

    int ret = chdir(argv[2]);
    if (ret != 0) {
        perror("chdir error");
        exit(1);
    }
    epoll_run(port);
        

    return 0;
}
