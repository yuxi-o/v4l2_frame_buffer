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
#include "avilib.h"

#define PWIDTH	640 //1280	
#define PHEIGHT	480 //720

#define DEVNAME "/dev/video1"
#define AVIFILE_PREFIX "wb"

#define CONFIG_FRAME_SIZE	30
//#define CONFIG_SAVE_INTERVAL_SECOND	300
#define CONFIG_SAVE_INTERVAL_SECOND	60

typedef enum {
	CAMERA_STATE_CLOSE,
	CAMERA_STATE_CAP,
	CAMERA_STATE_ERR
} camera_state_t;

int gcamera_fd;
camera_state_t gcamera_state = CAMERA_STATE_CLOSE;
squeue_t gqueue;
unsigned char gframe_rgb24[PWIDTH * PHEIGHT *3];
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

static avi_t *get_avi(time_t *t)
{
	struct tm *local;
	avi_t *avifd = NULL;
	char buf[64];

	local = localtime(t);
	if(local == NULL){
		return NULL;
	}

	snprintf(buf, sizeof(buf), "%s%4d%02d%02d_%02d%02d%02d.avi", AVIFILE_PREFIX,
			local->tm_year + 1900, local->tm_mon+1, local->tm_mday,
			local->tm_hour, local->tm_min, local->tm_sec);
	avifd = AVI_open_output_file(buf);
	if(avifd == NULL){
		perror("Fail to open AVI");
		return NULL;
	}
	if (gis_mjpeg){
		AVI_set_video(avifd, PWIDTH, PHEIGHT, 30, "MJPG");
	} else {
		AVI_set_video(avifd, PWIDTH, PHEIGHT, 30, "YUYV422");
	}

	return avifd;
}

void * process_frame_thread(void *arg)
{
	char gbmp_name[32];
	squeue_data_t sdata;
	time_t t1, t2, t3;
    unsigned int width = PWIDTH;
    unsigned int height = PHEIGHT;
	unsigned int bmpsize = 0;
	int ret = 0;
	avi_t *avifd ;

	t1 = t2 = time(NULL);
	avifd = get_avi(&t1);
	if(avifd == NULL){
		return NULL;
	}

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

		ret = AVI_write_frame(avifd, sdata.pdata, sdata.length, 0);
		if(ret < 0)
		{
			printf("Warn: AVI_write_frame error\n");
			continue;
		}
#if 0
		if (gis_mjpeg)
		{
			jpeg_to_rgb24(gframe_rgb24, sdata.pdata, &width, &height, sdata.length);
		}
		else 
		{
			yuv422_to_rgb24(gframe_rgb24, sdata.pdata, width, height);
		}
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
		gframe_count++;
		if (gframe_count > 100){
			t3 = time(NULL);
			printf("[%lds] get %d frames (mjpeg: %d): %d bytes, width is %d, height is %d\n", t3-t1, gframe_count, gis_mjpeg, sdata.length, width, height);
			gframe_count=0;
			if((t3 - t2)> CONFIG_SAVE_INTERVAL_SECOND){
				AVI_close(avifd);
				avifd = get_avi(&t3);
				if(avifd == NULL){
					return NULL;
				}
				t2 = t3;
			}
		}

		squeue_data_destroy((void *)&sdata);
    }

	gcamera_state = CAMERA_STATE_ERR;
	printf("Camera process frame thread stop!\n");

	AVI_close(avifd);
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

