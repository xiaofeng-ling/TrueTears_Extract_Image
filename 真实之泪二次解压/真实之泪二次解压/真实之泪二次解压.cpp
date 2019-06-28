// 真实之泪二次解压.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "zlib.h"
#include <windows.h>

using namespace std;

struct CRX
{
	char head[4];		// 头幻数 CRXG
	int none_1;			// 未知的4个字节
	unsigned short width;	// 宽
	unsigned short height;	// 高
	unsigned short none_2;	// 未知，似乎固定为02？
	unsigned short none_3;  // 未知，似乎固定为01？
	unsigned int type;		// 类型，0表示24位（8*3）图像， 1表示32位（8*4）图像,带有透明色，需要解码
};

// 构建位图文件头
char *build_bmp_header(const unsigned int width, const unsigned int height, const unsigned int bits)
{
	BITMAPFILEHEADER file_header;
	BITMAPINFOHEADER info_header;
	file_header.bfType = 0x4D42;
	file_header.bfReserved1 = 0;
	file_header.bfReserved2 = 0;
	file_header.bfSize = width * height*bits + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	info_header.biWidth = width;
	info_header.biHeight = height;
	info_header.biBitCount = bits == 3 ? 24 : 32;
	info_header.biSize = sizeof(BITMAPINFOHEADER);
	info_header.biPlanes = 1;
	info_header.biCompression = 0;
	info_header.biSizeImage = 0;
	info_header.biXPelsPerMeter = 3780;
	info_header.biYPelsPerMeter = 3780;
	info_header.biClrUsed = 0;
	info_header.biClrImportant = 0;

	char *temp = new char[sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)]();
	memcpy(temp, &file_header, sizeof(BITMAPFILEHEADER));
	memcpy(temp + sizeof(BITMAPFILEHEADER), &info_header, sizeof(BITMAPINFOHEADER));
	return temp;
}

int extract(const char *filename, const char *out_filename);
char *filter_background(const unsigned char *data, const int width, int height, const int bits);

int main()
{
	const char *files[] = {
		"D:\\TrueTears\\ADVDATA\\GRP\\EVG\\1RUI_BIG_C3.CRX",
	};

	for (int i = 0; i < sizeof(files); i++)
	{
		string out_filename(files[i]);
		out_filename.replace(out_filename.end() - 4, out_filename.end(), ".bmp");

		extract(files[i], (const char *)out_filename.data());
	}

	std::cout << "Hello World!\n";
}

int extract(const char *filename, const char *out_filename)
{
	char *file_data;
	char *unzip_data;

	// todo crxg文件格式
	// 开头四字节的幻数，4字节无用，2字节的宽度，2字节的高度，固定2字节02，固定2字节01 ，4字节，如果00，表明24位，01表示32位

	ifstream stream;
	stream.open(filename, ios::in | ios::binary);
	int filesize = 0;
	stream.seekg(0, ios::_Seekend);
	filesize = stream.tellg();
	stream.seekg(0, ios::_Seekbeg);
	file_data = new char[filesize];
	stream.read(file_data, filesize);
	stream.close();

	struct CRX *crxg_info = new struct CRX;
	crxg_info = (struct CRX *)file_data;

	int width = 0;
	int height = 0;
	int bits = 3;

	// 校验文件信息
	if (strncmp(crxg_info->head, "CRXG", 4))
	{
		printf("文件错误\n");
		return 0;
	}


	width = crxg_info->width;
	height = crxg_info->height;
	bits = crxg_info->type == 1 ? 4 : 3;

	// 跳过头信息
	file_data += sizeof(struct CRX);

	// 首先进行zlib数据解码
	int unzip_data_len = 800 * 600 * 30;
	unzip_data = new char[unzip_data_len]();	// 这种内存空间应该是够用了吧

	int result = uncompress((Bytef *)unzip_data, (uLongf *)&unzip_data_len, (Bytef *)file_data, filesize - sizeof(struct CRX));

	char *temp = new char[unzip_data_len]();

	memcpy(temp, unzip_data, unzip_data_len);

	file_data -= sizeof(struct CRX);
	delete[]file_data;
	delete[]unzip_data;

	unzip_data = temp;

	int i = 0;

	char *dst = new char[width * height * bits]();
	char *dst_ptr = dst + width * height * bits;
	char *origin = dst_ptr;

	// 初始化
	dst_ptr -= width * bits;

	// 针对不同的bits，采用不同的处理方式
	if (3 == bits)
	{
		// 高的循环
		for (int h = height; h > 0; h--)
		{
			char al = unzip_data[i];
			i++;

			if (al == 0)
			{
				// 拉取三个字节
				*dst_ptr = unzip_data[i++];
				*dst_ptr++;

				*dst_ptr = unzip_data[i++];
				*dst_ptr++;

				*dst_ptr = unzip_data[i++];
				*dst_ptr++;
				for (int j = 0; j < (width - 1) * 3; j++)
				{
					*dst_ptr = (unsigned char)(*(dst_ptr - bits) + unzip_data[i++]);
					dst_ptr++;
				}

				dst_ptr -= width * bits * 2;
			}
			else if (al == 1)
			{
				// 把前一行的数据加上下一行的数据 再复制一行（2400）一模一样的
				for (int j = 0; j < width * bits; j++)
				{
					dst_ptr[j] = (unsigned char)(unzip_data[i + j] + dst_ptr[j + width * bits]);
				}

				// 直接跳过这一行
				i += width * bits;
				// 继续空出一定的空间
				dst_ptr -= width * bits;
			}
			else if (al == 2)
			{
				// 拉取三个字节
				dst_ptr[0] = unzip_data[i + 0];
				dst_ptr[1] = unzip_data[i + 1];
				dst_ptr[2] = unzip_data[i + 2];
				for (int j = 3; j < width * bits; j++)
				{
					dst_ptr[j] = (unsigned char)(unzip_data[i + j] + dst_ptr[j + (width - 1) * bits]);
				}

				dst_ptr -= width * bits;
				i += width * bits;
			}
			else if (al == 3)
			{
				int j;
				for (j = 3; j < width * bits; j++)
				{
					dst_ptr[j - bits] = (unsigned char)(unzip_data[i + j - bits] + dst_ptr[j + width * bits]);
				}
				dst_ptr[j + 0 - bits] = unzip_data[i + j + 0 - bits];
				dst_ptr[j + 1 - bits] = unzip_data[i + j + 1 - bits];
				dst_ptr[j + 2 - bits] = unzip_data[i + j + 2 - bits];

				// 直接跳过这一行
				i += width * bits;
				// 继续空出一定的空间
				dst_ptr -= width * bits;
			}
			else if (al == 4)
			{
				for (int ecx = 3; ecx > 0; ecx--)
				{
					// 获取宽
					for (int _width = width; _width > 0; )
					{
						// ps: 获取地址，准备开始写入

						// 获取数据中的下一个值
						char cl = unzip_data[i++];

						// 将数据写入到指定位置
						*dst_ptr = cl;
						dst_ptr += bits;
						_width--;

						if (cl == unzip_data[i])
						{
							i++;
							unsigned char bl = unzip_data[i];	// 表明下一个字节代表多少的偏移量
							i++;
							_width -= bl;

							for (bl; bl > 0; bl--)
							{
								*dst_ptr = cl;
								dst_ptr += bits;
							}
						}
					}

					dst_ptr -= width * bits - 1;
				}
				// 将dst的内存地址向前移动width *= 3（2403）个字节，继续开始填充数据
				// 因为最开始的数据会提前1条
				dst_ptr -= 801 * bits;
			}
		}
	}
	else if (4 == bits)
	{
		// 高的循环
		for (int h = height; h > 0; h--)
		{
			char al = unzip_data[i];
			i++;

			if (al == 0)
			{
				// 拉取三个字节
				*dst_ptr = unzip_data[i++];
				*dst_ptr++;

				*dst_ptr = unzip_data[i++];
				*dst_ptr++;

				*dst_ptr = unzip_data[i++];
				*dst_ptr++;

				*dst_ptr = unzip_data[i++];
				*dst_ptr++;
				for (int j = 0; j < (width - 1)*bits; j++)
				{
					*dst_ptr = (unsigned char)(*(dst_ptr - bits) + unzip_data[i++]);
					dst_ptr++;
				}

				dst_ptr -= width * bits * 2;
			}
			else if (al == 1)
			{
				// 把前一行的数据加上下一行的数据 再复制一行（2400）一模一样的
				for (int j = 0; j < width * bits; j++)
				{
					dst_ptr[j] = (unsigned char)(unzip_data[i + j] + dst_ptr[j + width * bits]);
				}

				// 直接跳过这一行
				i += width * bits;
				// 继续空出一定的空间
				dst_ptr -= width * bits;
			}
			else if (al == 2)
			{
				// 拉取四个字节
				dst_ptr[0] = unzip_data[i + 0];
				dst_ptr[1] = unzip_data[i + 1];
				dst_ptr[2] = unzip_data[i + 2];
				dst_ptr[3] = unzip_data[i + 3];

				for (int j = 4; j < width * bits; j++)
				{
					dst_ptr[j] = (unsigned char)(unzip_data[i + j] + dst_ptr[j + (width - 1) * bits]);
				}

				dst_ptr -= width * bits;
				i += width * bits;
			}
			else if (al == 3)
			{

				int j;
				for (j = 4; j < width * bits; j++)
				{
					dst_ptr[j - bits] = (unsigned char)(unzip_data[i + j - bits] + dst_ptr[j + width * bits]);
				}
				dst_ptr[j + 0 - bits] = unzip_data[i + j + 0 - bits];
				dst_ptr[j + 1 - bits] = unzip_data[i + j + 1 - bits];
				dst_ptr[j + 2 - bits] = unzip_data[i + j + 2 - bits];
				dst_ptr[j + 3 - bits] = unzip_data[i + j + 3 - bits];

				// 直接跳过这一行
				i += width * bits;
				// 继续空出一定的空间
				dst_ptr -= width * bits;
			}
			else if (al == bits)
			{
				for (int ecx = bits; ecx > 0; ecx--)
				{
					// 获取宽
					for (int _width = width; _width > 0; )
					{
						// ps: 获取地址，准备开始写入

						// 获取数据中的下一个值
						char cl = unzip_data[i++];

						// 将数据写入到指定位置
						*dst_ptr = cl;
						dst_ptr += bits;
						_width--;

						if (cl == unzip_data[i])
						{
							i++;
							unsigned char bl = unzip_data[i];	// 表明下一个字节代表多少的偏移量
							i++;
							_width -= bl;

							for (bl; bl > 0; bl--)
							{
								*dst_ptr = cl;
								dst_ptr += bits;
							}
						}

						int debug = 1;
					}

					dst_ptr -= width * bits - 1;
				}
				// 将dst的内存地址向前移动width *= 3（2403）个字节，继续开始填充数据
				// 因为最开始的数据会提前1条
				dst_ptr -= (width + 1) * bits;
			}
		}

		// 这个函数有问题，所以直接输出透明度的图片
		//dst = filter_background((unsigned char*)dst, width, height, bits);
	}

	char *file_header = build_bmp_header(width, height, bits);

	fstream fs;
	fs.open(out_filename, ios::out | ios::binary);
	fs.write(file_header, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));
	fs.write(dst, width * height * bits);
	fs.close();
	delete[] dst;
	delete[] unzip_data;
	delete[] file_header;

	return 0;
}

// 过滤背景色
// 这个函数有问题
char *filter_background(const unsigned char *data, const int width, int height, const int bits)
{
	int i = 0;
	unsigned char *result = new unsigned char[width*height * 3]();

	// 初始化
	for (int j = 0; j < width*height; j++)
	{
		result[j * 3 + 0] = 0xCB;
		result[j * 3 + 1] = 0xDC;
		result[j * 3 + 2] = 0xF5;
	}

	unsigned char *result_ptr = result;
	for (int j = 0; j < width * height; j++)
	{
		unsigned char al = data[i];

		if (al == 0xFF)
		{
			result_ptr += 3;
			i += 4;
			continue;
		}

		if (al == 0)
		{
			i++;
			unsigned int a = *(unsigned int *)result_ptr;
			unsigned int b = data[i];
			unsigned int r = ((a ^ b) & 0x00FFFFFF) ^ a;
			*(unsigned int *)result_ptr = r;

			i += 3;
			result_ptr += 3;
		}
		else
		{
			i++;

			al &= 0xFF;

			unsigned char head = al;

			// #CBDCF5  背景颜色值
			unsigned char dl = *result_ptr;
			unsigned int edi = dl * head;
			al = data[i++];
			unsigned int eax = al * (0xFF - head);
			edi = edi + eax;

			edi = edi / 255;
			*result_ptr = (unsigned char)edi;
			result_ptr++;

			for (int j = 0; j < 2; j++)
			{
				// #CBDCF5  背景颜色值
				unsigned char dl = *result_ptr;
				al = data[i++];
				// gcc居然会将x/255转为 (x *0x80808081) >> 39...详见：https://research.swtch.com/divmult
				// 这汇编代码看得我一脸懵逼
				// 这里的算法是应该是不透明度算法的逆向
				*result_ptr = (unsigned char)((dl * head + al * (0xFF - head)) / 255);
				result_ptr++;
			}
		}
	}
	return (char *)result;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门提示: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
