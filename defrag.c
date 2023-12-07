#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "defrag.h"
unsigned char *iBuff_ibAddr;
/* Buffer to read input file */
unsigned char *indirectBlockBuffer;
/* Buffer to write output file */
unsigned char *oBuff;
int nthindblock = 0;
int nthdatablock = 0; 
unsigned char *oBuff_ibAddr;
unsigned char *oBuff_i2bAddr;
unsigned char *iBuff_i3bAddr;

int main(int argc, char *argv[]) {

    FILE *in;
    char *fileName = argv[1];

    // Allocate memory for a buffer of BUFFER_SIZE bytes
    indirectBlockBuffer = malloc(MAX_BUFFER_SIZE);

    in = fopen(fileName, "r");

    int bytes_read = fread(indirectBlockBuffer, sizeof(unsigned char), MAX_BUFFER_SIZE, in);

    fclose(in);

    oBuff = malloc(MAX_BUFFER_SIZE);

    //Defrag the disk image
    int defragged = defragDiskImage(indirectBlockBuffer, oBuff);

    if (defragged != 0) {
        exit(EXIT_FAILURE);
    }

    //Create an output file
    FILE *out = fopen( OUTPUT_FILE_NAME, "w+" );

    // Write the contents of the buffer to the output file
    int bytes_written = fwrite(oBuff, sizeof(unsigned char), bytes_read, out);

    // Close the output file and free the allocated memory
    fclose(out);
    free(indirectBlockBuffer);
    free(oBuff);

    return 0; // Exit the program successfully
}

// Function to read a 32-bit integer from a specific memory location
int readIntAt(unsigned char *p)
{
    return *(p+3) * 256 * 256 * 256 + *(p+2) * 256 * 256 + *(p+1) * 256 + *p;
}

// Extract and write the four bytes of the integer value to the memory location
void writeIntAt(unsigned char *p, int value) {

    p[3] = (unsigned char)((value >> 24) & 0xFF);

    // Byte 2: Second most significant byte
    p[2] = (unsigned char)((value >> 16) & 0xFF);

    // Byte 1: Third most significant byte
    p[1] = (unsigned char)((value >> 8) & 0xFF);

    // Byte 0: Least significant byte (rightmost)
    p[0] = (unsigned char)(value & 0xFF);

}

// Function to replace a block in the oBuff with a block from the indirectBlockBuffer
void replaceblock(int inBlockNum, int outBlockNum) {
    // Calculate the starting positions of the source and destination blocks
    size_t srcStart = DATAOFFSET + inBlockNum * DATABLKSIZE;
    size_t destStart = DATAOFFSET + outBlockNum * DATABLKSIZE;

    // Calculate the ending positions of the source and destination blocks
    size_t srcEnd = srcStart + DATABLKSIZE;
    size_t destEnd = destStart + DATABLKSIZE;

    // Use memcpy for efficient block copying
    memcpy(oBuff + destStart, indirectBlockBuffer + srcStart, DATABLKSIZE);
}


int defragDiskImage(unsigned char* indirectBlockBuffer, unsigned char* oBuff) {

    // Process superblock data from the buffer
    struct superblock sb = *(struct superblock*)(indirectBlockBuffer + 512);

    DATABLKSIZE = sb.blocksize;
    DATAOFFSET = 1024 + (DATABLKSIZE * (sb.data_offset));
    int i = 0;

    // Copy inital section
    // Assuming you want to copy 1024 bytes from indirectBlockBuffer to oBuff
    memcpy(oBuff, indirectBlockBuffer, 1024);
    
    int numberofindoes = DATABLKSIZE * (sb.data_offset - sb.inode_offset) / sizeof(struct inode);
    int nthinode = 0;
    int inode_address = 1024 + (DATABLKSIZE * sb.inode_offset);
    
    for (nthinode; nthinode < numberofindoes; nthinode++, inode_address += sizeof(struct inode)) {
        struct inode inode = *(struct inode*)(indirectBlockBuffer + inode_address);

        if (inode.nlink == 0) {
            memcpy(oBuff + inode_address, &inode, sizeof(struct inode));
            continue;
        }

        // Initializing the counter for data blocks
        int nthdatablock = 0;   
        // Initializing the counter for indirect blocks
        int nthindblock = 0;

        // Loop over direct data blocks in the inode
        for (; nthdatablock < 10; nthdatablock++) {
            // Check if the current direct block is valid (not -1)
            if (inode.dblocks[nthdatablock] != -1) {
                // Replace the current data block with a new block at FILLEDBLOCK position
                replaceblock(inode.dblocks[nthdatablock], FILLEDBLOCK);
                // Update the inode's direct block entry to the new block index
                inode.dblocks[nthdatablock] = FILLEDBLOCK;
                // Increment the filled block counter
                FILLEDBLOCK++;
            }
            else {
                // Break the loop if an invalid block (-1) is encountered
                break;
            }
        }

        // Loop over indirect blocks in the inode
        for (; nthindblock < 4; nthindblock++) {
            // Check if the current indirect block is invalid (-1), then exit loop
            if (inode.iblocks[nthindblock] == -1) {
                break; 
            }

            // Calculate the address of the indirect block in the input buffer
            unsigned char *iBuff_ibAddr = indirectBlockBuffer + (inode.iblocks[nthindblock] * DATABLKSIZE) + DATAOFFSET;
            // Update the inode's indirect block entry to the new block index
            inode.iblocks[nthindblock] = FILLEDBLOCK;
            
            // Calculate the address to write the indirect block data in the output buffer
            unsigned char *oBuff_ibAddr = oBuff + (FILLEDBLOCK * DATABLKSIZE) + DATAOFFSET;
            
            // Increment the filled block counter
            FILLEDBLOCK++;
            // Reset the data block counter for processing indirect block's data blocks
            nthdatablock = 0; 
            // Loop over each data block within the indirect block
            for (; nthdatablock < (DATABLKSIZE / sizeof(int)); nthdatablock++) {
                // Read the value of the current data block index
                int ibval = readIntAt(iBuff_ibAddr + nthdatablock * sizeof(int));
                // Check if the current data block is unused
                if (ibval == -1) {
                    // Mark the corresponding block in the output buffer as unused
                    writeIntAt(oBuff_ibAddr + nthdatablock * sizeof(int), -1);
                }
                else {
                    // Mark the location of the data block in the output buffer and replace the block
                    writeIntAt(oBuff_ibAddr + nthdatablock * sizeof(int), FILLEDBLOCK);
                    // Replace the old block with a new block
                    replaceblock(ibval, FILLEDBLOCK);
                    // Incrementing the filled block counter
                    FILLEDBLOCK++;
                }
            }
        }


        // Checking if the inode has a double indirect block (i2block)
        if (inode.i2block != -1){

            // Calculating the address of the double indirect block in the buffer
            unsigned char *iBuff_i2bAddr = indirectBlockBuffer + DATAOFFSET + inode.i2block * DATABLKSIZE;
            
            // Creating an array to hold the indexes of the double indirect block
            int i2block[(DATABLKSIZE / sizeof(int))];

            // Looping over the double indirect block to fill it with data
            for (int i = 0; i < (DATABLKSIZE / sizeof(int)); i++) {
                // Reading integer values at each index of the double indirect block
                i2block[i] = readIntAt(iBuff_i2bAddr + i * sizeof(int));
            }
            // Marking the inode's double indirect block as filled
            inode.i2block = FILLEDBLOCK;
            // Calculating the address to write the double indirect block data in the output buffer
            unsigned char *oBuff_i2bAddr = oBuff + (FILLEDBLOCK * DATABLKSIZE) + DATAOFFSET;

            // Incrementing the filled block counter
            FILLEDBLOCK++;
            // Initializing a variable to iterate over indirect blocks within the double indirect block
            int nthindblock = 0;  

            // Iterating over each indirect block within the double indirect block
            for (; nthindblock < (DATABLKSIZE / 4); nthindblock++) {
                // Check if the current indirect block is unused
                if (i2block[nthindblock] == -1){
                    // Mark the corresponding block in the output buffer as unused
                    writeIntAt(oBuff_i2bAddr + nthindblock * 4, -1);
                }
                else {
                    // Calculating the address of the indirect block in the input buffer
                    unsigned char *iBuff_ibAddr = indirectBlockBuffer + DATAOFFSET + i2block[nthindblock] * DATABLKSIZE;
                    // Writing the index of the new location of the indirect block in the output buffer
                    writeIntAt(oBuff_i2bAddr + nthindblock * 4, FILLEDBLOCK);
                    
                    // Calculating the address to write the indirect block data in the output buffer
                    unsigned char *oBuff_ibAddr = oBuff + (FILLEDBLOCK * DATABLKSIZE) + DATAOFFSET;
                    
                    // Incrementing the filled block counter
                    FILLEDBLOCK++;
                    // Initializing a variable to iterate over data blocks within the indirect block
                    int nthdatablock = 0;
                    // Iterating over each data block within the indirect block
                    for(; nthdatablock < (DATABLKSIZE / 4); nthdatablock++) {

                        // Reading the value of the current data block index
                        int ibval = readIntAt(iBuff_ibAddr + nthdatablock * 4);
                        // Check if the current data block is unused
                        if (ibval == -1) {
                            // Mark the corresponding block in the output buffer as unused
                            writeIntAt(oBuff_ibAddr + nthdatablock * sizeof(int), -1);
                        }
                        else {
                            // Mark the location of the data block in the output buffer and replace the block
                            writeIntAt(oBuff_ibAddr + nthdatablock * sizeof(int), FILLEDBLOCK);
                            replaceblock(ibval, FILLEDBLOCK);
                            // Incrementing the filled block counter
                            FILLEDBLOCK++;
                        }
                    }
                }
            }

        }

        // Checking if the inode has a triple indirect block (i3block)
        if (inode.i3block != -1){
            // Calculating the address of the triple indirect block in the buffer
            unsigned char *iBuff_i3bAddr = indirectBlockBuffer + DATAOFFSET + inode.i3block * DATABLKSIZE;
            
            // Creating an array to hold the indexes of the triple indirect block
            int i3block[(DATABLKSIZE / sizeof(int))];

            // Looping over the triple indirect block to fill it with data
            for (int i = 0; i < (DATABLKSIZE / sizeof(int)); i++) {
                // Reading integer values at each index of the triple indirect block
                i3block[i] = readIntAt(iBuff_i3bAddr + i * sizeof(int));
            }

            // Marking the inode's triple indirect block as filled
            inode.i3block = FILLEDBLOCK;
            // Calculating the address to write the triple indirect block data in the output buffer
            unsigned char *oBuff_i3bAddr = oBuff + DATAOFFSET + FILLEDBLOCK * DATABLKSIZE;
            // Incrementing the filled block counter
            FILLEDBLOCK++;

            // Initializing a variable to iterate over double indirect blocks within the triple indirect block
            int ni2block = 0;  
            // Iterating over each double indirect block within the triple indirect block
            for (; ni2block < (DATABLKSIZE / sizeof(int)); ni2block++) {
                // Check if the current double indirect block is unused
                if (i3block[ni2block] == -1){
                    // Mark the corresponding block in the output buffer as unused
                    writeIntAt(oBuff_i3bAddr + ni2block * sizeof(int), -1);
                }
                else {
                    // Calculating the address of the double indirect block in the input buffer
                    unsigned char *iBuff_i2bAddr = indirectBlockBuffer + DATAOFFSET + i3block[ni2block] * DATABLKSIZE;

                    // Writing the index of the new location of the double indirect block in the output buffer
                    writeIntAt(oBuff_i3bAddr + ni2block * sizeof(int), FILLEDBLOCK);
                    // Calculating the address to write the double indirect block data in the output buffer
                    unsigned char *oBuff_i2bAddr = oBuff + DATAOFFSET + FILLEDBLOCK * DATABLKSIZE;
                    // Incrementing the filled block counter
                    FILLEDBLOCK++;
                    
                    // Creating an array to hold the indexes of the double indirect block
                    int i2block[(DATABLKSIZE / sizeof(int))];

                    // Looping over the double indirect block to fill it with data
                    for (int i = 0; i < (DATABLKSIZE / sizeof(int)); i++) {
                        // Reading integer values at each index of the double indirect block
                        i2block[i] = readIntAt(iBuff_i2bAddr + i * sizeof(int));
                    }

                    // Initializing a variable to iterate over indirect blocks within the double indirect block
                    int nthindblock = 0;
                    // Iterating over each indirect block within the double indirect block
                    for (; nthindblock < (DATABLKSIZE / sizeof(int)); nthindblock++) {

                        // Check if the current indirect block is unused
                        if (i2block[nthindblock] == -1){
                            // Mark the corresponding block in the output buffer as unused
                            writeIntAt(oBuff_i2bAddr + nthindblock * sizeof(int), -1);
                        }
                        else {
                            // Calculating the address of the indirect block in the input buffer
                            unsigned char *iBuff_ibAddr = indirectBlockBuffer + DATAOFFSET + i2block[nthindblock] * DATABLKSIZE;
                            
                            // Writing the index of the new location of the indirect block in the output buffer
                            writeIntAt(oBuff_i2bAddr + nthindblock * sizeof(int), FILLEDBLOCK);
                            // Calculating the address to write the indirect block data in the output buffer
                            unsigned char *oBuff_ibAddr = oBuff + DATAOFFSET + FILLEDBLOCK * DATABLKSIZE;
                            // Incrementing the filled block counter
                            FILLEDBLOCK++;
                            
                            // Initializing a variable to iterate over data blocks within the indirect block
                            int nthdatablock = 0;
                            // Iterating over each data block within the indirect block
                            for (; nthdatablock < (DATABLKSIZE / sizeof(int)); nthdatablock++) {
                                // Reading the value of the current data block index
                                int ibval = readIntAt(iBuff_ibAddr + nthdatablock * sizeof(int));

                                // Check if the current data block is unused
                                if (ibval == -1) {
                                    // Mark the corresponding block in the output buffer as unused
                                    writeIntAt(oBuff_ibAddr + nthdatablock * sizeof(int), -1);
                                }
                                else {
                                    // Mark the location of the data block in the output buffer and replace the block
                                    writeIntAt(oBuff_ibAddr + nthdatablock * sizeof(int), FILLEDBLOCK);
                                    // Replace the old block with a new block
                                    replaceblock(ibval, FILLEDBLOCK);
                                    // Incrementing the filled block counter
                                    FILLEDBLOCK++;
                                }
                            }
                        }
                    }
                }
            }
        }

        memcpy(oBuff + inode_address, &inode, sizeof(struct inode));
    }


    int index = FILLEDBLOCK; 
    for (index; index < (sb.swap_offset - sb.data_offset - 1); index++) {
        writeIntAt(oBuff + 1024 + (DATABLKSIZE * (index + sb.data_offset)), index+1);
    }
    // Copy the swap blocks
    int swapbyte = (1024 + (DATABLKSIZE * (sb.swap_offset)));
    memcpy(oBuff + swapbyte, indirectBlockBuffer + swapbyte, MAX_BUFFER_SIZE - swapbyte);
    
    writeIntAt(oBuff + 1024 + (DATABLKSIZE * (index + sb.data_offset)), -1);
    writeIntAt(oBuff + 512 + 20, FILLEDBLOCK);


    return 0;
}