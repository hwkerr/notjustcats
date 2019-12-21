#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define SECTORSIZE 0x200
#define FAT 1
#define ROOT 19
#define CLUSTERSTART 31 // 33 - 2

#define BYTESIZE 4
#define FATSIZE 12

#define DIRENTRYSIZE 32
#define FILENAME_OFFSET 0
#define FILENAME_LENGTH 8
#define EXT_OFFSET 8
#define EXT_LENGTH 3
#define ATTR_OFFSET 11
#define ATTR_LENGTH 1
#define RESERVED_OFFSET 12
#define RESERVED_LENGTH 2
#define CREATETIME_OFFSET 14
#define CREATETIME_LENGTH 2
#define CREATEDATE_OFFSET 16
#define CREATEDATE_LENGTH 2
#define LASTACCESS_OFFSET 18
#define LASTACCESS_LENGTH 2
#define LASTWTIME_OFFSET 22
#define LASTWTIME_LENGTH 2
#define LASTWDATE_OFFSET 24
#define LASTWDATE_LENGTH 2
#define FIRSTCLUSTER_OFFSET 26
#define FIRSTCLUSTER_LENGTH 2
#define FILESIZE_OFFSET 28
#define FILESIZE_LENGTH 4

#define DELETED (char)0xE5
#define LASTGOODCLUSTER 0xFEF
#define DIRECTORY 0x10

#define STRINGBUFFER 50

void rwdir(char *img, int startsector, char *path, char *outputdir);
void writedata(FILE *dest, char *src, uint16_t cluster, uint32_t size, int deleted);
uint16_t fatnext(char *img, uint16_t cluster);
void trimfilename(char *dest, const char *src);

int filect;

int main(int argc, char **argv)
{
	char *imagefile = "", *outputdir = "";
	if (argc > 2)
  {
		imagefile = argv[1];
    outputdir = argv[2];
  }
  else if (argc == 2)
  {
    imagefile = argv[1];
    outputdir = "./recovered_files";
  }
	else
  {
		fprintf(stderr, "Improper usage.\n");
		return 0;
	}
  mkdir(outputdir, ACCESSPERMS);

  int fd = open(imagefile, O_RDONLY);
  struct stat sb;
	if (fstat(fd, &sb) < 0)
	{
		perror("couldn't get file size.\n");
	}
	char *floppy = mmap(NULL, sb.st_size,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE, fd, 0);

  filect = 0;

  char *path = malloc(STRINGBUFFER);
  rwdir(floppy, ROOT, path, outputdir);
	free(path);

  munmap(floppy, sb.st_size);
  close(fd);
	return 0;
}

// Reads through the image file getting metadata about directories
// Writes information to files placed in outputdir
void rwdir(char *img, int startsector, char *path, char *outputdir)
{
  for(int i = startsector * SECTORSIZE; img[i] != 0; i += DIRENTRYSIZE)
  {
    char *filename = malloc(FILENAME_LENGTH + 1);
    strncpy(filename, &img[i+FILENAME_OFFSET], FILENAME_LENGTH);
    filename[FILENAME_LENGTH] = 0;
    char *trimname = malloc(FILENAME_LENGTH + 1);
    trimfilename(trimname, filename);
		if (strcmp(trimname, ".") == 0 || strcmp(trimname, "..") == 0)
			continue;
    char *name = malloc(STRINGBUFFER);
    strcpy(name, "/");
    strcat(name, trimname);

    char *extension = malloc(EXT_LENGTH + 1);
    strncpy(extension, &img[i+EXT_OFFSET], EXT_LENGTH);
    extension[EXT_LENGTH] = 0;

		uint8_t attr = *(uint8_t *)(img+i+ATTR_OFFSET);
		int isDir = ((attr & DIRECTORY) == DIRECTORY);

    uint16_t cluster = (uint16_t) img[i + FIRSTCLUSTER_OFFSET];
    uint32_t size = *(uint32_t *)(img + i + FILESIZE_OFFSET);

    if (!isDir)
    {
      if (img[i] == DELETED)
        printf("FILE\tDELETED\t%s%s.%s\t%d\n", path, name, extension, size);
      else
        printf("FILE\tNORMAL\t%s%s.%s\t%d\n", path, name, extension, size);

      char filenum[FILENAME_LENGTH];
      sprintf(filenum, "%d", filect);
      char newfile[STRINGBUFFER] = "";
      strcat(newfile, outputdir);
      strcat(newfile, "/file");
      strcat(newfile, filenum);
      strcat(newfile, ".");
      strcat(newfile, extension);
      filect++;
      FILE *fp = fopen(newfile, "wb");
      writedata(fp, img, cluster, size, img[i]==DELETED);
    }
    else if (isDir)
    {
      int len = strlen(path);
      strcat(path, name);
      if (startsector != CLUSTERSTART + cluster && cluster > 1)
        rwdir(img, CLUSTERSTART + cluster, path, outputdir);
      path[len] = 0;
    }

    free(filename);
    free(trimname);
    free(name);
    free(extension);
  }
}

// Accepts parameters that include details about the file to be written
// Recursively accesses clusters to write to file
void writedata(FILE *out, char *img, uint16_t cluster, uint32_t size, int deleted)
{
	if (size > 0 && cluster > 1)
	{
    int data = (CLUSTERSTART + cluster) * SECTORSIZE;
    for (int i = data; i < data + SECTORSIZE && size > 0; i++, size--)
      fprintf(out, "%c", img[i]);

    uint16_t nextcluster = fatnext(img, cluster);
		if (deleted) {
			if (nextcluster == 0) nextcluster = cluster+1;
			else if (nextcluster != 0) return;
		}

    if (nextcluster <= LASTGOODCLUSTER && ((!deleted && nextcluster > 1 && nextcluster != cluster) || deleted))
      writedata(out, img, nextcluster, size, deleted);
  }
}

// Decodes FAT at specified cluster
// returns fatnext = FAT cluster pointed to by "cluster"
uint16_t fatnext(char *img, uint16_t cluster)
{
  int offset = FAT * SECTORSIZE;
  // math for rearranging nibbles (see specification document section 3.1)
  offset += cluster / 2 * (FATSIZE / BYTESIZE);
  if (cluster % 2 == 0)
  {
    uint16_t word = *(uint16_t *)(img+offset);
    return word & 0x0fff; // math to get the first of the two entries
  }
  else
  {
    uint16_t word = *(uint16_t *)(img+offset+1);
    return word >> 4; // math to get the second of the two entries
  }
}

// removes trailing spaces from a string
// replaces any initial DELETED character with '_'
void trimfilename(char *dest, const char *src)
{
  size_t len = strlen(src);
  for(int i = len-1; i >= 0; i--)
  {
		if (strncmp(src+i, " ", 1) != 0)
		{
			strncpy(dest, src, i+1);
			dest[i+1] = 0;
			break;
		}
	}
	if (src[0] == DELETED)
		dest[0] = '_';
}
