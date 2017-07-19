/*
 * REally Dang Simple File System (for flash/micro filesystems/esp8266)
 * Author: Farran Rebbeck
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#define _BV(b) (1 << (b))

typedef uint32_t (*flash_read)(uint32_t addr, uint32_t size, uint8_t *dst);
typedef uint32_t (*flash_write)(uint32_t addr, uint32_t size, uint8_t *src);

typedef struct redsfs__filesystem {
    uint32_t	fs_start;
    uint32_t	fs_block_size;
    flash_read  call_read_f;
    flash_write call_write_f;
    uint32_t	fs_end;
    int8_t	mounted;
} redsfs_fs;

#define MODE_READ   0
#define MODE_WRITE  1
#define MODE_APPEND 2

typedef struct redsfs__filehandle {
    int8_t 	handle;         // Handle = 1 for basic operation 0 is "not open"
    uint32_t    f_start_blk;    // Chunk offset for first part of file
    uint32_t    f_cur_blk;      // Chunk offset for current part of file
    uint32_t	blk_curoffset;  // Block offset in the current chunk
    uint8_t     mode;
} redsfs_fh;

// Block size is 256
//   file block flags = 4  //fb
//   next block addr  = 4  //fb
//   size = 4              //db
//   optional char namedata = 32
//   data block total = 256 - 5(fb) = 251
#define BLK_OFFSET_FIRST 44
#define BLK_OFFSET_CHUNK 12
#define BLK_SIZE 256
typedef struct redsfs__datablock {
    uint32_t	size; 		// 4 Size of block (!namedata/dblock total not including header)
    char        namedata[32];   // 32 not included in size calculations in first block
    uint8_t     dblock[212];    // 212
} redsfs_db;

//
#define FB_IS_USED    _BV(0)
#define FB_IS_FIRST   _BV(1)
#define FB_IS_CONT    _BV(2)
#define FB_IS_LAST    _BV(3)

typedef struct redsfs__fb {
    //bool	used;
    //bool	isfirst;
    //bool	iscontinuing;
    //bool	islast;
    uint32_t	flags;
    uint32_t    next_blk_addr;
    redsfs_db	data;
} redsfs_fb;

// Callable functions.
uint8_t redsfs_mount(redsfs_fs *rfs);
char * redsfs_next_file();
int32_t redsfs_next_empty_block();
uint8_t redsfs_open(char * fname, uint8_t mode);
void redsfs_close();
uint8_t redsfs_delete(char * name);
void redsfs_seek_to_end();
uint8_t redsfs_unmount();
size_t redsfs_write( char * buf, size_t size );
size_t redsfs_read( char * buf, size_t size );
