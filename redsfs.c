/*
 * REally Dang Simple File System (for flash/micro filesystems)
 * Author: Farran Rebbeck
 */

#include "redsfs.h"

// GLOBAL Vars

redsfs_fs r_fsys;
redsfs_fh r_fhand;
uint32_t seek_chunk; // For seeking through filesystem (ls)

// File in/out cache
uint8_t * redsfs_cache;

// File Seeker cache (only needs to seek headers 40bytes, but keeping 256 for strictness)
// separate to file cache so we dont damage the file cache when seeking new blank blocks
uint8_t * redsfs_seek_cache;

// Helper functions
int32_t redsfs_next_empty_block()
{
    uint32_t chunk;
    uint8_t rres;

    // Check we are mounted
    if (r_fsys.mounted != 1) {
        return -1;
    }

    // Check the filesystem and find the first block not marked as used.
    for ( chunk = r_fsys.fs_start; chunk < r_fsys.fs_end; chunk += r_fsys.fs_block_size )
    {
        rres = r_fsys.call_read_f ( chunk, 40, redsfs_seek_cache );
        if ( ((redsfs_fb*)redsfs_seek_cache)->flags & ( FB_IS_USED ) ) {
	    continue;
	} else {
            return chunk;
	}
    }
    return -2;
}

char * redsfs_next_file()
{
    uint32_t chunk;
    uint8_t rres;

    // Check we are mounted
    if (r_fsys.mounted != 1) {
        return NULL;
    }

    // Check and seek through the file system
    for ( chunk = seek_chunk; chunk < r_fsys.fs_end; chunk += r_fsys.fs_block_size ) {
        rres = r_fsys.call_read_f ( chunk, 40, redsfs_seek_cache );
	// Do we have a new file header block?
        if ( ((redsfs_fb*)redsfs_seek_cache)->flags & ( FB_IS_FIRST ) ) {
            // Update our current seeking mark to the next one
	    seek_chunk = chunk + r_fsys.fs_block_size;
	    // Read the current full block, with filename
	    rres = r_fsys.call_read_f ( chunk, 256, redsfs_seek_cache );
	    // Return the file name of the current block
            return ((redsfs_fb*)redsfs_seek_cache)->data.namedata;
	} else {
	    // Keep going until we find the next populated first block
            continue;
	}
    }
    // Update (the last block)
    seek_chunk = chunk;
    // Finish up returning NULL, like readdir.
    return NULL;
}

// Seek the file chunk pointer and size pointer to one past the last byte of the current file.
void redsfs_seek_to_end()
{
    uint32_t chunk;
    uint32_t file_size;
    uint32_t block_size;
    int rres;

    // Check we are mounted
    if (r_fsys.mounted != 1)
        return;
   
    // Check we have an open file!
    if (r_fhand.handle != 1)
	return;

    // Loop through file system to last written byte.
    // assume we have the current chunk in the redsfs_cache
    chunk = r_fhand.f_start_blk;
    while ( chunk < r_fsys.fs_end )
    {
	rres = r_fsys.call_read_f ( chunk, 256, redsfs_cache );
	// Check if this is the last block, if not go to next one.
        if (( ( ((redsfs_fb*)redsfs_cache)->flags & FB_IS_USED ) &&
              ( ((redsfs_fb*)redsfs_cache)->flags & FB_IS_LAST ) ) )
	{
	    // Set the pointer to the current data size, dependant on whether first or othe block.
	    if ( ((redsfs_fb*)redsfs_cache)->flags & FB_IS_FIRST ) {
                r_fhand.blk_curoffset = ((redsfs_fb*)redsfs_cache)->data.size + BLK_OFFSET_FIRST;
	    } else {
		r_fhand.blk_curoffset = ((redsfs_fb*)redsfs_cache)->data.size + BLK_OFFSET_CHUNK;
	    }

	    // Set the current chunk in handle
	    r_fhand.f_cur_blk = chunk;
	    break;
	} else { // Not the last block, increment and next.
            chunk += r_fsys.fs_block_size;
	}
    }
    //printf("Returning at chunk %d with offset at %d\r\n", r_fhand.f_cur_blk, r_fhand.blk_curoffset );
    return;
}

// Main function calls
uint8_t redsfs_mount(redsfs_fs *rfs)
{
    // Check for header of file system, return true if function calls for read succeed with header
    r_fsys.fs_start = rfs->fs_start;
    r_fsys.fs_block_size = rfs->fs_block_size;
    r_fsys.fs_end = rfs->fs_end;
 
    // Calling functions copied
    r_fsys.call_read_f = rfs->call_read_f;
    r_fsys.call_write_f = rfs->call_write_f;
    r_fsys.mounted = 1;
    
    // Seeking/ls for file system
    seek_chunk = r_fsys.fs_start;

    // Allocate memory and clear
    redsfs_cache = malloc(r_fsys.fs_block_size);
    memset (redsfs_cache, 0, r_fsys.fs_block_size);
    redsfs_seek_cache = malloc(r_fsys.fs_block_size);
    memset (redsfs_seek_cache, 0, r_fsys.fs_block_size);

    return 0;

}

uint8_t redsfs_unmount()
{
    // Check if mounted flag set, unset.
    if (r_fsys.mounted == 1) r_fsys.mounted = 0;
        else return -1;

    // Free/release allocated memory
    free(redsfs_cache);
    redsfs_cache = 0;
    free(redsfs_seek_cache);
    redsfs_seek_cache = 0;

    return 0;
}

uint8_t redsfs_open(char * fname, uint8_t mode)
{
    int32_t chunk;
    uint8_t rres;

    r_fhand.handle = -1;

    // Check to see if filename is in the filesystem
    // Cycle through all blocks until file is found or not
    for (chunk = r_fsys.fs_start; chunk < r_fsys.fs_end; chunk += r_fsys.fs_block_size)
    {
        rres = r_fsys.call_read_f ( chunk, r_fsys.fs_block_size, redsfs_cache );
        // Check if block is USED and is FIRST
	if ( ( ((redsfs_fb*)redsfs_cache)->flags & FB_IS_FIRST ) &&
	     ( ((redsfs_fb*)redsfs_cache)->flags & FB_IS_USED ) ) {
            // Check the file name
	    char * fb_fname = ((redsfs_fb*)redsfs_cache)->data.namedata;
	    // Found the file in this block
	    if ( strcmp( fb_fname, fname ) == 0 )
	    {
                r_fhand.handle = 1;
		r_fhand.f_start_blk = chunk;
		r_fhand.f_cur_blk = chunk;
		r_fhand.blk_curoffset = BLK_OFFSET_FIRST;
		r_fhand.mode = mode;
		if ( mode == MODE_APPEND )
                    redsfs_seek_to_end();
		return 0;
	    }
	}
    }
    // If we were just opening to read and didnt find the file, we're out here with a fail
    if ( mode == MODE_READ )
        return -1;

    // If we are opening to write, we can create a file stub here...
    // must also setup the cache memory chunk
    if ( r_fhand.handle == -1 ) {
        chunk = redsfs_next_empty_block();

	//printf("Next chunk found at %d\r\n", chunk);
        if (chunk < 0)
            return -1;

        r_fhand.handle = 1; 
	r_fhand.mode = MODE_WRITE;
        r_fhand.f_start_blk = chunk;
        r_fhand.f_cur_blk = chunk;
        r_fhand.blk_curoffset = BLK_OFFSET_FIRST;
	// Clear the memory structure for file cache of block.
	memset ( redsfs_cache, 0, r_fsys.fs_block_size );
        // Setup the first block flags and used flags
	((redsfs_fb*)redsfs_cache)->flags |= ( FB_IS_USED | FB_IS_FIRST );
	// Setup the first block filename part of struct (not used in other blocks)
	//printf("Copying file name to block... %d size and %s name..:%p: old name ...", strlen(fname), fname, ((redsfs_fb*)redsfs_cache)->data.namedata );
	memcpy( ((redsfs_fb*)redsfs_cache)->data.namedata, fname, strlen(fname) );
    }
    //printf(" Returning open file %d \r\n", r_fhand.handle);
    return r_fhand.handle;
}

void redsfs_close()
{
    // Invalidate our handle
    r_fhand.handle = -1;
    
    // If we are writing, then a block exists in cache to write to memory
    if ( ( r_fhand.mode == MODE_WRITE ) || (r_fhand.mode == MODE_APPEND ) ) {
        // Complete the flags (ensure "FB_IS_LAST" is set)
        ((redsfs_fb*)redsfs_cache)->flags |= ( FB_IS_USED | FB_IS_LAST );
        // Write to mem
	//printf("Committing rest of file to flash at chunk %d .\r\n", r_fhand.f_cur_blk);
        r_fsys.call_write_f ( r_fhand.f_cur_blk, 256, redsfs_cache );

        // Clear file handle vars
        r_fhand.f_start_blk = 0;
        r_fhand.f_cur_blk = 0;
        r_fhand.blk_curoffset = 0;
	r_fhand.mode = 0;
    }
}

uint8_t redsfs_delete( char * name )
{
    // Open the file for reading ( open file at the beginning )
    redsfs_open ( name, MODE_READ );

    // For all the bits of the file scrub and delete
    while ( ((redsfs_fb*)redsfs_cache)->next_blk_addr )
    {
        uint32_t nextBlk = ((redsfs_fb*)redsfs_cache)->next_blk_addr ;
        memset( redsfs_cache, 0, r_fsys.fs_block_size );
	r_fsys.call_write_f ( r_fhand.f_cur_blk, r_fsys.fs_block_size, redsfs_cache );
	r_fsys.call_read_f ( nextBlk, r_fsys.fs_block_size, redsfs_cache );
    } 
    // Last block wont have a next address, just remove it
    memset( redsfs_cache, 0, r_fsys.fs_block_size );
    r_fsys.call_write_f ( r_fhand.f_cur_blk, r_fsys.fs_block_size, redsfs_cache );

    return 0;
}

size_t redsfs_read( char * buf, size_t size )
{
    size_t toFetch = size;
    size_t cacheLeft = 0;
    size_t readSz = 0;
    size_t readBytes = 0;
    int rres;
    uint32_t chunk = 0;

    while (toFetch > 0) {
        // Request the block/chunk into memory.
        chunk = r_fhand.f_cur_blk;
        rres = r_fsys.call_read_f ( chunk, 256, redsfs_cache );
    
        // Caclculate the amount left in the current block, depends on if it is first
        if ( ((redsfs_fb*)redsfs_cache)->flags & FB_IS_FIRST) {
          cacheLeft = ( ((redsfs_fb*)redsfs_cache)->data.size + BLK_OFFSET_FIRST) - r_fhand.blk_curoffset;
	} else {
          cacheLeft = ( ((redsfs_fb*)redsfs_cache)->data.size + BLK_OFFSET_CHUNK) - r_fhand.blk_curoffset;
        }
        
	// Might get to the end of the buffer and still have more to request? Break here.
	if (cacheLeft <= 0)
            break;

        // If the requested amount is greater or equal to what's left
        if (toFetch >= cacheLeft) {
            readSz = cacheLeft;
	} else { // The requested amount is less than what's left in the cache
            readSz = toFetch;
        }

        // Copy to the return buffer, the requested file size, if the current block is used up only fill a bit
        memcpy( buf + (size - toFetch), redsfs_cache + r_fhand.blk_curoffset, readSz );
    
        // Are we into the next block?
        if ( (r_fhand.blk_curoffset + readSz) >= BLK_SIZE ) {
            // If we are at the end of the block, move to the next block
            r_fhand.f_cur_blk = ((redsfs_fb*)redsfs_cache)->next_blk_addr;
    	    // Set the next block's offset
            r_fhand.blk_curoffset = BLK_OFFSET_CHUNK; // Chunk offset, the next block wont be a header
        } else {
    	    // Increase the current offset
    	    r_fhand.blk_curoffset += readSz;
        }

        // Update the amount left to fetch
        toFetch -= readSz;
	readBytes += readSz;
    }
    // Return the amount read from the file
    return readBytes;
}

size_t redsfs_write( char * buf, size_t size )
{
    size_t toWrite = size;
    size_t writeSz = 0;
    size_t cacheLeft = 0;
    size_t writtenBytes = 0;
    int32_t nextBlkAddr = 0;
    int rres;

    // While we have bytes to write.
    while (toWrite > 0)
    {
        // Check to see how many bytes are left in this chunk
	cacheLeft = BLK_SIZE - r_fhand.blk_curoffset;

	// If the amount to write is less than the cache leftover ensure we dont over write
	if (toWrite >= cacheLeft) {
	    writeSz = cacheLeft;
	} else {  // If what we have will fit in the cache then just use that amount.
	    writeSz = toWrite;
        }

	//printf(" Perform cache prep %s \r\n", buf);
        //printf(" Copy to cache offset %d from %d of buf\r\n", r_fhand.blk_curoffset, (size - toWrite));
	if (writeSz > 0) {
	    // Copy to the cache from buffer
	    memcpy( redsfs_cache + r_fhand.blk_curoffset, buf + (size - toWrite), writeSz );
	}
        // Update block size information
	((redsfs_fb*)redsfs_cache)->data.size += writeSz;

	// Update amount we have left to write in toWrite
	toWrite -= writeSz;
	writtenBytes += writeSz;

	//printf(" toWrite now %d, writeSz was %d, chunk size is currently %d \r\n", toWrite, writeSz, ((redsfs_fb*)redsfs_cache)->data.size);
        // Have we filled the current block?
	if ( (r_fhand.blk_curoffset + writeSz) >= BLK_SIZE )  {
	    // Unset the last block flag
            ((redsfs_fb*)redsfs_cache)->flags &= ~(FB_IS_LAST);

            // Next block pointer needs to be populated
            // need to write usage flags to this current block first
            rres = r_fsys.call_write_f ( r_fhand.f_cur_blk, 256, redsfs_cache );

	    // Find the next available block
	    nextBlkAddr = redsfs_next_empty_block();
	    ((redsfs_fb*)redsfs_cache)->next_blk_addr = nextBlkAddr;
            
	    // Re-Commit this block to memory with next block addr and setup the new one.
            rres = r_fsys.call_write_f ( r_fhand.f_cur_blk, 256, redsfs_cache );

	    // If we've not got a new block (no space left) exit
	    if (nextBlkAddr >= 0) 
	      ((redsfs_fb*)redsfs_cache)->next_blk_addr = nextBlkAddr;
            else
              return nextBlkAddr;

            // Setup new block
	    r_fhand.f_cur_blk = nextBlkAddr;
            r_fhand.blk_curoffset = BLK_OFFSET_CHUNK;
            // Clear the memory structure for file cache of block.
            memset ( redsfs_cache, 0, r_fsys.fs_block_size );
            // Setup the first block flags and used flags
            ((redsfs_fb*)redsfs_cache)->flags |= ( FB_IS_USED | FB_IS_CONT );
            //printf("New chunk setup with size %d \r\n", ((redsfs_fb*)redsfs_cache)->data.size);
	}
        else { // We've finished writing but not the block yet
	    r_fhand.blk_curoffset += writeSz;
	}
    }
    return writtenBytes;
}

