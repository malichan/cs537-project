#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"

// #define DEBUG

const char* IMAGE_NOT_FOUND = "image not found.";
const char* ERR_INODE = "ERROR: bad inode.";
const char* ERR_ADDR_BND = "ERROR: bad address in inode.";
const char* ERR_DIR_ROOT = "ERROR: root directory does not exist.";
const char* ERR_DIR_FMT = "ERROR: directory not properly formatted.";
const char* ERR_DIR_PARENT = "ERROR: parent directory mismatch.";
const char* ERR_DBMP_FREE = "ERROR: address used by inode but marked free in bitmap.";
const char* ERR_DBMP_USED = "ERROR: bitmap marks block in use but it is not in use.";
const char* ERR_ADDR_DUP = "ERROR: address used more than once.";
const char* ERR_ITBL_USED = "ERROR: inode marked use but not found in a directory.";
const char* ERR_ITBL_FREE = "ERROR: inode referred to in directory but marked free.";
const char* ERR_FILE_REF = "ERROR: bad reference count for file.";
const char* ERR_DIR_REF = "ERROR: directory appears more than once in file system.";

const uchar bit_masks[8] = {0b00000001, 0b00000010, 0b00000100, 0b00001000,
    0b00010000, 0b00100000, 0b01000000, 0b10000000};

uchar get_bit(uchar* bitmap, uint index) {
    uint location = index >> 3;
    uint offset = index & 7;
    return (bitmap[location] & bit_masks[offset]) >> offset;
}

void set_bit(uchar* bitmap, uint index) {
    uint location = index >> 3;
    uint offset = index & 7;
    bitmap[location] |= bit_masks[offset];
}

uchar test_and_set_bit(uchar* bitmap, uint index) {
    uint location = index >> 3;
    uint offset = index & 7;
    uchar old = (bitmap[location] & bit_masks[offset]) >> offset;
    bitmap[location] |= bit_masks[offset];
    return old;
}

int main(int argc, char* argv[]) {
    FILE* image = fopen(argv[1], "rb");
    if (image == NULL) {
        fprintf(stderr, "%s\n", IMAGE_NOT_FOUND);
        exit(1);
    }

    uchar buffer[BSIZE];

    // Skip Empty Block
    fseek(image, BSIZE, SEEK_CUR);

    // Read Superblock
    struct superblock super_blk;
    fread(buffer, 1, BSIZE, image);
    memcpy(&super_blk, buffer, sizeof(struct superblock));

    // Read Inode Table
    uint inode_blks = (super_blk.ninodes + IPB - 1) / IPB;
    struct dinode inode_tbl[inode_blks * IPB];
    for (uint i = 0; i < inode_blks; ++i) {
        fread(buffer, 1, BSIZE, image);
        memcpy(&inode_tbl[IPB * i], buffer, BSIZE);
    }

    // Skip Empty Block
    fseek(image, BSIZE, SEEK_CUR);

    // Read Data Bitmap
    uint bitmap_blks = (super_blk.size + BPB - 1) / BPB;
    uchar bitmap[bitmap_blks * BSIZE];
    for (uint i = 0; i < bitmap_blks; ++i) {
        fread(buffer, 1, BSIZE, image);
        memcpy(bitmap, buffer, BSIZE);
    }

#ifdef DEBUG
    printf("Superblock:\n");
    printf("size = %d \n", super_blk.size);
    printf("nblocks = %d \n", super_blk.nblocks);
    printf("ninodes = %d \n", super_blk.ninodes);
    printf("\n");

    printf("Inode Table:\n");
    for (uint i = 0; i < super_blk.ninodes; ++i) {
        if (inode_tbl[i].type != 0)
            printf("[%d] type = %d, size = %d, nlink = %d\n",
                i, inode_tbl[i].type, inode_tbl[i].size, inode_tbl[i].nlink);
    }
    printf("\n");

    printf("Data Bitmap:\n");
    for (uint i = 0; i < super_blk.size; ++i) {
        printf("%d", get_bit(bitmap, i));
        if ((i + 1) % 64 == 0)
            printf("\n");
    }
    printf("\n");

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
#endif

    // Initialize Reconstructed Data Bitmap
    uchar bitmap_rec[bitmap_blks * BSIZE];
    memset(bitmap_rec, 0, sizeof(bitmap_rec));
    set_bit(bitmap_rec, 0);
    set_bit(bitmap_rec, 1);
    for (uint i = 0; i < inode_blks; ++i) {
        set_bit(bitmap_rec, i + 2);
    }
    set_bit(bitmap_rec, inode_blks + 2);
    for (uint i = 0; i < bitmap_blks; ++i) {
        set_bit(bitmap_rec, i + inode_blks + 3);
    }
    uint data_region = bitmap_blks + inode_blks + 3;

    // Initialize Directory-Related Data Structures
    ushort ref_count[super_blk.ninodes];
    memset(ref_count, 0, sizeof(ref_count));
    ref_count[1] = 1; 
    ushort parent_map[super_blk.ninodes];
    memset(parent_map, 0, sizeof(parent_map));
    ushort child_map[super_blk.ninodes];
    memset(child_map, 0, sizeof(child_map));
    child_map[1] = 1;

    // Enumerate Inodes - First Pass
    for (uint i = 0; i < super_blk.ninodes; ++i) {
        if (inode_tbl[i].type == 0) {
            continue;
        } else if (inode_tbl[i].type > T_DEV) {
            fprintf(stderr, "%s\n", ERR_INODE);
            exit(1);
        } else {
            if (i == 1 && inode_tbl[i].type != T_DIR) {
                fprintf(stderr, "%s\n", ERR_DIR_ROOT);
                exit(1);
            }
            for (uint j = 0; j < NDIRECT; ++j) {
                if (inode_tbl[i].addrs[j] == 0) {
                    // break;
                } else if (inode_tbl[i].addrs[j] > super_blk.size
                    || inode_tbl[i].addrs[j] < data_region) {
                    fprintf(stderr, "%s\n", ERR_ADDR_BND);
                    exit(1);
                } else if (test_and_set_bit(bitmap_rec, inode_tbl[i].addrs[j]) != 0) {
                    fprintf(stderr, "%s\n", ERR_ADDR_DUP);
                    exit(1);
                } else if (inode_tbl[i].type == T_DIR) {
                    fseek(image, inode_tbl[i].addrs[j] * BSIZE, SEEK_SET);
                    fread(buffer, 1, BSIZE, image);
                    struct dirent* dirent = (struct dirent*)buffer;
                    uint k = 0;
                    if (j == 0) {
                        if (strcmp(dirent[0].name, ".") != 0 || strcmp(dirent[1].name, "..") != 0) {
                            fprintf(stderr, "%s\n", ERR_DIR_FMT);
                            exit(1);
                        }
                        if (i == 1 && dirent[1].inum != 1) {
                            fprintf(stderr, "%s\n", ERR_DIR_ROOT);
                            exit(1);
                        }
                        parent_map[i] = dirent[1].inum;
                        k = 2;
                    }
                    for (; k < DPB; ++k) {
                        if (dirent[k].inum != 0) {
                            ref_count[dirent[k].inum] += 1;
                            child_map[dirent[k].inum] = i;
                        }
                    }
                }
            }
            if (inode_tbl[i].addrs[NDIRECT] == 0) {
            } else if (inode_tbl[i].addrs[NDIRECT] > super_blk.size
                || inode_tbl[i].addrs[NDIRECT] < data_region) {
                fprintf(stderr, "%s\n", ERR_ADDR_BND);
                exit(1);
            } else if (test_and_set_bit(bitmap_rec, inode_tbl[i].addrs[NDIRECT]) != 0) {
                fprintf(stderr, "%s\n", ERR_ADDR_DUP);
                exit(1);
            } else {
                uint indirect_blk[NINDIRECT];
                fseek(image, inode_tbl[i].addrs[NDIRECT] * BSIZE, SEEK_SET);
                fread(indirect_blk, 1, BSIZE, image);
                for (uint j = 0; j < NINDIRECT; ++j) {
                    if (indirect_blk[j] == 0) {
                        break;
                    } else if (indirect_blk[j] > super_blk.size
                        || indirect_blk[j] < data_region) {
                        fprintf(stderr, "%s\n", ERR_ADDR_BND);
                        exit(1);
                    } else if (test_and_set_bit(bitmap_rec, indirect_blk[j]) != 0) {
                        fprintf(stderr, "%s\n", ERR_ADDR_DUP);
                        exit(1);
                    } else if (inode_tbl[i].type == T_DIR) {
                        fseek(image, inode_tbl[i].addrs[j] * BSIZE, SEEK_SET);
                        fread(buffer, 1, BSIZE, image);
                        struct dirent* dirent = (struct dirent*)buffer;
                        for (uint k = 0; k < DPB; ++k) {
                            if (dirent[k].inum != 0) {
                                ref_count[dirent[k].inum] += 1;
                                child_map[dirent[k].inum] = i;
                            }
                        }
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
            fprintf(stderr, "%s\n", ERR_DBMP_FREE);
            exit(1);
        } else {
            fprintf(stderr, "%s\n", ERR_DBMP_USED);
            exit(1);
        }
    }

    // Enumerate Inodes - Second Pass
    for (uint i = 0; i < super_blk.ninodes; ++i) {
        if (inode_tbl[i].type == 0) {
            if (ref_count[i] > 0) {
                fprintf(stderr, "%s\n", ERR_ITBL_FREE);
                exit(1);
            }
        } else {
            if (ref_count[i] == 0) {
                fprintf(stderr, "%s\n", ERR_ITBL_USED);
                exit(1);
            }
            if (inode_tbl[i].type == T_DIR) {
                if (ref_count[i] > 1) {
                    fprintf(stderr, "%s\n", ERR_DIR_REF);
                    exit(1);
                }
                if (parent_map[i] != child_map[i]) {
                    fprintf(stderr, "%s\n", ERR_DIR_PARENT);
                    exit(1);
                }
            } else if (inode_tbl[i].type == T_FILE) {
                if (inode_tbl[i].nlink != ref_count[i]) {
                    fprintf(stderr, "%s\n", ERR_FILE_REF);
                    exit(1);
                }
            }
        }
    }

    fclose(image);
    return 0;
}