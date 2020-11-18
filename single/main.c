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

#define PWIDTH	640 //800 //1280
#define PHEIGHT	480 //600 // 720

#define DEVNAME "/dev/video1"

#define CONFIG_FRAME_SIZE	10

typedef enum {
	CAMERA_STATE_CLOSE,
	CAMERA_STATE_CAP,
	CAMERA_STATE_ERR
} camera_state_t;

int gcamera_fd;
camera_state_t gcamera_state = CAMERA_STATE_CLOSE;
squeue_t gqueue;
unsigned char gframe_rgb24[PWIDTH * PHEIGHT *3];
unsigned char gframe_yuv[PWIDTH * PHEIGHT *3];
unsigned char gframe_bmp[54 + PWIDTH * PHEIGHT *3];
unsigned int gis_mjpeg = 1; // set mjpeg format
unsigned int gwidth = PWIDTH;
unsigned int gheight = PHEIGHT;
unsigned int gframe_size = 0;
unsigned int gframe_count = 0;
unsigned int gindex = 0;

void* get_frame_thread(void *arg)
{
	int ret = -1;
	unsigned char *buf = NULL;
	unsigned int size = 0;

    /*开始捕获图像数据*/
    ret = camera_start(gcamera_fd);
	if (ret < 0)
	{
        printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
        printf("camera start failed!\n");
        exit(-1);
	}

    while(CAMERA_STATE_CAP == gcamera_state)
    {
/*		if(squeue_is_full(&gqueue))
		{
			printf("Warn: frame buffer queue is overflow!\n");
			usleep(10000);
			continue;
		}
*/
        /*把图像数据存放到用户缓存空间*/
        ret = camera_dqbuf(gcamera_fd,(void **)&buf, &size, &gindex);
		if (ret < 0)
		{
			perror("Camera dequeue fail");
			break;
		}

		ret = squeue_enqueue_ext(&gqueue, buf, size);
		if (ret < 0)
		{
			if (ret == -1) // queue overflow
			{
				camera_eqbuf(gcamera_fd, gindex);
//				printf("Warn: frame buffer queue is overflow! index:[%d]\n", gindex);
				usleep(10000);
				continue;
			}
			else 
			{
				perror("Camera enqueue fail");
				break;
			}
		}

        camera_eqbuf(gcamera_fd, gindex);
    }

	gcamera_state = CAMERA_STATE_ERR;
	printf("Camera get frame thread stop!\n");
	return NULL;
}

void * process_frame_thread(void *arg)
{
	char gbmp_name[32];
	squeue_data_t sdata;
	time_t t1, t2;
    unsigned int width = PWIDTH;
    unsigned int height = PHEIGHT;
	unsigned int bmpsize = 0;
	int ret = 0;
	FILE *fp;

	t1 = time(NULL);	
	sprintf(gbmp_name, "/tmp/%ld.h264", t1);
	fp = fopen(gbmp_name, "w");
	if(fp == NULL){
		printf("Create h264 file failed!\n");
		exit(-1);
	}

	h264_encode_start(width, height);
    while(CAMERA_STATE_CAP == gcamera_state)
//	while(1)
    {
/*		if(squeue_is_empty(&gqueue))
		{
			printf("Warn: frame buffer queue is empty!\n");
			usleep(10000);
			continue;
		}
*/
		ret = squeue_dequeue(&gqueue, &sdata);
		if (ret < 0)
		{
//			printf("Warn: frame buffer queue is empty!\n");
			usleep(10000);
			continue;
		}


		if (gis_mjpeg)
		{
			jpeg_to_rgb24(gframe_rgb24, sdata.pdata, &width, &height, sdata.length);
			unsigned int data_len = sdata.length;
			rgb24_to_yuv420p(gframe_yuv, gframe_rgb24, width, height, &data_len);
			h264_encode_frame_420p(gframe_yuv, width, height, data_len, fp);
		}
		else 
		{
			//yuv422_to_rgb24(gframe_rgb24, sdata.pdata, width, height);
			h264_encode_frame(sdata.pdata, width, height, sdata.length, fp);
		}
#if 0
		rgb24_to_bmp(gframe_bmp, gframe_rgb24, width, height, gis_mjpeg, &bmpsize);
		sprintf(gbmp_name, "%d.bmp", gframe_count);
		ret = write_file(gbmp_name, gframe_bmp, bmpsize);
		if (ret < 0)
		{
			printf("bmp save failed!\n");
			printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
			exit(-1);
		}
#endif

// 统计帧处理速度
#if 1 
		gframe_count++;
		if (gframe_count > 100 ){
			t2 = time(NULL);	
			printf("camera data (mjpeg: %d) is %d bytes, width is %d, height is %d\n", gis_mjpeg, sdata.length, width, height);
			printf("100 frame (%u) diff timestamp: %ld\n", gframe_count, t2-t1);
			gframe_count=0;
			break;
		}
#endif
		squeue_data_destroy((void *)&sdata);
    }

	gcamera_state = CAMERA_STATE_ERR;
	printf("Camera process frame thread stop!\n");

	h264_encode_stop();
	fflush(fp);
	fclose(fp);
	return NULL;
}

void signal_int_handle(int signo)
{
	if (gcamera_state == CAMERA_STATE_CLOSE)
	{
		return;
	}

	printf("Clean resource and exit!\n");
	gcamera_state = CAMERA_STATE_CLOSE;
	squeue_destroy(&gqueue, squeue_data_destroy);
    camera_stop(gcamera_fd);
    camera_exit(gcamera_fd);
}

int main(int argv,char ** argc)
{
	int ret = 0;
    char *dev_name=DEVNAME;
    pthread_t get_frame_td = 0;
    pthread_t process_frame_td = 0;

	signal(SIGINT, signal_int_handle);
	
	// 初始化camera
    gcamera_fd = camera_init(dev_name, &gwidth, &gheight, &gframe_size, &gis_mjpeg);
    if (gcamera_fd < 0)
    {
        printf("camera init is failed!\n");
        exit(-1);
    }
	gcamera_state = CAMERA_STATE_CAP;

	// 初始化队列
	ret = squeue_init(&gqueue, CONFIG_FRAME_SIZE);
	if (0 > ret)
	{
		perror("init queue failed!\n");
		exit(-1);
	}

	// 开启线程
    if ((0 != pthread_create(&get_frame_td, NULL, get_frame_thread, NULL))
	    || (0 != pthread_create(&process_frame_td, NULL, process_frame_thread, NULL)))
	{
		perror("pthread_create");
		exit(-1);
	}
	printf("thread id: [%lu], [%lu]\n", get_frame_td, process_frame_td);
    
	// 等待线程退出
	void *retval = NULL;
	if (0 != pthread_join(get_frame_td, &retval))
	{
		printf("get_frame_thread exit fault!\n");
		exit(-1);
	} else {
		printf("get_frame_thread exit with value %s\n", (char *)retval);
	}
	if (0 != pthread_join(process_frame_td, &retval))
	{
		printf("process_frame_thread exit fault!\n");
		exit(-1);
	} else {
		printf("process_frame_thread exit with value %s\n", (char *)retval);
	}

	signal_int_handle(0);
    return 0;
}

