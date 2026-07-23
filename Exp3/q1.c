#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>

double ms(struct timeval a, struct timeval b){
    return (b.tv_sec-a.tv_sec)*1000.0 + (b.tv_usec-a.tv_usec)/1000.0;
}

int main(){
    char url[2048], host[512], path[1024]={0}, req[2048], buf[4096], fname[1024];
    int port=80;

    printf("Enter URL: ");
    fgets(url, sizeof(url), stdin);
    url[strcspn(url,"\r\n")]=0;

    char *p = url;
    if(!strncmp(p,"http://",7)) p+=7;

    char *slash = strchr(p,'/');
    char hp[512];
    strncpy(hp, p, slash? (size_t)(slash-p): strlen(p));
    hp[slash? slash-p: strlen(p)]=0;

    char *colon = strchr(hp,':');
    if(colon){ *colon=0; strcpy(host,hp); port=atoi(colon+1); }
    else strcpy(host,hp);

    strcpy(path, slash? slash: "/");

    char *name = strrchr(path,'/');
    name = name? name+1: (char*)path;
    strcpy(fname, "downloaded_");
    strcat(fname, *name? name: "index.html");

    struct hostent *server = gethostbyname(host);
    if(!server){ return 1; }

    struct sockaddr_in addr={0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct timeval t0,t1,t2,t3;
    gettimeofday(&t0,NULL);
    if(connect(sock,(struct sockaddr*)&addr,sizeof(addr))<0){ return 1; }
    gettimeofday(&t1,NULL);

    snprintf(req,sizeof(req),"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",path,host);
    send(sock, req, strlen(req), 0);

    FILE *f = fopen(fname,"wb");
    long total=0, body=0;
    int got_header=0, first=0;
    char hdr[8192]; size_t hlen=0;
    ssize_t n;

    while((n=recv(sock,buf,sizeof(buf),0))>0){
        if(!first){ gettimeofday(&t2,NULL); first=1; }
        total+=n;
        if(!got_header){
            size_t c=n;
            if(hlen+c>sizeof(hdr)-1) c=sizeof(hdr)-1-hlen;
            memcpy(hdr+hlen,buf,c); hlen+=c; hdr[hlen]=0;
            char *end=strstr(hdr,"\r\n\r\n");
            if(end){
                got_header=1;
                size_t hb=(end-hdr)+4;
                size_t rem=hlen-hb;
                if(rem>0){ fwrite(hdr+hb,1,rem,f); body+=rem; }
            }
        } else {
            fwrite(buf,1,n,f);
            body+=n;
        }
    }
    gettimeofday(&t3,NULL);
    fclose(f);
    close(sock);

    double resp = ms(t1,t2), dl = ms(t2,t3), tot = ms(t0,t3);
    double kbps = dl>0? (body/1024.0)/(dl/1000.0): 0;

    printf("Saved to: %s\n", fname);
    printf("Response time: %.2f ms\n", resp);
    printf("Download time: %.2f ms\n", dl);
    printf("Total time: %.2f ms\n", tot);
    printf("Data size: %ld bytes\n", body);
    printf("Throughput: %.2f KB/s\n", kbps);

    return 0;
}