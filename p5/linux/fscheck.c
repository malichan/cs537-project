#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"

uchar read_bit(uchar* bitmap, uint index) {
    const uchar masks[8] = {0b00000001, 0b00000010, 0b00000100, 0b00001000,
        0b00010000, 0b00100000, 0b01000000, 0b10000000};

    uint location = index / 8;
    uint offset = index % 8;
    return (bitmap[location] & masks[offset]) >> offset;
}

int main(int argc, char* argv[]) {
    FILE* image = fopen(argv[1], "rb");
    if (image == NULL) {
        fprintf(stderr, "image not found.\n");
        exit(1);
    }

    uchar buffer[BSIZE];

    // Skip Empty Block
    fseek(image, BSIZE, SEEK_CUR);

    // Read Superblock
    struct superblock super_blk;
    fread(buffer, 1, BSIZE, image);
    memcpy(&super_blk, buffer, sizeof(struct superblock));

    printf("Superblock:\n");
    printf("size = %d \n", super_blk.size);
    printf("nblocks = %d \n", super_blk.nblocks);
    printf("ninodes = %d \n", super_blk.ninodes);
    printf("\n");

    // Read Inode Table
    struct dinode inode_tbl[super_blk.ninodes];
    uint inode_blks = (super_blk.ninodes + IPB - 1) / IPB;
    for (uint i = 0; i < inode_blks; ++i) {
        fread(buffer, 1, BSIZE, image);
        memcpy(&inode_tbl[IPB * i], buffer, IPB * (i + 1) <= super_blk.ninodes ?
            BSIZE : (super_blk.ninodes - IPB * i) * sizeof(struct dinode));
    }

    printf("Inode Table:\n");
    for (uint i = 0; i < super_blk.ninodes; ++i) {
        if (inode_tbl[i].type != 0)
            printf("[%d] type = %d, size = %d\n", i, inode_tbl[i].type, inode_tbl[i].size);
    }
    printf("\n");

    // Skip Empty Block
    fseek(image, BSIZE, SEEK_CUR);

    // Read Data Bitmap
    uint bitmap_size = (super_blk.size + 8 - 1) / 8;
    uchar bitmap[bitmap_size];
    fread(buffer, 1, BSIZE, image);
    memcpy(bitmap, buffer, bitmap_size);

    printf("Data Bitmap:\n");
    for (uint i = 0; i < super_blk.size; ++i) {
        printf("%d", read_bit(bitmap, i));
        if ((i + 1) % 64 == 0)
            printf("\n");
    }
    printf("\n");

    fclose(image);
    return 0;
}