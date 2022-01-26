/*************************************************************************
 *
	> File Name: http.c
	> Author:liu 
	> Mail: 
	> Created Time: Tue 30 Nov 2021 05:05:36 PM CST
 ************************************************************************/

#include "head.h"
#include <sys/stat.h>
#include <dirent.h>
#include <DBG.h>
#include <signal.h>

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

// 由于 服务器发送给 浏览器 的数据默认是按照 iso-8859-1 编码（可以通过设置修改 服务器 编码方式），
// 浏览器接收到数据后按照当前页面的的显示解码 进行解码后显示， 如果浏览器的当前页面编码 不是服务器的编码， 就出现乱码
// 而服务器使用 iso-885959-1编码中文字符， 浏览器接收到的数据一定是乱码， 因为 iso-8859-1 不包括中文字符

// 十六进制转换十进制
int hexit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return 0;
}

// 编码 ：汉字
void decode_str(char *to, char *from) {
    for (; *from != '\0'; ++to, ++from) {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
            *to = hexit(from[1]) * 16 + hexit(from[2]);
            from += 2;
        }else {
            *to = *from;
        }
    }
    *to = '\0';
}

/*
 *  这里的内容是处理%20之类的东西！是"解码"过程。
 *  %20 URL编码中的‘ ’(space)
 *  %21 '!' %22 '"' %23 '#' %24 '$'
 *  %25 '%' %26 '&' %27 ''' %28 '('......
 *  相关知识html中的‘ ’(space)是&nbsp
 */

// 解码 对 数据进行解码
void encode_str(char* to, int tosize, const char* from) {
    int tolen;

    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0) {
            *to = *from;
            ++to;
            ++tolen;
        }else {
            sprintf(to, "%%%2x", (int) *from & 0xff);
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}

const char *get_file_type(const char *name)
{
    char* dot;

    // 自右向左查找‘.’字符, 如不存在返回NULL
    dot = strrchr(name, '.');   
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav" ) == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}



int do_accept(int lfd, int epfd) {
    DBG("do_accept\n");
    struct sockaddr_in cli_addr;
    struct epoll_event ev;
    memset(&cli_addr, 0, sizeof(cli_addr));
    int size = sizeof(cli_addr);
    int cfd = accept(lfd, (struct sockaddr *)&cli_addr, &size);
    if (cfd == -1) {
        perror("accept error");
        exit(1);
    }
    /*
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    ev.events = EPOLLIN | EPOLLET;
    */
    ev.events = EPOLLIN;
    ev.data.fd = cfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev) < 0) {
        perror("epoll_ctl error");
        exit(1);
    }
    return cfd;
}

void send_error(int cfd, int status, char *title, char *text) {
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

    sprintf(buf, "<html><head><title> 目录名: %s</title></head>", dirname);
    sprintf(buf + strlen(buf), "<body><h1> current dirname: %s</h1><table>", dirname);

    char enstr[1024] = {0};
    char path[1024] = {0};

    struct dirent **ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort);
    /*
     *  int scandir(const char *dirp, struct dirent ***namelist,
              int (*filter)(const struct dirent *),
              int (*compar)(const struct dirent **, const struct dirent **));

     struct dirent {
               ino_t          d_ino;        Inode number 
               off_t          d_off;        Not an offset; see below 
               unsigned short d_reclen;     Length of this record 
               unsigned char  d_type;       Type of file; not supported
                                              by all filesystem types 
               char           d_name[256];  Null-terminated filename 
    };
    */

    for (i = 0; i < num; ++i) {
        char *name = ptr[i]->d_name;
        DBG("scandir name = %s\n", name);
        // 生成 %E5 之类的 编码格式

        sprintf(path, "%s/%s",dirname, name);
        DBG("path = %s\n", path);
        struct stat st;
        stat(path, &st);

        //encode_str(enstr, sizeof(enstr), name);
        strcpy(enstr, name);
        DBG("scandir name = %s\n", enstr);

        if (S_ISREG(st.st_mode)) {
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>", enstr, name, (long)st.st_size);
            //若href后面跟着的不是绝对路径，html 会将URl 当成一个相对路径处理， 
            //即 href 赋值时会自动加上当前页面的 url（父集相对路径） 作为前缀， 
        }else if (S_ISDIR(st.st_mode)) {
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>", enstr, name, (long)st.st_size);
        }
        ret = send(cfd, buf, strlen(buf), 0);
        DBG("dir buf %s\n", buf);
        if (ret == -1) {
            if (errno == EAGAIN) {
                perror("send error");
                continue;
            }else if (errno == EINTR) {
                perror("send error");
                continue;
            }else {
                perror("send error");
                exit(1);
            }
        }
        memset(buf, 0, sizeof(buf));
    }
    sprintf(buf + strlen(buf), "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);
    DBG("dir message send ok!\n");
}




void send_file(const char *file, int cfd) {
    DBG("send_file\n");
    int fd = open(file, O_RDONLY);
    int n = 0, ret = 0;
    char buf[4096] = {0};
    if (fd == -1) {
        // 404 错误页面
        send_error(cfd, 404, "Not Found", "No such file or dirctory");
        exit(1);
    }
    while ((n = read(fd, buf, sizeof(buf))) >= 0) {
        if(n < 0) {
            perror("read error");
            DBG("error no %d\n", errno);
            exit(1);
        }
        if (n == 0) {
            DBG("close fd\n");
            return ;
        }
        ret = send(cfd, buf, n, 0);
        if (ret == -1) {
            if (errno == EAGAIN) {
                perror("send error:");
                continue;
            }else if (errno == EINTR) {
                perror("send error:");
                continue;
            }else {
                perror("send error");
                exit(1);
            }
        }
        //sleep(1);
        DBG("send file %d size\n", n);
    }
    if (n == -1)  {
        perror("read file error");
        exit(1);
    }
    close(fd);
}



void send_respond_head(int cfd, int no, const char *disp, const char *type, long len) {
    DBG("send_respond\n");
    char buf[1024] = {0};
    sprintf(buf, "HTTP/1.1 %d %s\r\n", no, disp);
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type:%s\r\n", type);
    sprintf(buf + strlen(buf), "Content-Length:%ld\r\n", len);
    send(cfd, buf, strlen(buf), 0);
    send(cfd, "\r\n", 2, 0);

}

// 处理http 请求  判断文件是否存在， 再回复
void http_request(const char *line, int cfd) {
    DBG("deel http_request\n");
    char method[16], path[256], protocol[16];
    sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);

    DBG("method: %s path: %s protocol: %s\n", method, path, protocol);

    // 将不能识别的中文乱码 转换成中文 
    //decode_str(path, path);

    char *file = path + 1;
    if (strcmp(path, "/") == 0) {
        file = "./";
    }

    struct stat sbuf;
    int ret = stat(file, &sbuf);
    if (ret != 0) {
        // 回发浏览器 404 错误页面
        send_error(cfd, 404, "Not Found", "No such file or dirctory");
        DBG("client request error\n");
        exit(1);
    }
    if (S_ISDIR(sbuf.st_mode)) {
        send_respond_head(cfd, 200, "OK", get_file_type(".html"), -1);

        send_dir(file, cfd);
    }else if (S_ISREG(sbuf.st_mode)) {
        // 是一个普通文件 打开文件 回复给客户端
        // 编辑http 协议应答
        send_respond_head(cfd, 200, "OK", get_file_type(file), sbuf.st_size);
        // 回发文件内容
        send_file(file, cfd);
        
    }
}


void do_read(int cfd, int epfd) {
    char line[1024] = {0};

    int len = get_line(cfd, line, sizeof(line));
    // 读取http协议 首行 : GET xxx http/1.1
    if (len == 0) {
        DBG("client close...\n");
        disconnect(cfd, epfd);
    }else {
        while (1) {
            char buf[1024] = {0};
            len = get_line(cfd, buf, sizeof(buf));
            if (buf[0] == '\n' || len == -1) break;
            DBG("buf :  %d %s \n", len, buf);
        }
    }
    DBG("read finish\n");
    if (strncasecmp("GET", line, 3) == 0) {
        http_request(line, cfd);
        disconnect(cfd, epfd);
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
                DBG("before read \n");
                do_read(fd, epfd);
                DBG("after read \n");
            }
        }
    }

    return ;
}

void sig_cath(int signo) {
    DBG("sigpipe: %d\n", signo);
    sleep(50);
    return ;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage port path\n");
        return 0;
    }
    signal(SIGPIPE, sig_cath); // 客户端 断开连接， 服务端继续发送数据导致， client sigpipe 信号 ？？

    int port = atoi(argv[1]);

    int ret = chdir(argv[2]);
    if (ret != 0) {
        perror("chdir error");
        exit(1);
    }
    epoll_run(port);
        

    return 0;
}
