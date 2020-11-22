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

#define REQBUFS_COUNT  10 
#define CONFIG_CAPTURE_FPS 30

struct img {
    void * start;
    unsigned int length;
};

struct img bufs[REQBUFS_COUNT];
struct v4l2_requestbuffers reqbufs;

int camera_init(char *devpath, unsigned int *width, unsigned int *height, unsigned int *size, unsigned int *ismjpeg)
{
    int i;
    int fd;
    int ret;
    struct v4l2_format format;         //�趨����ͷ��Ƶ��׽ģʽ
    struct v4l2_capability capability; //��ѯ�豸����

    fd = open(devpath, O_RDWR);
    if (fd == -1) {
		printf("camera->init: Open %s failed.\n", devpath);
        return -1;
    }
    /* ���òɼ���Դ */ // ��ʡ��
    struct v4l2_input input;
    input.index = 0;
    if (ioctl(fd, VIDIOC_S_INPUT, &input)) {
        printf("camera->init: set input failed.\n");
    }

    /*��ѯ�豸����*/
    ret = ioctl(fd, VIDIOC_QUERYCAP, &capability);
    if (ret == -1) {
        printf("camera->init: querycap failed.\n");
        close(fd);
        return -1;
    }

	printf("driver:\t\t%s\n", capability.driver);
	printf("card:\t\t%s\n", capability.card);
	printf("bus_info:\t%s\n", capability.bus_info);
	printf("version:\t%u.%u.%u\n", (capability.version>>16)&0xFF, (capability.version>>8)&0xFF, capability.version&0xFF);
	printf("capabilities:\t%x\n", capability.capabilities);
	printf("device_caps:\t%x\n", capability.device_caps);

    if(!(capability.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        printf("camera->init: camera can not support V4L2_CAP_VIDEO_CAPTURE\n");
        close(fd);
        return -1;
    }

    if(!(capability.capabilities & V4L2_CAP_STREAMING)) {
        printf("camera->init: camera can not support V4L2_CAP_STREAMING\n");
        close(fd);
        return -1;
    }

    /*��ȡ��ǰ����ͷ��֧�ֵ�������Ƶ�����ʽ*/
    struct v4l2_fmtdesc fmt1;
	struct v4l2_frmsizeenum fsize;
    memset(&fmt1, 0, sizeof(fmt1));
    fmt1.index = 0;            //��ʼ��Ϊ0
    fmt1.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while ((ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmt1)) == 0)
    {
        printf("PixelFormat: pixelformat = '%c%c%c%c', description = '%s' \n",
			fmt1.pixelformat & 0xFF, (fmt1.pixelformat >> 8) & 0xFF, (fmt1.pixelformat >> 16) & 0xFF, 
			(fmt1.pixelformat >> 24) & 0xFF, fmt1.description);
        fmt1.index++;

		//��ȡframe sizes
		fsize.index = 0;
		fsize.pixel_format = fmt1.pixelformat;
		while ((ret = ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0)
		{
			fsize.index++;
			if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
			{
				printf("    discrete: %ux%u:   ", fsize.discrete.width, fsize.discrete.height);
				// enumerate frame rates
				struct v4l2_frmivalenum fival;
				fival.index = 0;
				fival.pixel_format = fmt1.pixelformat;
				fival.width = fsize.discrete.width;
				fival.height = fsize.discrete.height;
				while ((ret = ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0)
				{
					fival.index++;
					if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
					{
						printf("%u/%u ", fival.discrete.numerator, fival.discrete.denominator);
					}
					else
						printf("I only handle discrete frame intervals...\n");
				}
				printf("\n");
			}
			else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)
			{
				printf("  continuous: %ux%u to %ux%u\n",
					fsize.stepwise.min_width, fsize.stepwise.min_height,
					fsize.stepwise.max_width, fsize.stepwise.max_height);
			}
			else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
			{
				printf("  stepwise: %ux%u to %ux%u step %ux%u\n",
					fsize.stepwise.min_width,  fsize.stepwise.min_height,
					fsize.stepwise.max_width,  fsize.stepwise.max_height,
					fsize.stepwise.step_width, fsize.stepwise.step_height);
			}
			else
			{
				printf("  fsize.type not supported: %d\n", fsize.type);
			}
		}
	}

    //�趨����ͷ�����ʽ
	if (*ismjpeg)
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
			printf("camera->init: set V4L2_PIX_FMT_MJPEG failed!\n");
		}
		else {
			printf("camera->init: picture format is mjpeg\n");
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
		printf("camera->init: set V4L2_PIX_FMT_YUYV failed!\n");
        return -1;
    } else {
        *ismjpeg = 0;
		printf("camera->init: picture format is yuyu.\n");
    }

    /*�鿴��ǰ��Ƶ���õĲ���ģʽ*/
get_fmt:
    ret = ioctl(fd, VIDIOC_G_FMT, &format);
    if (ret == -1) {
		printf("camera->init: get format failed!\n");
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
		printf("camera->init: picture format is mjpeg\n");
    }

    if (format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
    {
		printf("camera->init: picture format is yuyu.\n");
    }

#if 1
	/* ������Ƶ����Ϣ */
	struct v4l2_streamparm streamparam;
	memset(&streamparam, 0, sizeof(struct v4l2_streamparm));
	streamparam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 
	/* ֡�ʷ�ĸ �������� */
	streamparam.parm.capture.timeperframe.denominator = CONFIG_CAPTURE_FPS;
	streamparam.parm.capture.timeperframe.numerator = 1;
	if(ioctl(fd, VIDIOC_S_PARM, &streamparam) == -1) {
		printf("camera->init: set fps failed");
        close(fd);
        return -1;
	}

	/* ��ȡ��Ƶ����Ϣ */
	streamparam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 
	ret = ioctl(fd, VIDIOC_G_PARM, &streamparam);
	if(ret) {
		printf("camera->init: Get stream info failed");
        close(fd);
        return -1;
	}
	printf("After setting stream params:\n");
	printf("Capability: %u\n", streamparam.parm.capture.capability);
	printf("Capturemode: %u\n", streamparam.parm.capture.capturemode);
	printf("Extendedmode: %u\n", streamparam.parm.capture.extendedmode);
	printf("Timeperframe denominator: %u\n", streamparam.parm.capture.timeperframe.denominator);
	printf("Timeperframe numerator: %u\n", streamparam.parm.capture.timeperframe.numerator);
	/* �����Զ��ع� */ // ��ʡ��
	struct v4l2_control ctrl;
	ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
	ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
	if(ret) {
		printf("camera->init: set exposure failed!\n");
	}
#endif

    /*��ͼ�����ݷ���������ں˵������ڴ�ռ�*/
    memset(&reqbufs, 0, sizeof(struct v4l2_requestbuffers));
    reqbufs.count   = REQBUFS_COUNT;
    reqbufs.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory  = V4L2_MEMORY_MMAP;
    ret = ioctl(fd, VIDIOC_REQBUFS, &reqbufs);
    if (ret == -1) {
		printf("camera->init: reqbufs failed!\n");
        close(fd);
        return -1;
    }

    /*���ں˵��ڴ�ӳ�䵽�û��ռ䣬�����û�����*/
    struct v4l2_buffer vbuf;
    for (i = 0; i < reqbufs.count; i++)
    {
        /*����ں˿ռ仺��ĵ�ַ*/
        memset(&vbuf, 0, sizeof(struct v4l2_buffer));
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        vbuf.index = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &vbuf);
        if (ret == -1) {
			printf("camera->init: querybuf failed!\n");
            close(fd);
            return -1;
        }

        /*�ں˿ռ�ӳ�䵽�û��ռ䣬��������bufs��*/
        bufs[i].length = vbuf.length;
        bufs[i].start = mmap(NULL, vbuf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, vbuf.m.offset);
        if (bufs[i].start == MAP_FAILED)
        {
			printf("camera->init: mmap failed!\n");
            close(fd);
            return -1;
        }

        /*�Ż��ں˿ռ仺���ַ, �Ա��´����»�ȡ������е���һ�黺���ַ*/
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd, VIDIOC_QBUF, &vbuf);
        if (ret == -1) {
			printf("camera->init: qbuf failed!\n");
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
        printf("camera->start: streamon error!\n");
        return -1;
    }
    printf("camera->start: start capture\n");

    return 0;
}

// �Ӷ�����ȡ��֡ 
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
            printf("camera->dqbuf: select error!\n");
            if (errno == EINTR)
                continue;
            else
                return -1;
        } else if (ret == 0) {
            printf("camera->dqbuf: dequeue buffer timeout\n");
            continue;
        } else {
            vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            vbuf.memory = V4L2_MEMORY_MMAP;
            ret = ioctl(fd, VIDIOC_DQBUF, &vbuf);
            if (ret == -1) {
                printf("camera->dqbuf:dqbuf error!\n");
                return -1;
            }
            *buf = bufs[vbuf.index].start;
            *size = vbuf.bytesused;
            *index = vbuf.index;

            return 0;
        }
    }
}

// ��֡�������
int camera_eqbuf(int fd, unsigned int index)
{
    int ret;
    struct v4l2_buffer vbuf;

    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuf.memory = V4L2_MEMORY_MMAP;
    vbuf.index = index;
    ret = ioctl(fd, VIDIOC_QBUF, &vbuf);
    if (ret == -1) {
        printf("camera->eqbuf: qbuf error!\n");
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
        printf("camera->stop: streamoff error!\n");
        return -1;
    }
    printf("camera->stop: stop capture\n");

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

    printf("camera->exit: camera exit\n");

    return close(fd);

}
