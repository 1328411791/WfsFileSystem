#include <stddef.h>
#define FS_BLOCK_SIZE 512
#define SUPER_BLOCK 1
#define BITMAP_BLOCK 1280
#define ROOT_DIR_BLOCK 1
#define MAX_DATA_IN_BLOCK 504
#define MAX_DIR_IN_BLOCK 8
#define MAX_FILENAME 8
#define MAX_EXTENSION 3
long TOTAL_BLOCK_NUM;

// 超级块中记录的，大小为 24 bytes（3个long），占用1块磁盘块
struct super_block
{
    long fs_size;   // size of file system, in blocks（以块为单位）
    long first_blk; // first block of root directory（根目录的起始块位置，以块为单位）
    long bitmap;    // size of bitmap, in blocks（以块为单位）
};

// 记录文件信息的数据结构,统一存放在目录文件里面，也就是说目录文件里面存的全部都是这个结构，大小为 40 bytes，占用1块磁盘块
struct file_directory
{
    char fname[MAX_FILENAME + 1]; // 文件名 (plus space for nul)
    char fext[MAX_EXTENSION + 1]; // 扩展名 (plus space for nul)
    time_t atime;                 /* 上次访问时间 */
    time_t mtime;                 /*上次修改时间 */
    // time_t ctime;                 /* 上次文件状态改变时间 */
    int uid;          //
    int mode;         //
    size_t fsize;     // 文件大小（file size）
    long nStartBlock; // 目录开始块位置（where the first block is on disk）
    int flag;         // indicate type of file. 0:for unused; 1:for file; 2:for directory
};

// 文件内容存放用到的数据结构，大小为 512 bytes，占用1块磁盘块
struct data_block
{
    size_t size;                  // 文件使用了这个块里面的多少Bytes
    long nNextBlock;              // （该文件太大了，一块装不下，所以要有下一块的地址）   long的大小为4Byte
    char data[MAX_DATA_IN_BLOCK]; // And all the rest of the space in the block can be used for actual data storage.
};

struct bitmap
{
    char fname[MAX_FILENAME + 1]; // 文件名 (plus space for nul)
    char fext[MAX_EXTENSION + 1]; // 扩展名 (plus space for nul)
    long hashcode;
    long nStartBlock; // 目录开始块位置（where the first block is on disk）
    int flag;         // indicate type of file. 0:for unused; 1:for file; 2:for directory
};

// 加载镜像目录
char *disk_path = "/tmp/diskimg";

// 辅助函数声明
void read_cpy_file_dir(struct file_directory *a, struct file_directory *b);
int read_cpy_data_block(long blk_no, struct data_block *data_blk);
int write_data_block(long blk_no, struct data_block *data_blk);
int divide_path(char *name, char *ext, const char *path, long *par_dir_stblk, int flag, int *par_size);
int exist_check(struct file_directory *file_dir, char *p, char *q, int *offset, int *pos, int size, int flag);
int set_blk_use(long start_blk, int flag);
int path_is_emp(const char *path);
int setattr(const char *path, struct file_directory *attr, int flag);
void ClearBlocks(long next_blk, struct data_block *data_blk);
int get_empty_blk(int num, long *start_blk);

// 功能函数声明
int get_fd_to_attr(const char *path, struct file_directory *attr);
int create_file_dir(const char *path, int flag);
int remove_file_dir(const char *path, int flag);
