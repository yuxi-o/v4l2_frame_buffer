#ifndef __IMAGE_PROCESS_H__
#define __IMAGE_PROCESS_H__

#include <stdio.h>

//void rgb24_to_jpeg(unsigned char *jpeg, const unsigned char *rgb24, unsigned int width, unsigned int height);
void rgb24_to_jpeg(unsigned char *jpeg, unsigned char *rgb24, unsigned int width, unsigned int height);
int rgb24_to_yuv420p(unsigned char *yuvBuf, unsigned char *rgb24, unsigned int width, unsigned int height, unsigned int *len);
void yuv422_to_rgb24(unsigned char *rgb24, const unsigned char *yuv422, unsigned int width, unsigned int height);
//int jpeg_to_rgb24(unsigned char *rgb24, const unsigned char *jpeg, unsigned int *width, unsigned int *height, unsigned int jpegsize);
int jpeg_to_rgb24(unsigned char *rgb24, unsigned char *jpeg, unsigned int *width, unsigned int *height, unsigned int jpegsize);
int jpeg_file_to_rgb24(unsigned char *rgb24, const char *filepath, unsigned int *width, unsigned int *height);
void rgb24_to_bmp(unsigned char *bmp, const unsigned char *rgb24, unsigned int width, unsigned int height, unsigned char isMjpeg, unsigned int *bmpsize);
int write_file(const char *path, const void *buf, unsigned int count);
int write_fd(const char *path, const char *buf, unsigned int count);
int h264_encode_start(unsigned int width, unsigned int height);
int h264_encode_stop(void);
int h264_encode_frame(const unsigned char *pdata, unsigned int width, unsigned int height, unsigned int length, FILE* fp);
int h264_encode_frame_420p(const unsigned char *pdata, unsigned int width, unsigned int height, unsigned int length, FILE* fp);
#endif
