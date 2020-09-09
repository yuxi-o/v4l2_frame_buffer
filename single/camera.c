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

struct v4l2_requestbuffers reqbufs;
struct img bufs[REQBUFS_COUNT];

int camera_init(char *devpath, unsigned int *width, unsigned int *height, unsigned int *size, unsigned int *ismjpeg)
{
    int i;
    int fd;
    int ret;
    struct v4l2_format format;         //设定摄像头视频捕捉模式
    struct v4l2_capability capability; //查询设备属性

    fd = open(devpath, O_RDWR);
    if (fd == -1) {
        perror("camera->init");
        printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
        return -1;
    }

    /*查询设备属性*/
    ret = ioctl(fd, VIDIOC_QUERYCAP, &capability);
    if (ret == -1) {
        printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
        perror("camera->init");
        return -1;
    }

	printf("driver:\t\t%s\n", capability.driver);
	printf("card:\t\t%s\n", capability.card);
	printf("bus_info:\t%s\n", capability.bus_info);
	printf("version:\t%u.%u.%u\n", (capability.version>>16)&0xFF, (capability.version>>8)&0xFF, capability.version&0xFF);
	printf("capabilities:\t%x\n", capability.capabilities);
	printf("device_caps:\t%x\n", capability.device_caps);

    if(!(capability.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "camera->init: device can not support V4L2_CAP_VIDEO_CAPTURE\n");
        printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
        close(fd);
        return -1;
    }

    if(!(capability.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "camera->init: device can not support V4L2_CAP_STREAMING\n");
        printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
        close(fd);
        return -1;
    }

    /*获取当前摄像头所支持的所有视频输出格式*/
    struct v4l2_fmtdesc fmt1;
    memset(&fmt1, 0, sizeof(fmt1));
    fmt1.index = 0;            //初始化为0
    fmt1.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    //获取支持的格式
    while ((ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmt1)) == 0)
    {
        fmt1.index++;
        printf("PixelFormat: pixelformat = '%c%c%c%c', description = '%s' \n",
			fmt1.pixelformat & 0xFF, (fmt1.pixelformat >> 8) & 0xFF, (fmt1.pixelformat >> 16) & 0xFF, 
			(fmt1.pixelformat >> 24) & 0xFF, fmt1.description);
    }

    //设定摄像头捕获格式
	if (ismjpeg)
	{
		memset(&format, 0, sizeof(format));
		format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
		format.fmt.pix.width = *width;//PWIDTH; //640;
		format.fmt.pix.height = *height;//PHEIGHT; //480;
		format.fmt.pix.field = V4L2_FIELD_ANY;
		ret = ioctl(fd, VIDIOC_S_FMT, &format);
		if(ret == -1)
		{
			printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
			perror("camera_init");
		}
		else {
			fprintf(stdout, "camera->init: picture format is mjpeg\n");
			printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
			*ismjpeg = 1;
			goto get_fmt;
		}
	}

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.width = *width;//PWIDTH;//1280;//640;//0;
    format.fmt.pix.height = *height; //PHEIGHT;//720;//480;//0;
    format.fmt.pix.field = V4L2_FIELD_ANY;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    if(ret == -1) {
        perror("camera init");
        printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
        return -1;
    } else {
        *ismjpeg = 0;
        fprintf(stdout, "camera->init: picture format is yuyv\n");
        printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
    }

    /*查看当前视频设置的捕获模式*/
get_fmt:
    ret = ioctl(fd, VIDIOC_G_FMT, &format);
    if (ret == -1) {
        perror("camera init");
        printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
        return -1;
    }
	printf("Format type: %d\n", format.type);
	printf("Pixel: width: %d, height: %d, pixelformat: 0x%x, field: 0x%x\n" 
			"bytesperline: %d, sizeimage: %d, colorspace: %d, priv: 0x%x\n",
		format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.pixelformat,
		format.fmt.pix.field, format.fmt.pix.bytesperline, format.fmt.pix.sizeimage, 
		format.fmt.pix.colorspace, format.fmt.pix.priv);

    if (format.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG)
    {
        fprintf(stdout, "camera->init: picture format is mjpeg\n");
    }

    if (format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
    {
        fprintf(stdout, "camera->init: picture format is yuyv\n");
    }

    /*给图像数据分配存在于内核的物理内存空间*/
    memset(&reqbufs, 0, sizeof(struct v4l2_requestbuffers));
    reqbufs.count   = REQBUFS_COUNT;
    reqbufs.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory  = V4L2_MEMORY_MMAP;
    ret = ioctl(fd, VIDIOC_REQBUFS, &reqbufs);
    if (ret == -1) {
        perror("camera init");
        printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
        close(fd);
        return -1;
    }

    /*把内核的内存映射到用户空间，方便用户操作*/
    struct v4l2_buffer vbuf;
    for (i = 0; i < reqbufs.count; i++)
    {
        /*获得内核空间缓存的地址*/
        memset(&vbuf, 0, sizeof(struct v4l2_buffer));
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        vbuf.index = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &vbuf);
        if (ret == -1) {
            perror("camera init");
            printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
            close(fd);
            return -1;
        }

        /*内核空间映射到用户空间，并保存在bufs中*/
        bufs[i].length = vbuf.length;
        bufs[i].start = mmap(NULL, vbuf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, vbuf.m.offset);
        if (bufs[i].start == MAP_FAILED)
        {
            perror("camera init");
            printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
            close(fd);
            return -1;
        }

        /*放回内核空间缓存地址, 以便下次重新获取缓存队列的下一块缓存地址*/
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd, VIDIOC_QBUF, &vbuf);
        if (ret == -1) {
            perror("camera init");
            printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
            close(fd);
            return -1;
        }
    }

    *width = format.fmt.pix.width;
    *height = format.fmt.pix.height;
    *size = bufs[0].length;
    printf("camera init success\n");
    return fd;
}
//start
int camera_start(int fd)
{
    int ret;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    if (ret == -1) {
        perror("camera->start");
        return -1;
    }
    fprintf(stdout, "camera->start: start capture\n");

    return 0;
}
// 从队列中取出帧 
int camera_dqbuf(int fd, void **buf, unsigned int *size, unsigned int *index)
{
    int ret;
    fd_set fds;
    struct timeval timeout;
    struct v4l2_buffer vbuf;

    while (1) {
        FD_ZERO(&fds);
        //printf("fd = %d\n", fd);
        FD_SET(fd, &fds);
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(fd + 1, &fds, NULL, NULL, &timeout);
        if (ret == -1) {
            perror("camera->dqbuf");
            if (errno == EINTR)
                continue;
            else
                return -1;
        } else if (ret == 0) {
            fprintf(stderr, "camera->dqbuf: dequeue buffer timeout\n");
            continue;
        } else {
            vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            vbuf.memory = V4L2_MEMORY_MMAP;
            ret = ioctl(fd, VIDIOC_DQBUF, &vbuf);
            if (ret == -1) {
                perror("camera->dqbuf");
                return -1;
            }
            *buf = bufs[vbuf.index].start;
            *size = vbuf.bytesused;
            *index = vbuf.index;

            return 0;
        }
    }
}

// 把帧放入队列
int camera_eqbuf(int fd, unsigned int index)
{
    int ret;
    struct v4l2_buffer vbuf;

    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuf.memory = V4L2_MEMORY_MMAP;
    vbuf.index = index;
    ret = ioctl(fd, VIDIOC_QBUF, &vbuf);
    if (ret == -1) {
        perror("camera->eqbuf");
        return -1;
    }

    return 0;
}
//stop
int camera_stop(int fd)
{
    int ret;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if (ret == -1) {
        perror("camera->stop");
        return -1;
    }
    fprintf(stdout, "camera->stop: stop capture\n");

    return 0;
}
//exit
int camera_exit(int fd)
{
    int i;
    int ret;
    struct v4l2_buffer vbuf;

    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuf.memory = V4L2_MEMORY_MMAP;

    for (i = 0; i < reqbufs.count; i++) {
        ret = ioctl(fd, VIDIOC_DQBUF, &vbuf);
        if (ret == -1)
            break;
    }

    for (i = 0; i < reqbufs.count; i++)
        munmap(bufs[i].start, bufs[i].length);

    fprintf(stdout, "camera->exit: camera exit\n");

    return close(fd);

}

