#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"

const uchar bit_masks[8] = {0b00000001, 0b00000010, 0b00000100, 0b00001000,
    0b00010000, 0b00100000, 0b01000000, 0b10000000};

uchar read_bit(uchar* bitmap, uint index) {
    uint location = index / 8;
    uint offset = index % 8;
    return (bitmap[location] & bit_masks[offset]) >> offset;
}

uchar test_and_set_bit(uchar* bitmap, uint index) {
    uint location = index / 8;
    uint offset = index % 8;
    uchar old = (bitmap[location] & bit_masks[offset]) >> offset;
    bitmap[location] |= bit_masks[offset];
    return old;
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
    uint inode_blks = (super_blk.ninodes + IPB - 1) / IPB;
    struct dinode inode_tbl[inode_blks * IPB];
    for (uint i = 0; i < inode_blks; ++i) {
        fread(buffer, 1, BSIZE, image);
        memcpy(&inode_tbl[IPB * i], buffer, BSIZE);
    }

    printf("Inode Table:\n");
    for (uint i = 0; i < super_blk.ninodes; ++i) {
        if (inode_tbl[i].type != 0)
            printf("[%d] type = %d, size = %d, nlinks = %d\n",
                i, inode_tbl[i].type, inode_tbl[i].size, inode_tbl[i].nlink);
    }
    printf("\n");

    // Skip Empty Block
    fseek(image, BSIZE, SEEK_CUR);

    // Read Data Bitmap
    uint bitmap_blks = (super_blk.size + BPB - 1) / BPB;
    uchar bitmap[bitmap_blks * BSIZE];
    for (uint i = 0; i < bitmap_blks; ++i) {
        fread(buffer, 1, BSIZE, image);
        memcpy(bitmap, buffer, BSIZE);
    }

    printf("Data Bitmap:\n");
    for (uint i = 0; i < super_blk.size; ++i) {
        printf("%d", read_bit(bitmap, i));
        if ((i + 1) % 64 == 0)
            printf("\n");
    }
    printf("\n");

    // Initialize Reconstructed Data Bitmap
    uchar bitmap_rec[bitmap_blks * BSIZE];
    memset(bitmap_rec, 0, sizeof(bitmap_rec));
    test_and_set_bit(bitmap_rec, 0);
    test_and_set_bit(bitmap_rec, 1);
    for (uint i = 0; i < inode_blks; ++i) {
        test_and_set_bit(bitmap_rec, i + 2);
    }
    test_and_set_bit(bitmap_rec, inode_blks + 2);
    for (uint i = 0; i < bitmap_blks; ++i) {
        test_and_set_bit(bitmap_rec, i + inode_blks + 3);
    }

    // Check Inode Table
    for (uint i = 0; i < super_blk.ninodes; ++i) {
        if (inode_tbl[i].type == 0) {
            continue;
        } else if (inode_tbl[i].type > T_DEV) {
            fprintf(stderr, "bad inode.\n");
            exit(1);
        } else {
            if (i == 1 && inode_tbl[i].type != T_DIR) {
                fprintf(stderr, "root directory does not exist.\n");
                exit(1);
            }
            for (uint j = 0; j < NDIRECT; ++j) {
                if (inode_tbl[i].addrs[j] == 0) {
                    break;
                } else if (inode_tbl[i].addrs[j] > super_blk.size) {
                    fprintf(stderr, "bad address in inode.\n");
                    exit(1);
                } else if (test_and_set_bit(bitmap_rec, inode_tbl[i].addrs[j]) != 0) {
                    fprintf(stderr, "address used more than once.\n");
                    exit(1);
                }
            }
            if (inode_tbl[i].addrs[NDIRECT] == 0) {
            } else if (inode_tbl[i].addrs[NDIRECT] > super_blk.size) {
                fprintf(stderr, "bad address in inode.\n");
                exit(1);
            } else if (test_and_set_bit(bitmap_rec, inode_tbl[i].addrs[NDIRECT]) != 0) {
                fprintf(stderr, "address used more than once.\n");
                exit(1);
            } else {
                uint indirect_blk[NINDIRECT];
                fseek(image, inode_tbl[i].addrs[NDIRECT] * BSIZE, SEEK_SET);
                fread(indirect_blk, 1, BSIZE, image);
                for (uint j = 0; j < NINDIRECT; ++j) {
                    if (indirect_blk[j] == 0) {
                        break;
                    } else if (indirect_blk[j] > super_blk.size) {
                        fprintf(stderr, "bad address in inode.\n");
                        exit(1);
                    } else if (test_and_set_bit(bitmap_rec, indirect_blk[j]) != 0) {
                        fprintf(stderr, "address used more than once.\n");
                        exit(1);
                    }
                }
            }
        }   
    }

    // Compare Data Bitmap
    for (uint i = 0; i < bitmap_blks * BSIZE; ++i) {
        if (bitmap_rec[i] == bitmap[i]) {
            continue;
        } else if (bitmap_rec[i] > bitmap[i]) {
            fprintf(stderr, "address used by inode but marked free in bitmap.\n");
            exit(1);
        } else {
            fprintf(stderr, "bitmap marks block in use but it is not in use.\n");
            exit(1);
        }
    }

    // Read Directory Data
    printf("Directory Data:\n");
    for (uint i = 0; i < super_blk.ninodes; ++i) {
        if (inode_tbl[i].type == T_DIR) {
            printf("[%d]\n", i);
            for (uint j = 0; j < NDIRECT; ++j) {
                if (inode_tbl[i].addrs[j] != 0) {
                    fseek(image, inode_tbl[i].addrs[j] * BSIZE, SEEK_SET);
                    fread(buffer, 1, BSIZE, image);
                    for (struct dirent* dir = (struct dirent*)buffer;
                        dir < (struct dirent*)(buffer + BSIZE); ++dir) {
                        if (dir->inum != 0)
                            printf("%s: %d\n", dir->name, dir->inum);
                    }
                }
            }
            if (inode_tbl[i].addrs[NDIRECT] != 0) {
                uint indirect_blk[NINDIRECT];
                fseek(image, inode_tbl[i].addrs[NDIRECT] * BSIZE, SEEK_SET);
                fread(indirect_blk, 1, BSIZE, image);
                for (uint j = 0; j < NINDIRECT; ++j) {
                    if (indirect_blk[j] != 0) {
                        fseek(image, indirect_blk[j] * BSIZE, SEEK_SET);
                        fread(buffer, 1, BSIZE, image);
                        for (struct dirent* dir = (struct dirent*)buffer;
                            dir < (struct dirent*)(buffer + BSIZE); ++dir) {
                            if (dir->inum != 0)
                                printf("%s: %d\n", dir->name, dir->inum);
                        }
                    }
                }
            }
        }
    }

    fclose(image);
    return 0;
}