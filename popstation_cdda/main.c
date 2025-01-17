#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif
#include <zlib.h>

#include "toc.h"
#include "data.h"
#include "version.h"

int getsize(FILE *f)
{
	int size;

	fseek(f, 0, SEEK_END);
	size = ftell(f);

	fseek(f, 0, SEEK_SET);
	return size;
}

void ErrorExit(char *fmt, ...)
{
	va_list list;
	char msg[256];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	printf(msg);
	exit(-1);
}

z_stream z;

int deflateCompress(void *inbuf, int insize, void *outbuf, int outsize, int level)
{
	int res;

	z.zalloc = Z_NULL;
	z.zfree  = Z_NULL;
	z.opaque = Z_NULL;

	if (deflateInit2(&z, level , Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
		return -1;

	z.next_out  = (Bytef *)outbuf;
	z.avail_out = outsize;
	z.next_in   = (Bytef *)inbuf;
	z.avail_in  = insize;

	if (deflate(&z, Z_FINISH) != Z_STREAM_END)
	{
		return -1;
	}

	res = outsize - z.avail_out;

	if (deflateEnd(&z) != Z_OK)
		return -1;

	return res;
}

#ifndef WIN32
typedef struct  __attribute__((packed))
#else
#pragma pack(1)
typedef struct
#endif
{
	unsigned int signature;
	unsigned int version;
	unsigned int fields_table_offs;
	unsigned int values_table_offs;
	int nitems;
} SFOHeader;
#ifdef WIN32
#pragma pack()
#endif

#ifndef WIN32
typedef struct  __attribute__((packed))
#else
#pragma pack(1)
typedef struct
#endif
{
	unsigned short field_offs;
	unsigned char  unk;
	unsigned char  type; // 0x2 -> string, 0x4 -> number
	unsigned int length;
	unsigned int size;
	unsigned short val_offs;
	unsigned short unk4;
} SFODir;
#ifdef WIN32
#pragma pack()
#endif

void SetSFOTitle(char *sfo, char *title)
{
	SFOHeader *header = (SFOHeader *)sfo;
	SFODir *entries = (SFODir *)(sfo+0x14);
	int i;

	for (i = 0; i < header->nitems; i++)
	{
		if (strcmp(sfo+header->fields_table_offs+entries[i].field_offs, "TITLE") == 0)
		{
			strncpy(sfo+header->values_table_offs+entries[i].val_offs, title, entries[i].size);

			if (strlen(title)+1 > entries[i].size)
			{
				entries[i].length = entries[i].size;
			}
			else
			{
				entries[i].length = strlen(title)+1;
			}
		}
	}
}

#define BASE "BASE.PBP"

char buffer[1*1048576];
char buffer2[0x9300];

int *ib = (int*)buffer;

int pic0 = 0, pic1 = 0, icon0 = 0, icon1 = 0, snd = 0, toc = 0, boot = 0;
int sfo_size, pic0_size, pic1_size, icon0_size, icon1_size, snd_size, toc_size, boot_size, prx_size;
int start_dat = 0, min, sec, frm;
unsigned int psp_header[0x30/4];
unsigned int base_header[0x28/4];
unsigned int header[0x28/4];
unsigned int dummy[6];

#ifndef WIN32
typedef struct __attribute__((packed))
#else
#pragma pack(1)
typedef struct
#endif
{
	unsigned int offset;
	unsigned int length;
	unsigned int dummy[6];
} IsoIndex;
#ifdef WIN32
#pragma pack()
#endif

void convert(char *input, char *output, char *title, char *code, int complevel)
{
	FILE *in, *out, *base, *t;
	int i, offset, isosize, isorealsize, x;
	int index_offset, p1_offset, p2_offset, end_offset, curoffs;
	IsoIndex *indexes;

	void* tocptr;

	in = fopen (input, "rb");
	if (!in)
	{
		ErrorExit("Cannot open %s\n", input);
	}

	isosize = getsize(in);
	isorealsize = isosize;

	if ((isosize % 0x9300) != 0)
	{
		isosize = isosize + (0x9300 - (isosize%0x9300));
	}

	//printf("isosize, isorealsize %08X  %08X\n", isosize, isorealsize);

	base = fopen(BASE, "rb");
	if (!base)
	{
		ErrorExit("Cannot open %s\n", BASE);
	}

	out = fopen(output, "wb");
	if (!out)
	{
		ErrorExit("Cannot create %s\n", output);
	}

	printf("Writing header...\n");

	fread(base_header, 1, 0x28, base);

	if (base_header[0] != 0x50425000)
	{
		ErrorExit("%s is not a PBP file.\n", BASE);
	}

	sfo_size = base_header[3] - base_header[2];

	t = fopen("ICON0.PNG", "rb");
	if (t)
	{
		icon0_size = getsize(t);
		icon0 = 1;
		fclose(t);
	}
	else
	{
		icon0_size = base_header[4] - base_header[3];
	}

	t = fopen("ICON1.PMF", "rb");
	if (t)
	{
		icon1_size = getsize(t);
		icon1 = 1;
		fclose(t);
	}
	else
	{
		icon1_size = 0;
	}

	t = fopen("PIC0.PNG", "rb");
	if (t)
	{
		pic0_size = getsize(t);
		pic0 = 1;
		fclose(t);
	}
	else
	{
		pic0_size = 0; //base_header[6] - base_header[5];
	}

	t = fopen("PIC1.PNG", "rb");
	if (t)
	{
		pic1_size = getsize(t);
		pic1 = 1;
		fclose(t);
	}
	else
	{
		pic1_size = 0; // base_header[7] - base_header[6];
	}

	t = fopen("SND0.AT3", "rb");
	if (t)
	{
		snd_size = getsize(t);
		snd = 1;
		fclose(t);
	}
	else
	{
		snd = 0;
	}

	if ((tocptr = create_toc(input, &toc_size)) != NULL)
	{
		toc = 2;
	}
	else if ((t = fopen("ISO.TOC", "rb")) != NULL)
	{
		printf("  Using ISO.TOC for toc\n");
		toc_size = getsize(t);
		toc = 1;
		fclose(t);
	}
	else if (toc != 0)
	{
		toc = 0;
	}

	t = fopen("BOOT.PNG", "rb");
	if (t)
	{
		boot_size = getsize(t);
		boot = 1;
		fclose(t);
	}

	fseek(base, base_header[8], SEEK_SET);
	fread(psp_header, 1, 0x30, base);

	prx_size = psp_header[0x2C/4];

	curoffs = 0x28;

	header[0] = 0x50425000;
	header[1] = 0x10000;

	header[2] = curoffs;

	curoffs += sfo_size;
	header[3] = curoffs;

	curoffs += icon0_size;
	header[4] = curoffs;

	curoffs += icon1_size;
	header[5] = curoffs;

	curoffs += pic0_size;
	header[6] = curoffs;

	curoffs += pic1_size;
	header[7] = curoffs;

	curoffs += snd_size;
	header[8] = curoffs;

	x = header[8] + prx_size;

	if ((x % 0x10000) != 0)
	{
		x = x + (0x10000 - (x % 0x10000));
	}

	header[9] = x;

	fwrite(header, 1, 0x28, out);

	printf("Writing sfo...\n");

	fseek(base, base_header[2], SEEK_SET);
	fread(buffer, 1, sfo_size, base);
	SetSFOTitle(buffer, title);
	strcpy(buffer+0x108, code);
	fwrite(buffer, 1, sfo_size, out);

	if (!icon0)
	{
		fseek(base, base_header[3], SEEK_SET);
		fread(buffer, 1, icon0_size, base);
		fwrite(buffer, 1, icon0_size, out);
	}
	else
	{
		printf("Writing icon0.png...\n");

		t = fopen("ICON0.PNG", "rb");
		fread(buffer, 1, icon0_size, t);
		fwrite(buffer, 1, icon0_size, out);
		fclose(t);
	}

	if (icon1)
	{
		printf("Writing icon1.pmf...\n");

		t = fopen("ICON1.PMF", "rb");
		fread(buffer, 1, icon1_size, t);
		fwrite(buffer, 1, icon1_size, out);
		fclose(t);
	}

	if (!pic0)
	{
		//fseek(base, base_header[5], SEEK_SET);
		//fread(buffer, 1, pic0_size, base);
		//fwrite(buffer, 1, pic0_size, out);
	}
	else
	{
		printf("Writing pic0.png...\n");

		t = fopen("PIC0.PNG", "rb");
		fread(buffer, 1, pic0_size, t);
		fwrite(buffer, 1, pic0_size, out);
		fclose(t);
	}

	if (!pic1)
	{
		//fseek(base, base_header[6], SEEK_SET);
		//fread(buffer, 1, pic1_size, base);
		//fwrite(buffer, 1, pic1_size, out);
	}
	else
	{
		printf("Writing pic1.png...\n");

		t = fopen("PIC1.PNG", "rb");
		fread(buffer, 1, pic1_size, t);
		fwrite(buffer, 1, pic1_size, out);
		fclose(t);
	}

	if (snd)
	{
		printf("Writing snd0.at3...\n");

		t = fopen("SND0.AT3", "rb");
		fread(buffer, 1, snd_size, t);
		fwrite(buffer, 1, snd_size, out);
		fclose(t);
	}

	printf("Writing DATA.PSP...\n");
	fseek(base, base_header[8], SEEK_SET);
	fread(buffer, 1, prx_size, base);
	fwrite(buffer, 1, prx_size, out);

	offset = ftell(out);

	for (i = 0; i < header[9]-offset; i++)
	{
		fputc(0, out);
	}

	printf("Writing iso header...\n");

	fwrite("PSISOIMG0000", 1, 12, out);

	p1_offset = ftell(out);

	x = isosize + 0x100000;
	fwrite(&x, 1, 4, out);

	x = 0;
	for (i = 0; i < 0xFC; i++)
	{
		fwrite(&x, 1, 4, out);
	}

	memcpy(data1+1, code, 4);
	memcpy(data1+6, code+4, 5);

	offset = isorealsize/2352+150;
	min = offset/75/60;
	sec = (offset-min*60*75)/75;
	frm = offset-(min*60+sec)*75;
	data1[0x41b] = bcd(min);
	data1[0x41c] = bcd(sec);
	data1[0x41d] = bcd(frm);

	// Wiser to use the CCD if it was specified over the ISO.TOC
	if (toc == 2)
	{
		printf("  Copying toc to iso header...\n");

		memcpy(data1+1024, tocptr, toc_size);
		free(tocptr);
	}
	else if (toc == 1)
	{
		printf("  Copying toc to iso header...\n");

		t = fopen("ISO.TOC", "rb");
		fread(buffer, 1, toc_size, t);
		memcpy(data1+1024, buffer, toc_size);
		fclose(t);
	}

	fwrite(data1, 1, sizeof(data1), out);

	p2_offset = ftell(out);
	x = isosize + 0x100000 + 0x2d31;
	fwrite(&x, 1, 4, out);

	strcpy((char *)data2+8, title);
	fwrite(data2, 1, sizeof(data2), out);

	index_offset = ftell(out);

	printf("Writing indexes...\n");

	memset(dummy, 0, sizeof(dummy));

	offset = 0;

	if (complevel == 0)
	{
		x = 0x9300;
	}
	else
	{
		x = 0;
	}

	for (i = 0; i < isosize / 0x9300; i++)
	{
		fwrite(&offset, 1, 4, out);
		fwrite(&x, 1, 4, out);
		fwrite(dummy, 1, sizeof(dummy), out);

		if (complevel == 0)
			offset += 0x9300;
	}

	offset = ftell(out);

	for (i = 0; i < (header[9]+0x100000)-offset; i++)
	{
		fputc(0, out);
	}

	printf("Writing iso...\n");

	if (complevel == 0)
	{
		while ((x = fread(buffer, 1, 1048576, in)) > 0)
		{
			fwrite(buffer, 1, x, out);
		}

		for (i = 0; i < (isosize-isorealsize); i++)
		{
			fputc(0, out);
		}
	}
	else
	{
		indexes = (IsoIndex *)malloc(sizeof(IsoIndex) * (isosize/0x9300));

		if (!indexes)
		{
			fclose(in);
			fclose(out);
			fclose(base);

			ErrorExit("Cannot alloc memory for indexes!\n");
		}

		i = 0;
		offset = 0;

		while ((x = fread(buffer2, 1, 0x9300, in)) > 0)
		{
			if (x < 0x9300)
			{
				memset(buffer2+x, 0, 0x9300-x);
			}

			x = deflateCompress(buffer2, 0x9300, buffer, sizeof(buffer), complevel);

			if (x < 0)
			{
				fclose(in);
				fclose(out);
				fclose(base);
				free(indexes);

				ErrorExit("Error in compression!\n");
			}

			memset(&indexes[i], 0, sizeof(IsoIndex));

			indexes[i].offset = offset;

			if (x >= 0x9300) /* Block didn't compress */
			{
				indexes[i].length = 0x9300;
				fwrite(buffer2, 1, 0x9300, out);
				offset += 0x9300;
			}
			else
			{
				indexes[i].length = x;
				fwrite(buffer, 1, x, out);
				offset += x;
			}

			i++;
		}

		if (i != (isosize/0x9300))
		{
			fclose(in);
			fclose(out);
			fclose(base);
			free(indexes);

			ErrorExit("Some error happened.\n");
		}

		x = ftell(out);

		if ((x % 0x10) != 0)
		{
			end_offset = x + (0x10 - (x % 0x10));

			for (i = 0; i < (end_offset-x); i++)
			{
				fputc('0', out);
			}
		}
		else
		{
			end_offset = x;
		}

		end_offset -= header[9];
	}

	printf("Writing special data...\n");

	fseek(base, base_header[9]+12, SEEK_SET);
	fread(&x, 1, 4, base);

	x += 0x50000;

	fseek(base, x, SEEK_SET);
	fread(buffer, 1, 8, base);

	if (memcmp(buffer, "STARTDAT", 8) != 0)
	{
		ErrorExit("Cannot find STARTDAT in %s.\n",
			      "Not a valid PSX eboot.pbp\n", BASE);
	}

	fseek(base, x+16, SEEK_SET);
	fread(header, 1, 8, base);
	fseek(base, x, SEEK_SET);
	fread(buffer, 1, header[0], base);

	if (!boot)
	{
		fwrite(buffer, 1, header[0], out);
		fread(buffer, 1, header[1], base);
		fwrite(buffer, 1, header[1], out);
	}
	else
	{
		printf("Writing boot.png...\n");

		ib[5] = boot_size;
		fwrite(buffer, 1, header[0], out);
		t = fopen("BOOT.PNG", "rb");
		fread(buffer, 1, boot_size, t);
		fwrite(buffer, 1, boot_size, out);
		fclose(t);
		fread(buffer, 1, header[1], base);
	}

	while ((x = fread(buffer, 1, 1048576, base)) > 0)
	{
		fwrite(buffer, 1, x, out);
	}

	if (complevel != 0)
	{
		printf("Updating compressed indexes...\n");

		fseek(out, p1_offset, SEEK_SET);
		fwrite(&end_offset, 1, 4, out);

		end_offset += 0x2d31;
		fseek(out, p2_offset, SEEK_SET);
		fwrite(&end_offset, 1, 4, out);

		fseek(out, index_offset, SEEK_SET);
		fwrite(indexes, 1, sizeof(IsoIndex) * (isosize/0x9300), out);
	}

	fclose(in);
	fclose(out);
	fclose(base);
}

#define N_GAME_CODES	12

char *gamecodes[N_GAME_CODES] =
{
	"SCUS",
	"SLUS",
	"SLES",
	"SCES",
	"SCED",
	"SLPS",
	"SLPM",
	"SCPS",
	"SLED",
	"SLPS",
	"SIPS",
	"ESPM"
};

int main(int argc, char *argv[])
{
	int i;

	printf("popstation  %s\n", VERSION_STRING);
	printf("  see VERSION.H for more information\n");
	printf("  supports ccd handling (for toc)\n");
	printf("\n");

	if (argc != 5)
	{
		ErrorExit("Invalid number of arguments.\n"
				  "Usage: %s title gamecode compressionlevel file.iso\n", argv[0]);
	}

	if (strlen(argv[2]) != 9)
	{
		ErrorExit("Invalid game code.\n");
	}

	for (i = 0; i < N_GAME_CODES; i++)
	{
		if (strncmp(argv[2], gamecodes[i], 4) == 0)
			break;
	}

	if (i == N_GAME_CODES)
	{
		//ErrorExit("Invalid game code.\n");
		printf("Warning: The game code you specified is not using a standard prefix.\n");
	}

	for (i = 4; i < 9; i++)
	{
		if (argv[2][i] < '0' || argv[2][i] > '9')
		{
			ErrorExit("Invalid game code.\n");
		}
	}

	if (strlen(argv[3]) != 1)
	{
		ErrorExit("Invalid compression level.\n");
	}

	if (argv[3][0] < '0' || argv[3][0] > '9')
	{
		ErrorExit("Invalid compression level.\n");
	}

	convert(argv[4], "EBOOT.PBP", argv[1], argv[2], argv[3][0]-'0');

	printf("Done.\n");
	return 0;
}

