#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

// Time and date entry
struct __attribute__((__packed__)) dir_entry_timedate_t {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

// Super block
struct __attribute__((__packed__)) superblock_t {
    uint8_t fs_id[8];
    uint16_t block_size;
    uint32_t file_system_block_count;
    uint32_t fat_start_block;
    uint32_t fat_block_count;
    uint32_t root_dir_start_block;
    uint32_t root_dir_block_count;
};

// Directory entry
struct __attribute__((__packed__)) dir_entry_t {
    uint8_t status;
    uint32_t starting_block;
    uint32_t block_count;
    uint32_t size;
    struct dir_entry_timedate_t create_time;
    struct dir_entry_timedate_t modify_time;
    uint8_t filename[31];
    uint8_t unused[6];
};


struct dir_list {
    struct dir_entry_t* dirEntry;
    struct dir_list* next;
};

// superblock info
struct superblock_t* sb;

// global directory list
struct dir_list* FIRST = NULL;

/*
Main function and its helper
1) diskinfo - printinfo, fatinfo
2) disklist - findDir, printDir
3) diskget - getFile, findFile
4) diskput - createDir, creatFat
5) diskfix

general function
    freeDir
*/

/*
Purpose: get and print FAT info
*/
void fatinfo (void* address) {
    int bc = ntohl(sb->file_system_block_count);
    int block_size = ntohl(sb->block_size << 16);
    int fat_start = ntohl(sb->fat_start_block);
    int cur_block = fat_start*block_size;
    int end_block = block_size*ntohl(sb->fat_block_count);
    char p[4];
    int reserved = 0;
    int allocated = 0;

    memcpy(p, address + cur_block, 4);

    while (cur_block < end_block) {
        if (p[3] == 1) {
            reserved++;
        } else if (p[3] != 0 || p[2] != 0) {
            allocated++;
        }
        cur_block += 4;
        memcpy(p, address + cur_block, 4);
    }

    printf("FAT information:\n");
    printf("Free Blocks: %d\n", bc - reserved - allocated);
    printf("Reserved Blocks: %d\n", reserved);
    printf("Allocated Blocks: %d\n", allocated);

}

/*
Purpose: print superblock info and call fatinfo
*/
void printinfo(void* address) {
    int bc = ntohl(sb->file_system_block_count);
    int rds = ntohl(sb->root_dir_start_block);
    int rdb = ntohl(sb->root_dir_block_count);
    int block_size = ntohl(sb->block_size << 16);
    int fat_start = ntohl(sb->fat_start_block);

    printf("Super block information:\n");
    printf("Block size: %d\n", block_size);
    printf("Block count: %d\n", bc);
    printf("FAT starts: %d\n", fat_start);
    printf("FAT blocks: %d\n", ntohl(sb->fat_block_count));
    printf("Root directory start: %d\n", rds);
    printf("Root directory blocks: %d\n", rdb);

    printf("\n");

    fatinfo(address);

}

/*
Purpose: displays information about the file system
*/
void diskinfo(int argc, char* argv[]) {
    int fd = open(argv[1], O_RDWR);
    struct stat buffer;
    if (fstat(fd, &buffer) != 0) {
        printf("fstat unsuccessful");
        exit(0);
    }

    //tamplate:   pa=mmap(addr, len, prot, flags, fildes, off);
    //c will implicitly cast void* to char*, while c++ does NOT
    void* address = mmap(NULL, buffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    sb = (struct superblock_t*)address;
    printinfo(address);

    munmap(address,buffer.st_size);
    close(fd);
    

}

/*
Purpose: free global directory list
*/
void freeDir() {
    struct dir_list *p = FIRST;
    struct dir_list *cur;

    while (p != NULL) {
        cur = p;
        p = p->next;
        free(cur);
    }
}

/*
Purpose: print info from global directory list
*/
void printDir() {
    struct dir_list *p = FIRST;
    char *filename = malloc(31);

    while (p != NULL) {
        if (p->dirEntry->status == 0x03) {
            printf("F ");
        } else {
            printf("D ");
        }

        printf("%10d ", ntohl(p->dirEntry->size));

        memcpy(filename, (char*)(p->dirEntry->filename), sizeof(p->dirEntry->filename));
        printf("%30s ", filename);

        printf("%04d/%02d/%02d %02d:%02d:%02d\n", ntohs(p->dirEntry->create_time.year),
        (p->dirEntry->create_time.month), (p->dirEntry->create_time.day),
        (p->dirEntry->create_time.hour), (p->dirEntry->create_time.minute),
        (p->dirEntry->create_time.second));

        p = p->next;
    }
}

/*
Purpose: create global directory list
*/
void findDir(void* address) {
    int dir_start = ntohl(sb->block_size << 16)*ntohl(sb->root_dir_start_block);
    int dir_end = dir_start + (ntohl(sb->block_size << 16)*ntohl(sb->root_dir_block_count));
    struct dir_entry_t* de;
    struct dir_list* cur;
    struct dir_list* p;


    while (dir_start < dir_end) {
        de = (struct dir_entry_t*) (address + dir_start);
        cur = (struct dir_list*)malloc(sizeof(struct dir_list));
        cur->dirEntry = de;
        cur->next = NULL;

        if (FIRST == NULL) {
            if ((int)cur->dirEntry->status != 0) {
                FIRST = cur;
            } else {
                free(cur);
            }
        } else {
            if ((int)cur->dirEntry->status != 0) {
                p = FIRST;
                while (p->next != NULL) {
                    p = p->next;
                }
                p->next = cur;
            } else {
                free(cur);
            }
        }
        dir_start += 64;
    }


}

/*
Purpose: displays the contents of the root directory
*/
void disklist(int argc, char* argv[]) {
    int fd = open(argv[1], O_RDWR);
    struct stat buffer;
    if (fstat(fd, &buffer) == -1) {
        printf("fstat unsuccessful");
        exit(0);
    }

    void* address = mmap(NULL, buffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    sb = (struct superblock_t*)address;

    findDir(address);
    printDir();


    freeDir();
    munmap(address,buffer.st_size);
    close(fd);
}

/*
Purpose: see if file with filename is in global directory list
need to call findDir() first to create global directory list before call this function
*/
struct dir_list* findFile(char* filename) {
    struct dir_list* cur = FIRST;

    while (cur != NULL) {

        if (strcmp((char*)cur->dirEntry->filename, filename) == 0) {
            return cur;
        }

        cur = cur->next;
    }

    printf("File not found\n");
    exit(0);
}

/*
Purpose: copies a info from the file system to the file in current directory in Linux
*/
void getFile(struct dir_list* cur, char* filename, void* address) {
    FILE* fp = fopen(filename, "w+");
    char p[4];
    int block_size = ntohl(sb->block_size << 16);
    int fat_start = ntohl(sb->fat_start_block);
    int cur_block = fat_start*block_size;
    int size = ntohl(cur->dirEntry->starting_block)*4;
    cur_block += size;
    int num_of_block = ntohl(cur->dirEntry->block_count);
    int fatCopy;
    
    memcpy(p, address + cur_block, 4);

    fwrite((char*)address + ntohl(cur->dirEntry->starting_block)*block_size, 1, 512, fp);

    while (num_of_block > 1) {
        fatCopy = block_size*p[3];
        fwrite((char*)address + fatCopy, 1, 512, fp);

        cur_block += 4;
        memcpy(p, address + cur_block, 4);
        num_of_block--;
    }

    fclose(fp);
}

/*
Purpose: copies a file from the file system to the current directory in Linux
*/
void diskget(int argc, char* argv[]) {
    if (argc != 4) {
        printf("to run:./diskget *dmg file* *file to get from disk* *filename to copy to*\n");
        exit(0);
    }

    int fd = open(argv[1], O_RDWR);
    struct stat buffer;
    if (fstat(fd, &buffer) == -1) {
        printf("fstat unsuccessful");
        exit(0);
    }

    void* address = mmap(NULL, buffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    sb = (struct superblock_t*)address;

    findDir(address);

    struct dir_list* p = findFile(argv[2]);
    getFile(p, argv[3], address);

    freeDir();
    munmap(address,buffer.st_size);
    close(fd);
}

/*
Purpose: copy info from file to FAT
*/
void createFat(void* address, struct stat bufFile, char* filename, int final[20]) {
    int block_size = ntohl(sb->block_size << 16);
    int fat_start = ntohl(sb->fat_start_block);
    int cur_block = fat_start*block_size;
    int end_block = block_size*ntohl(sb->fat_block_count);
    char fatblock[512];
    char fatspace[4];
    int count = 0;
    int check = 0;
    int offset = 0;
    uint8_t cur[4] = {0,0,0,0};
    uint8_t end[4] = {0xff,0xff,0xff,0xff};
    FILE* fp = fopen(filename, "r");


    memcpy(fatspace, address + cur_block, 4);

    while (cur_block < end_block) {
        if (fatspace[3] == 0 && fatspace[2] == 0) {
            fseek(fp, offset, SEEK_CUR);
            final[check] = count;
            fread(fatblock, 1, (int)bufFile.st_size, fp);
            memcpy(address + (count*block_size), fatblock, (int)bufFile.st_size);
            check++;
            offset += block_size;
            cur[3] = (uint8_t)count;
            memcpy(address + cur_block, cur, 4);
            if (offset > (int)bufFile.st_size) {
                break;
            }
        }
        count++;
        cur_block += 4;
        memcpy(fatspace, address + cur_block, 4);
    }
    memcpy(address + cur_block, end, 4);

    fclose(fp);

}

/*
Purpose: copy info from file to file system directory
*/
void createDir(void* address, struct stat bufFile, char* filename) {
    int dir_start = ntohl(sb->block_size << 16)*ntohl(sb->root_dir_start_block);
    int dir_end = dir_start + (ntohl(sb->block_size << 16)*ntohl(sb->root_dir_block_count));
    struct dir_entry_t* de = (struct dir_entry_t*)malloc(sizeof(struct dir_entry_t));
    struct dir_entry_t* cur;
    uint8_t end[6] = {0x00, 0xff,0xff,0xff,0xff,0xff};
    struct tm sec = *localtime(&bufFile.st_ctime);


    int fatStoring[20];
    createFat(address, bufFile, filename, fatStoring);


    de->status = (uint8_t)3;
    de->starting_block = (uint32_t)fatStoring[0] << 24;
    de->block_count = (uint32_t)bufFile.st_blocks << 24;
    de->size = (uint32_t)bufFile.st_size << 24;
    de->create_time.year = htons(sec.tm_year + 1900);
    de->create_time.month = sec.tm_mon + 1;
    de->create_time.day = sec.tm_mday;
    de->create_time.hour = sec.tm_hour;
    de->create_time.minute = sec.tm_min;
    de->create_time.second = sec.tm_sec;

    // memcpy(de->modify_time, bufFile.st_mtime, 7);
    memcpy(de->filename, filename, strlen(filename));
    memcpy(de->unused, end, 5);

    while (dir_start < dir_end) {
        cur = (struct dir_entry_t*) (address + dir_start);
        if ((int)cur->status == 0) {
            memcpy((address + dir_start), de, 64);
            break;
        }
        
        dir_start += 64;
    }
}

/*
Purpose: copies a file from the current Linux directory into the file system
*/
void diskput(int argc, char* argv[]) {
    int fd = open(argv[1], O_RDWR);
    struct stat buffer;
    if (fstat(fd, &buffer) == -1) {
        printf("fstat unsuccessful");
        exit(0);
    }

    int file = open(argv[2], O_RDWR);
    struct stat bufFile;
    if (fstat(file, &bufFile) == -1) {
        printf("File not found\n");
        exit(0);
    }

    void* address = mmap(NULL, buffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    sb = (struct superblock_t*)address;

    findDir(address);

    createDir(address, bufFile, argv[2]);



    freeDir();
    munmap(address,buffer.st_size);
    close(file);
    close(fd);
}

/*
Purpose: fix file system
*NOT implement*
*/
void diskfix(int argc, char* argv[]) {
    exit(0);
}

/*
Purpose: invoke all main function
*/
int main(int argc, char* argv[])
{
    #if defined(PART1)
        diskinfo(argc, argv);
    #elif defined(PART2)
        disklist(argc, argv);
    #elif defined(PART3)
        diskget(argc, argv);
    #elif defined(PART4)
        diskput(argc,argv);
    #elif defined(PART5)
        diskfix(argc,argv);
    #else
        printf("error PART[12345] must be defined");
    #endif

    return 0;
}
