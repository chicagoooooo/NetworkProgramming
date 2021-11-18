#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFSIZE 16384

void upload(int fd,char *buffer,int ret){
    char path[8192];
    char filename[8192];
    char *filenamestart=buffer;
    filenamestart=strstr(filenamestart,"filename=\"");
    filenamestart+=10;
    char *filenameend=strchr(filenamestart,'\"');
    strncpy(filename,filenamestart,filenameend-filenamestart);
    strcat(path,"uploadfiles/");
    strcat(path,filename);
    mkdir("uploadfiles",S_IRWXU| S_IRWXG | S_IROTH | S_IXOTH);
    chmod("uploadfiles",0777);
    int newfilefd = open(path, O_RDWR|O_CREAT, 0666);
    if(newfilefd==-1){
        printf("faill\n");
    }
    char *filesizeinfo=strstr(buffer,"Content-Length: ");
    int filesize=atoi(filesizeinfo+16);
    // printf("%d",filesize);
    char *p=buffer;
    int cnt=0;
    int cnt2=0;
    for(int i=0;i<17;i++){
        // printf("cnt+=%d\n",strchr(p,'\n')-p);
        cnt2+=strchr(p,'\n')-p+1;
        p=strchr(p,'\n');
        p++;
    }
    for(int i=0;i<4;i++){
        // printf("cnt+=%d\n",strchr(p,'\n')-p);
        cnt+=strchr(p,'\n')-p+1;
        cnt2+=strchr(p,'\n')-p+1;
        p=strchr(p,'\n');
        p++;
    }
    // printf("ret=%d\n",ret);
    // printf("cnt==%d\n",cnt);
    int readlen=filesize-63-cnt;
    // printf("readlen = %d\n",readlen);
    int writebytes;
    // printf("cnt2 = %d\n",cnt2);
    // printf("cnt = %d\n",cnt);
    if(readlen < BUFSIZE - cnt2){
        writebytes = write(newfilefd,buffer+cnt2,readlen);
    }else{
        writebytes = write(newfilefd,buffer+cnt2,BUFSIZE - cnt2);
    }
    
    readlen-=writebytes;
    // printf("%d\n",writebytes);
    while(readlen>0){
        if(readlen<BUFSIZE){
            read(fd,buffer,readlen);
            writebytes = write(newfilefd,buffer,readlen);
            readlen-=writebytes;
            // printf("%s",buffer);
            // printf("readlen = %d\n",readlen);
        }else{
            read(fd,buffer,BUFSIZE);
            writebytes = write(newfilefd,buffer,BUFSIZE);
            readlen-=writebytes;
            // printf("%s",buffer);
            // printf("readlen = %d\n",readlen);
        }
    }
    close(newfilefd);
}


struct {
    char *ext;
    char *filetype;
} extensions [] = {
    {"gif", "image/gif" },
    {"jpg", "image/jpeg"},
    {"jpeg","image/jpeg"},
    {"png", "image/png" },
    {"zip", "image/zip" },
    {"gz",  "image/gz"  },
    {"tar", "image/tar" },
    {"htm", "text/html" },
    {"html","text/html" },
    {"exe","text/plain" },
    {0,0} };


void handle_socket(int fd)
{
    int j, file_fd, buflen, len;
    long i, ret;
    char * fstr;
    static char buffer[BUFSIZE+1];

    ret = read(fd,buffer,BUFSIZE);   /* 讀取瀏覽器要求 */
    if (ret==0||ret==-1) {
     /* 網路連線有問題，所以結束行程 */
        exit(3);
    }
    if (!strncmp(buffer,"GET ",4) || !strncmp(buffer,"get ",4)){
        /* 程式技巧：在讀取到的字串結尾補空字元，方便後續程式判斷結尾 */
        if (ret>0&&ret<BUFSIZE)
            buffer[ret] = 0;
        else
            buffer[0] = 0;

        /* 移除換行字元 */
        for (i=0;i<ret;i++) 
            if (buffer[i]=='\r'||buffer[i]=='\n')
                buffer[i] = 0;

        /* 我們要把 GET /index.html HTTP/1.0 後面的 HTTP/1.0 用空字元隔開 */
        for(i=4;i<BUFSIZE;i++) {
            if(buffer[i] == ' ') {
                buffer[i] = 0;
                break;
            }
        }

        /* 檔掉回上層目錄的路徑『..』 */
        for (j=0;j<i-1;j++)
            if (buffer[j]=='.'&&buffer[j+1]=='.')
                exit(3);

        /* 當客戶端要求根目錄時讀取 index.html */
        if (!strncmp(&buffer[0],"GET /\0",6)||!strncmp(&buffer[0],"get /\0",6) )
            strcpy(buffer,"GET /index.html\0");
        // printf("%s\n",buffer);
        /* 檢查客戶端所要求的檔案格式 */
        buflen = strlen(buffer);
        fstr = (char *)0;

        for(i=0;extensions[i].ext!=0;i++) {
            len = strlen(extensions[i].ext);
            if(!strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
                fstr = extensions[i].filetype;
                break;
            }
        }

        /* 檔案格式不支援 */
        if(fstr == 0) {
            fstr = extensions[i-1].filetype;
        }
        /* 開啟檔案 */
        if((file_fd=open(&buffer[5],O_RDONLY))==-1){
            // printf("open error : %s\n",buffer+5);
            write(fd, "Failed to open file", 19);
        }
        

        /* 傳回瀏覽器成功碼 200 和內容的格式 */
        sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
        write(fd,buffer,strlen(buffer));


        /* 讀取檔案內容輸出到客戶端瀏覽器 */
        while ((ret=read(file_fd, buffer, BUFSIZE))>0) {
            write(fd,buffer,ret);
        }
        // printf("%s",buffer);
        // printf("get is down\n");

    }else if(!strncmp(buffer,"POST",4) || !strncmp(buffer,"post",4)){
        upload(fd,buffer,ret);

        memset(buffer,'\0',8192);
        if((file_fd = open("index.html", O_RDONLY))==-1){
        	write(fd, "Failed to open file\n", 20);
		}

		sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
        write(fd,buffer,strlen(buffer));

		while ((ret=read(file_fd, buffer, BUFSIZE))>0){
            write(fd,buffer,ret);
        }
        // printf("%s",buffer);
        // printf("post is down\n");

    }else{
        exit(3);
    }
    
    
    exit(1);
}

void sigchld(int signo)
{
	pid_t pid;
	
	while((pid=waitpid(-1, NULL, WNOHANG))>0);
}

int main(int argc, char **argv)
{
    int i, pid, listenfd, socketfd;
    socklen_t length;
    static struct sockaddr_in cli_addr;
    static struct sockaddr_in serv_addr;

    /* 使用 /tmp 當網站根目錄 */
    if(chdir("./tmp") == -1){ 
        printf("ERROR: Can't Change to directory %s\n",argv[2]);
        exit(4);
    }


    /* 讓父行程不必等待子行程結束  let init manage child process*/ 
    signal(SIGCLD, SIG_IGN);

    /* 開啟網路 Socket */
    if ((listenfd=socket(AF_INET, SOCK_STREAM,0))<0)
        exit(3);

    /* 網路連線設定 */
    serv_addr.sin_family = AF_INET;
    /* 使用任何在本機的對外 IP */
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* 使用 80 Port */
    serv_addr.sin_port = htons(80);

    /* 開啟網路監聽器 */
    if (bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
        printf("bind error\n");
        exit(3);
    }
        

    /* 開始監聽網路 */
    if (listen(listenfd,64)<0)
        exit(3);

    while(1) {
        length = sizeof(cli_addr);
        /* 等待客戶端連線 */
        if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length))<0)
            exit(3);

        /* 分出子行程處理要求 */
        if ((pid = fork()) < 0) {
            exit(3);
        } else {
            if (pid == 0) {  /* 子行程 */
                close(listenfd);
                handle_socket(socketfd);
            } else { /* 父行程 */
                close(socketfd);
            }
        }
    }
}