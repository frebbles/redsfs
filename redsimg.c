/*
 * REDSFS Image creator and reader tool
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
#include <string.h>
#include <dirent.h>

#include "redsfs.h"

static int retcode = 0;
static uint8_t *flash;

void die(char * msg)
{
    //printf("ERROR: %s \r\n", msg);
    exit(-1);
}

uint32_t linux_fs_read ( uint32_t addr, uint32_t size, uint8_t * dest ) 
{
    //printf ("Called READ on addr offset dec %d - %p >>> %p ( %d ) \r\n", addr, (flash+addr), dest, size );
    return (uint32_t) memcpy (dest, flash + addr, size );
}

uint32_t linux_fs_write ( uint32_t addr, uint32_t size, uint8_t * src )
{
    //printf ("Called WRITE on addr offset dec %d - %p <<< %p ( %d ) \r\n", addr, (flash+addr), src, size );
    return (uint32_t) memcpy ( flash + addr,  src, size );
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
    printf("Path is: %s\r\n",filepath);
    int fin = open( filepath, O_RDONLY );
        if (fin < 0) return fin;

    while ((n = read(fin, buf, sizeof(buf)))) {
        redsfs_write ( buf, n );
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

int export_dir ( char * path )
{

    return 0;
}

int main( int argc, char *argv[] )
{
    int opt;
    const char *fname = 0;
    bool create = false;
    enum { CMD_NONE, CMD_LIST, CMD_INTERACTIVE, CMD_SCRIPT } command = CMD_NONE;
    size_t sz = 0;
    const char *script_name = 0;

    //printf("Size of the redsfs_fb %d \r\n", sizeof(redsfs_fb) );
    while ((opt = getopt (argc, argv, "f:c:lir:")) != -1)
    {
        switch (opt)
	{
          case 'f': fname = optarg; break;
          case 'c': create = true; sz = strtoul (optarg, 0, 0); break;
          case 'l': command = CMD_LIST; break;
          case 'i': command = CMD_INTERACTIVE; break;
          case 'r': command = CMD_SCRIPT; script_name = optarg; break;
          default: die("no options");
       }
    }
    
    //printf("Attempting open of %s \r\n", fname);
    int fd = open (fname, (create ? (O_CREAT | O_TRUNC) : 0) | O_RDWR, 0660);
    if (fd == -1)
    die ("File not opened");

    if (create)
    {
	//printf("Seeking to end of file/creating \r\n");
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

    //printf("Mapping...\r\n");
    flash = mmap (0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (!flash)
        die ("mmap");
    else
	printf("Got flash pointer at %p to %p \r\n", flash, (flash+sz)); 
    if (create) {
	//printf("Zeroing data...\r\n");
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

    /*
    printf("Reading back file... \r\n");
    file = redsfs_open("random.txt", MODE_READ );
 
    printf("Opened...\r\n ");

    while (n = redsfs_read (buf, sizeof(buf))) {
      printf("READ: %d bytes: %s \r\n", n, buf);
    }
    
    redsfs_close();
    */

    import_dir( "./filesystem" );

    printf("Unmounting... \r\n");
    redsfs_unmount();

    munmap(flash, sz);
    close(fd);

}
