#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <jpeglib.h>
#include "image_process.h"

#pragma pack(2)        //设置为2字节对齐
struct bmp_fileheader
{
    unsigned short		bfType;        
    unsigned int		bfSize;
    unsigned short		bfReverved1;
    unsigned short		bfReverved2;
    unsigned int		bfOffBits;
};
struct bmp_infoheader
{
    unsigned int	biSize;
    unsigned int	biWidth;
    unsigned int    biHeight;
    unsigned short  biPlanes;
    unsigned short  biBitCount;
    unsigned int    biCompression;
    unsigned int    biSizeImage;
    unsigned int    biXPelsPerMeter;
    unsigned int    biYPelsPerMeter;
    unsigned int    biClrUsed;
    unsigned int    biClrImportant;
};

/*************************************************
Function: yuv422_to_rgb24
Description: 讲yuv422格式数据转换为rgb24格式
Input: 
    rgb24:指向存放rgb数据地址
    yuv422:指向yuv422数据地址
	width: rgb图片宽度
	height: rgb图片高度
Output: 
Return: 成功返回0，失败返回-1
Others: 
*************************************************/
void yuv422_to_rgb24(unsigned char *rgb24, const unsigned char *yuv422, unsigned int width, unsigned int height)
{
	int	i,j;
    unsigned char y1, y2, u, v;
    int r1,g1,b1,r2,g2,b2;
    const unsigned char *pointer;
    
	pointer = yuv422;
	
    for(i=0;i<height;i++)
    {
    	for(j=0;j<(width/2);j++)
    	{
    		y1 = *( pointer + (i*(width/2)+j)*4);
    		u  = *( pointer + (i*(width/2)+j)*4 + 1);
    		y2 = *( pointer + (i*(width/2)+j)*4 + 2);
    		v  = *( pointer + (i*(width/2)+j)*4 + 3);
    		
    		r1 = y1 + 1.042*(v-128);
    		g1 = y1 - 0.34414*(u-128) - 0.71414*(v-128);
    		b1 = y1 + 1.772*(u-128);
    		
    		r2 = y2 + 1.042*(v-128);
    		g2 = y2 - 0.34414*(u-128) - 0.71414*(v-128);
    		b2 = y2 + 1.772*(u-128);
    		
    		if(r1>255)
    			r1 = 255;
    		else if(r1<0)
    			r1 = 0;
    		
    		if(b1>255)
    			b1 = 255;
    		else if(b1<0)
    			b1 = 0;	
    		
    		if(g1>255)
    			g1 = 255;
    		else if(g1<0)
    			g1 = 0;	
    			
    		if(r2>255)
    			r2 = 255;
    		else if(r2<0)
    			r2 = 0;
    		
    		if(b2>255)
    			b2 = 255;
    		else if(b2<0)
    			b2 = 0;	
    		
    		if(g2>255)
    			g2 = 255;
    		else if(g2<0)
    			g2 = 0;		
    			
    		*(rgb24 + ((height-1-i)*(width/2)+j)*6    ) = (unsigned char)b1;
    		*(rgb24 + ((height-1-i)*(width/2)+j)*6 + 1) = (unsigned char)g1;
    		*(rgb24 + ((height-1-i)*(width/2)+j)*6 + 2) = (unsigned char)r1;
    		*(rgb24 + ((height-1-i)*(width/2)+j)*6 + 3) = (unsigned char)b2;
    		*(rgb24 + ((height-1-i)*(width/2)+j)*6 + 4) = (unsigned char)g2;
    		*(rgb24 + ((height-1-i)*(width/2)+j)*6 + 5) = (unsigned char)r2;
    	}
    }
}

/*************************************************
Function: rgb24_to_jpeg
Description: rgb24格式缓存转换为jpeg格式
Input: 
    jpeg:指向jpeg缓冲地址
    rgb:指向存放rgb图片缓冲区的首地址
Output: 
	width: rgb图片宽度
	height: rgb图片高度
Return: 成功返回0，失败返回-1
Others: 
*************************************************/
void rgb24_to_jpeg(unsigned char *jpeg, unsigned char *rgb24, unsigned int width, unsigned int height)
{
	unsigned long jpeg_size;
	struct jpeg_compress_struct jcs;
	struct jpeg_error_mgr jem;
	JSAMPROW row_pointer[1];
	int row_stride;

	jcs.err = jpeg_std_error(&jem);
	jpeg_create_compress(&jcs);

	jpeg_mem_dest(&jcs, &jpeg, &jpeg_size);

	jcs.image_width = width;
	jcs.image_height = height;

	jcs.input_components = 3;//1;
	jcs.in_color_space = JCS_RGB;//JCS_GRAYSCALE;

	jpeg_set_defaults(&jcs);
	jpeg_set_quality(&jcs, 180, TRUE);

	jpeg_start_compress(&jcs, TRUE);
	row_stride =jcs.image_width * 3;

	while(jcs.next_scanline < jcs.image_height){//对每一行进行压缩
		row_pointer[0] = &rgb24[jcs.next_scanline * row_stride];
		(void)jpeg_write_scanlines(&jcs, row_pointer, 1);
	}
	jpeg_finish_compress(&jcs);
	jpeg_destroy_compress(&jcs);
}

/*************************************************
Function: jpeg_to_rgb24
Description: 解压缩jpeg图片为rgb24格式的图片
Input: 
    rgb:指向存放rgb图片缓冲区的首地址
    jpeg:待解压缩的jpeg缓冲地址
Output: 
	width: rgb图片宽度
	height: rgb图片高度
Return: 成功返回0，失败返回-1
Others: 
*************************************************/
int jpeg_to_rgb24(unsigned char *rgb24, unsigned char *jpeg, unsigned int *width, unsigned int *height, unsigned int jpegsize)
{
    JSAMPARRAY buffer;  
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    
    cinfo.err=jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
//    jpeg_stdio_src(&cinfo, jpegfp);
	jpeg_mem_src(&cinfo, jpeg, (unsigned long)jpegsize);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    unsigned char *rgb_p = NULL;
    rgb_p = rgb24;
    *width = cinfo.output_width;
    *height = cinfo.output_height;
    unsigned short depth = cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)\
                ((j_common_ptr)&cinfo,JPOOL_IMAGE,(*width)*depth,1);

    while(cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(rgb_p, *buffer, depth * (*width));
        rgb_p += depth * (*width);
    }
    
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return 0;
}

/*************************************************
Function: jpeg_file_to_rgb24
Description: 解压缩jpeg图片为rgb24格式的图片
Input: 
    rgb:指向存放rgb图片缓冲区的首地址
    filepath:待解压缩的jpeg图片的路径
Output: 将产生的rgb图片放在了rgb所指向的缓冲区
	width: rgb图片宽度
	height: rgb图片高度
Return: 成功返回0，失败返回-1
Others: 
*************************************************/
int jpeg_file_to_rgb24(unsigned char *rgb24, const char *filepath, unsigned int *width, unsigned int *height)
{
    FILE * jpegfp = fopen(filepath, "rb");
    if(jpegfp == NULL)
        return -1;
    
    JSAMPARRAY buffer;  
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    
    cinfo.err=jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, jpegfp);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    unsigned char *rgb_p = NULL;
    rgb_p = rgb24;
    *width = cinfo.output_width;
    *height = cinfo.output_height;
    unsigned short depth = cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)\
                ((j_common_ptr)&cinfo,JPOOL_IMAGE,(*width)*depth,1);

    while(cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(rgb_p, *buffer, depth * (*width));
        rgb_p += depth * (*width);
    }
    
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    fclose(jpegfp);
    return 0;
}

/*************************************************
Function: rgb24_to_bmp
Description: 将24位的rgb图片转换为bmp图片
Input: 
    bmp:存放bmp格式图片数据的缓冲区首地址
    rgb24:存放rgb格式图片数据的缓冲区首地址
    width:rgb图片的宽
    heigth：rgb图片的高
	isMjpeg: 是否是JPEG格式，JPEG格式数据是正的，而BMP格式数据是倒着的
Output:
	bmpsize: bmp数据大小
Return: void
Others: 
*************************************************/
void rgb24_to_bmp(unsigned char *bmp, const unsigned char *rgb24, unsigned int width, unsigned int height, unsigned char isMjpeg, unsigned int *bmpsize)
{
    struct bmp_fileheader bfh;
    struct bmp_infoheader bih;
    
    unsigned short depth = 3;
    unsigned long headersize = 54;
    unsigned long filesize = width * height * depth;

	*bmpsize = 54 + filesize; 
    memset(&bfh,0,sizeof(struct bmp_fileheader));
    memset(&bih,0,sizeof(struct bmp_infoheader));
    
    //填充bmp头信息   
    bfh.bfType=0x4D42;
    bfh.bfSize=54 + filesize;
//    bfh.bfSize= filesize;
    bfh.bfOffBits=headersize;

    bih.biSize=40;
    bih.biWidth=width;
    bih.biHeight=height;
    bih.biPlanes=1;
    bih.biBitCount=(unsigned short)depth*8;
    bih.biSizeImage=width*height*depth;
//	bi.biCompression = 0;
//	bi.biXPelsPerMeter = 0;
//	bi.biYPelsPerMeter = 0;
//	bi.biClrUsed = 0;
//	bi.biClrImportant = 0;

    memcpy(bmp, &bfh, sizeof(struct bmp_fileheader));
    memcpy(bmp+sizeof(struct bmp_fileheader), &bih, sizeof(struct bmp_infoheader));
	if (! isMjpeg) // 不是MJPEG，是YUYV
	{
		memcpy(bmp + 54, rgb24, filesize);		
		return;
	}
 
    unsigned char *line_buff;
    const unsigned char *point;
    
    line_buff=(unsigned char *)malloc(width*depth);//申请一行rbg格式数据大小的缓冲区
    memset(line_buff,0,sizeof(unsigned char)*width*depth);

    point=rgb24+width*depth*(height-1);    //倒着写数据，bmp格式是倒的，jpg是正的
    unsigned int i, j;
    for (i=0;i<height;i++)
    {
        for (j=0;j<width * depth;j += depth)
        {
                line_buff[j+2]=point[j+0];
                line_buff[j+1]=point[j+1];
                line_buff[j+0]=point[j+2];
        }
        point-=width*depth;
      
        memcpy(bmp + 54 + i * width * depth, line_buff, width * depth);//以行的方式将数据存入bmp buff中去
    }
    free(line_buff);
}

/*************************************************
Function: write_file 
Description: write buf to file 
Input: 
	path: file name
	buf: data to write
	count: bytes num to write
Output: 写数据到文件中
Return: 0, 成功；-1，失败 
Others: 
*************************************************/
int write_file(const char *path, const void *buf, unsigned int count)
{
	FILE *fp;

	fp = fopen(path, "wb");
	if (!fp)
	{
		printf("fopen file %s error! \n", path);
		return -1;
	}

	fwrite(buf, count, 1, fp);
	fclose(fp);
	
	return 0;
}

/*************************************************
Function: write_fd 
Description: write buf to file 
Input: 
	path: file name
	buf: data to write
	count: bytes num to write
Output: 写数据到文件中
Return: 写入的字节数；-1，失败;  
Others: 
*************************************************/
int write_fd(const char *path, const char *buf, unsigned int count)
{
    int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0664);
    ssize_t ret = 0;
    ssize_t total = 0;
    while (total < count)
    {
        ret = write(fd, buf+total, count-total);
        if (-1 == ret)
        {
			if (errno == EINTR)
			{
				continue;
			}
			else
			{
				perror("write file fd");
				printf("%s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__);
				exit(-1);
			}
        }
        else if (0 == ret)
        {
            perror("write file fd 0 byte");
            continue;
        }
        else
        {
            total += ret;
        }
    }
#ifdef MDEBUG
    printf("write %d bytes to file %s.\n", total, path);
#endif
    close(fd);

	return total;
}
