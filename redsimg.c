/*
 * REDSFS Image creator and reader tool
 *
 * Author: Farran Rebbeck
 *         https://github.com/frebbles/redsfs
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>

#include "redsfs.h"

static int retcode = 0;
static uint8_t *flash;

// Die with an error message
void die(char * msg)
{
    printf("ERROR: %s \r\n", msg);
    exit(-1);
}

// Mapped function for reading (for micros this is usually a SPI/FLASH read function call)
uint32_t linux_fs_read ( uint32_t addr, uint32_t size, uint8_t * dest ) 
{
    memcpy (dest, flash + addr, size );
    return 0;
}

// Mapped function for writing (for micros this is usually a SPI/FLASH write function call)
uint32_t linux_fs_write ( uint32_t addr, uint32_t size, uint8_t * src )
{
    memcpy ( flash + addr,  src, size );
    return 0;
}

int import_file ( char * dir, char * path )
{
    int n;
    char buf[256];
    int filepathlen;
    int pathlen;
    int dirlen;
    char * filepath;

    // Open reds file for writing
    int file = redsfs_open( path, MODE_WRITE );
        if (file < 0) return -1;

    // Open file for reading to copy into redsfs
    filepathlen = strlen(dir) + strlen(path) + 2;
    pathlen = strlen(path);
    dirlen = strlen(dir);
    filepath = malloc( filepathlen );
    memset( filepath, 0, filepathlen );
    memcpy( filepath, dir, dirlen );
    memcpy( filepath+dirlen, "/", 1);
    memcpy( filepath+dirlen+1, path, pathlen);
    int fin = open( filepath, O_RDONLY );
        if (fin < 0) return fin;

    while ((n = read(fin, buf, sizeof(buf)))) {
        retcode = redsfs_write ( buf, n );
        if (retcode < 0) die("Write issue (out of space?)\r\n");
    }

    // Close the reds file (complete the copy);
    redsfs_close();
    close(fin);
    free(filepath);
    return 0;
}

int import_dir ( char * path )
{
    // Open directory
    DIR *d;
    struct dirent *dir;
    d = opendir(path);
    if (d) 
    {
        while ((dir = readdir(d)) != NULL)
	{
            if (strcmp(dir->d_name, "..") && strcmp(dir->d_name, ".")) 
	    {
                printf("Importing file : %s\n", dir->d_name);
		import_file(path, dir->d_name);
	    }
	}
	closedir(d);
    }

    return 0;
}

int export_file ( char * dir, char * path )
{
    int n;
    char buf[256];
    int filepathlen;
    int pathlen;
    int dirlen;
    char * filepath;

    // Open reds file for writing
    int file = redsfs_open( path, MODE_WRITE );
    if (file < 0) return -1;

    // Open file for reading to copy into redsfs
    filepathlen = strlen(dir) + strlen(path) + 2;
    pathlen = strlen(path);
    dirlen = strlen(dir);
    filepath = malloc( filepathlen );
    memset( filepath, 0, filepathlen );
    memcpy( filepath, dir, dirlen );
    memcpy( filepath+dirlen, "/", 1);
    memcpy( filepath+dirlen+1, path, pathlen);
    
    FILE * fout = fopen( filepath, "w" );
    if (fout < 0) return -1;
    
    while ((n = redsfs_read( buf, sizeof(buf)))) {
        retcode = fwrite ( buf, 1, n, fout );
    }

    // Close the reds file (complete the copy);
    redsfs_close();
    fclose(fout);
    free(filepath);
    return 0;
}

int export_dir ( char * path )
{
    uint8_t file;
    char * fname;

    // Check our export directory has been created
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }

    // Start export/list
    printf("Exporting/Listing files...\r\n");
    while ( (fname = redsfs_next_file()) != NULL ) {
        printf("Exporting file: %s \r\n", fname);
        export_file( path, fname );
    }

    return 0;
}

void list_files()
{
    uint8_t file;
    char * fname;

    printf("Files in redsfs image:\r\n");
    while ( ( fname = redsfs_next_file() ) != NULL ) {
        printf(" - %s\r\n", fname);
    }
}

int readwrite_test()
{
    char buf[256];
    int bufSz;

    printf ("Opening a file to write...\r\n");
    int file = redsfs_open( "test.txt", MODE_WRITE );
    printf ("Writing to file\r\n");
    if (redsfs_write("The quick brown fox jumps over the lazy dog... ", 48) == 0) 
    {
	redsfs_close();
	redsfs_unmount();
        die("Couldn't write to redsfs...");
    }
    printf("Closing file");
    redsfs_close();

    printf("Reopening read to read contents back...\r\n");
    file = redsfs_open("test.txt", MODE_WRITE );
    bufSz = redsfs_read(buf, 256);
    if (bufSz > 0)
    {
        printf("READ: %s\r\n", buf);
    } else {
	redsfs_close();
	redsfs_unmount();
        die("Could not read from redsfs...");
    }

    printf ("Opening a file to append some more\r\n");
    file = redsfs_open( "test.txt", MODE_APPEND );
    printf ("Writing to file\r\n");
    if (redsfs_write("\nThe slow wharty sloth crawls under the cosy blanket... ", 48) == 0)          
    {
        redsfs_close();
        redsfs_unmount();
        die("Couldn't write to redsfs...");
    }
    printf("Closing file");
    redsfs_close();

    printf("Reopening to read appended text\r\n");
    file = redsfs_open("test.txt", MODE_WRITE );
    bufSz = redsfs_read(buf, 256);
    if (bufSz > 0) 
    {
        printf("READ: %s\r\n", buf);
    } else {
        redsfs_close();
        redsfs_unmount();
        die("Could not read from redsfs...");
    }
    printf("Deleting file");
    redsfs_delete("test.txt");
    printf("Read/Write tests passed");
    return 0;
}

int main( int argc, char *argv[] )
{
    int opt;
    const char *fname = 0;
    bool create = false;
    enum { CMD_NONE, CMD_LIST, CMD_IMPORT, CMD_EXPORT, CMD_TEST } command = CMD_NONE;
    size_t sz = 0;
    char *imp_dir = 0;
    char *exp_dir = 0;

    while ((opt = getopt (argc, argv, "f:c:li:e:t:")) != -1)
    {
        switch (opt)
	{
          case 'f': fname = optarg; break;
          case 'c': create = true; sz = strtoul (optarg, 0, 0); break;
          case 'l': command = CMD_LIST; break;
          case 'i': command = CMD_IMPORT; imp_dir = optarg; break;
          case 'e': command = CMD_EXPORT; exp_dir = optarg; break;
          case 't': command = CMD_TEST; break;
          default: die("no options");
       }
    }
    
    int fd = open (fname, (create ? (O_CREAT | O_TRUNC) : 0) | O_RDWR, 0660);
    if (fd == -1)
    die ("File not opened");

    if (create)
    {
        if (lseek (fd, sz -1, SEEK_SET) == -1)
            die ("lseek");
        if (write (fd, "", 1) != 1)
            die ("write");
    }
    else if (!sz)
    {
        off_t offs = lseek (fd, 0, SEEK_END);
        if (offs == -1)
            die ("lseek");
        sz = offs;
    }

    if (sz & (BLK_SIZE -1)) 
        die ("file size not multiple of page size");

    flash = mmap (0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (!flash)
        die ("mmap");
    else
	printf("Got flash pointer at %p to %p \r\n", flash, (flash+sz)); 
    if (create) {
        memset (flash, 0, sz);
    }
    redsfs_fs redsfs_mnt;
    redsfs_mnt.fs_start = 0;
    redsfs_mnt.fs_block_size = 256;
    redsfs_mnt.call_read_f = linux_fs_read;
    redsfs_mnt.call_write_f = linux_fs_write;;
    redsfs_mnt.fs_end = sz;

    printf("Mounting redsfs...\r\n");
    int rfmt = redsfs_mount( &redsfs_mnt );

    if (command == CMD_IMPORT)
    { 
        import_dir( imp_dir );
    }
    
    if (command == CMD_EXPORT)
    {
        export_dir( exp_dir );
    }

    if (command == CMD_LIST)
    {
        list_files(); 
    }

    printf("Unmounting... \r\n");
    redsfs_unmount();

    munmap(flash, sz);
    close(fd);

}

