#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <signal.h>
#include <time.h>
#include "camera.h"
#include "image_process.h"
#include "queue.h"

#define PWIDTH	1280	
#define PHEIGHT	720

#define DEVNAME "/dev/video0"

#define CONFIG_FRAME_SIZE	30

pthread_mutex_t mutex;

int camera_fd;
unsigned int gframe_size;
unsigned int gframe_count = 0;
static char gbmp_name[32];
//static char  video_p[1024 * 128];
//static unsigned char video_p[PWIDTH * PHEIGHT *3];
static unsigned char gframe_rgb24[PWIDTH * PHEIGHT *3];
static unsigned char gframe_bmp[54 + PWIDTH * PHEIGHT *3];
squeue_t gqueue;
unsigned int ismjpeg = 1; // set mjpeg format

void * get_frame_thread(void *arg)
{
    char *dev_name=DEVNAME;
    unsigned int width = PWIDTH;
    unsigned int height = PHEIGHT;
//    unsigned int ismjpeg = 1; // set mjpeg format
    unsigned int index;

    camera_fd=camera_init(dev_name, &width, &height, &gframe_size, &ismjpeg);
    if (-1 == camera_fd)
    {
        printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
        printf("camera init is failed!\n");
        exit(-1);
    }

    /*开始捕获图像数据*/
    printf("camera_fd = %d\n", camera_fd);
    camera_start(camera_fd);

    while(1)
    {
        unsigned char *jpegbuf = NULL;
        unsigned int jpegsize = 0;

		if(squeue_is_full(&gqueue))
		{
			printf("Warn: frame buffer queue is overflow!\n");
			usleep(10000);
			continue;
		}

        /*把图像数据存放到用户缓存空间*/
        camera_dqbuf(camera_fd,(void **)&jpegbuf,&jpegsize,&index);
        pthread_mutex_lock(&mutex);
//        gframe_size = jpegsize;
//       memcpy(video_p, jpegbuf, jpegsize);
		squeue_enqueue_ext(&gqueue, jpegbuf, jpegsize);
        camera_eqbuf(camera_fd,index);
    }
}

void * process_frame_thread(void *arg)
{
    unsigned int width = PWIDTH;
    unsigned int height = PHEIGHT;
	time_t t1, t2;
	int ret = 0;

	t1 = time(NULL);	
    while(1)
    {
        unsigned char *jpegbuf = NULL;
        unsigned int jpegsize = 0;
		unsigned int bmpsize = 0;
		squeue_data_t sdata;

		if(squeue_is_empty(&gqueue))
		{
			printf("Warn: frame buffer queue is empty!\n");
			usleep(10000);
			continue;
		}

        pthread_mutex_lock(&mutex);
		squeue_dequeue(&gqueue, &sdata);
        pthread_mutex_unlock(&mutex);

		jpegbuf = sdata.pdata;
		jpegsize = sdata.length;
		if (ismjpeg)
		{

			jpeg_to_rgb24(gframe_rgb24, jpegbuf, &width, &height, jpegsize);
		}
		else 
		{
			yuv422_to_rgb24(gframe_rgb24, jpegbuf, width, height);
		}
		rgb24_to_bmp(gframe_bmp, gframe_rgb24, width, height, ismjpeg, &bmpsize);
		sprintf(gbmp_name, "%d.bmp", gframe_count);
		ret = write_file(gbmp_name, gframe_bmp, bmpsize);
		if (ret < 0)
		{
			printf("bmp save failed!\n");
			printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
			exit(-1);
		}

#if 1 
		gframe_count++;
		if (gframe_count > 100 ){
			t2 = time(NULL);	
//			printf("camera data (mjpeg: %d) is %d bytes, index is %d, width is %d, height is %d\n", ismjpeg, jpegsize, index, width, height);
			printf("100 frame (%u) diff timestamp: %ld\n", gframe_count, t2-t1);
			gframe_count=0;
		}
#endif
		squeue_data_destroy((void *)&sdata);
    }
}

int main(int argv,char ** argc)
{
	int ret = 0;

    signal(SIGPIPE, SIG_IGN);
	ret = squeue_init(&gqueue, CONFIG_FRAME_SIZE);
	if (0 > ret)
	{
		perror("init queue failed!\n");
		exit(-1);
	}

    pthread_t get_frame_td = 0;
    pthread_t process_frame_td = 0;
    pthread_create(&get_frame_td, NULL, get_frame_thread, NULL);
    pthread_create(&process_frame_td, NULL, process_frame_thread, NULL);
    if (0 > pthread_mutex_init(&mutex, NULL))
    {
        perror("pthread_mutex_init");
        exit(-1);
    }

#if 0
    sleep(2);
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in serveraddr;
    memset(&serveraddr,0,sizeof(serveraddr));
    serveraddr.sin_port=htons(4096);
    serveraddr.sin_addr.s_addr=htonl(INADDR_ANY);
    serveraddr.sin_family=AF_INET;
    int opt = 0;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(ret < 0)
        perror("set sock option error");

    ret=bind(sockfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr));
    if(ret<0)
    {
        perror("bind failed");
        return -1;
    }

    ret=listen(sockfd,5);
    if(ret<0)
    {
        perror("listen failed");
        return -1;
    }


    struct sockaddr_in client_address;
    socklen_t client_socket_lenth = sizeof(struct sockaddr_in);
    while(1)
    {
        int client_fd=accept(sockfd,(struct sockaddr*)&client_address,&client_socket_lenth);
        char* client_ip_address=inet_ntoa(client_address.sin_addr);
        short client_port=ntohs(client_address.sin_port);
        printf("%s \n  %d\n",client_ip_address,client_port);

        while(1)
        {
            char client_buf[256]={0};
            int recvbytes;
            /*接受客户端的图像数据获取请求*/
            recvbytes=recv(client_fd,client_buf,sizeof(client_buf),0);
            if(recvbytes==0)
            {
                printf("client exit");
                break;
            }else if(recvbytes<0){
                perror("recv failed");
                close(client_fd);
                break;
            }
            if(strcmp(client_buf,"request video")==0)
            {
                printf("client request video\n");
                /*
                   FILE * fp = fopen("t.jpg", "w+");
                   fwrite(video_p, gframe_size, 1, fp);
                   fclose(fp);
                 */
                char size_buf[10] = {0};
                pthread_mutex_lock(&mutex);
                sprintf(size_buf,"%d",gframe_size);
                printf("%s\n",size_buf);
                unsigned int send_ret=0;
                /*发送图像数据的大小给客户端*/
                int ret = send(client_fd,size_buf,sizeof(size_buf),0);
                if(ret < 0)
                {
                    perror("send picture to server failed");
                    pthread_mutex_unlock(&mutex);
                    break;
                }
                /*发送图像数据给客户端*/
                ret = send_ret = send(client_fd,video_p, gframe_size, 0);
                if(ret < 0)
                {
                    perror("send picture to server failed");
                    pthread_mutex_unlock(&mutex);
                    break;
                }
                printf("send %d bytes\n", send_ret);
                pthread_mutex_unlock(&mutex);
            }
        }
        close(client_fd);
    }
    close(sockfd);
#endif
	void *retval = NULL;
	if (0 != pthread_join(get_frame_td, &retval))
	{
		printf("get_frame_thread exit fault!\n");
//		exit(EXIT_FAILURE);
		ret = -1;
	} else {
		printf("get_frame_thread exit with value %s\n", (char *)retval);
		free(retval);
	}
	if (0 != pthread_join(process_frame_td, &retval))
	{
		printf("process_frame_thread exit fault!\n");
//		exit(EXIT_FAILURE);
		ret = -1;
	} else {
		printf("process_frame_thread exit with value %s\n", (char *)retval);
		free(retval);
	}

	squeue_destroy(&gqueue, squeue_data_destroy);
    camera_stop(camera_fd);
    camera_exit(camera_fd);

    return ret;
}

