#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include "WFS.h"

/*磁盘（5M文件）的初始化步骤：
    首先文件系统有1个超级块、1个根目录信息块、1280个bitmap块，所以初始情况下，diskimg应该
    有1282块是被分配了的。
    1. 超级块初始化：
        ① 通过计算可以得到fs_size = 5MB/512Bytes = 10240 块 
        ② 然后 根目录信息块位置 应该是1281(前面 0~1280 共 1281 块）的块空间都被超级块和bitmap占用了)
                但是我们设置的时候first_blk设置为偏移量：1281，因为从0移动 1281 bit
                后的位置是1281，之后所有文件的块位置都用偏移量来表示而不是直接给出块的位置
        ③ 最后bitmap的大小是1280块

    2. 1280块bitmap的初始化：
        首先要清楚，前面1282bit(0~1281)为1，后面的为0，又因为1282/32=40余2，一个整数32位，所以只需要将
        一个大小为40的整形数组赋值为-1，（-1时整数的32bit全1，补码表示），然后再专门用一个整数移位30bit和31bit
        1280、1281这bit也为1，就完成了'1’的初始化。剩余的就全部都是初始化为0了，又因为10240bit只需要3个block
        3*512*8=12288>10240 就可以表示了，每个block 4096bit， 而我们刚才只用了1280+32bit，第一个block都没有
        完全初始化，所以这个block剩余的 4096-1312 = 2784bit 要完成初始化，2784bit刚好等于87个整数，所以直接将
        一个大小为87的整形数组继续写到文件中就可以了
        然后剩余2个block共8192bit，可以用大小为256的 全0 整形数组 来初始化，这样bitmap的初始化就大功告成了

    3. 然后我们要对第1282块（即编号为1281块位置的根目录块）进行初始化，因为一开始所有块都是空的，所以我们习惯性地将
        根目录块放在1281位置的块中，并且这个块我们默认是知道这就是根目录的 数据块 其他文件要使用块时不能使用它
        在后面文件安排中，我们可能会用 首次适应或者最优适应 这种方法来存放文件
        那么初始化第1282块，直接写入1个data_block（512byte）就可以了

*/

int main()
{
    FILE *fp=NULL;
    fp = fopen("diskimg1", "r+");//打开文件
	if (fp == NULL) {
		printf("打开文件失败，文件不存在\n");return 0;
    }

    //1. 初始化super_block     大小：1块
    struct super_block *super_blk = malloc(sizeof(struct super_block));//动态内存分配，申请super_blk

    super_blk->fs_size=10240;
    super_blk->first_blk=1281;//根目录的data_block在1281编号的块中（从0开始编号）
    super_blk->bitmap=BITMAP_BLOCK;

    fwrite(super_blk, sizeof(struct super_block), 1, fp);
    printf("initial super_block success!\n");

    //2. 初始化bitmap_block    大小:1280块= 5242880 bit
    if (fseek(fp, FS_BLOCK_SIZE * 1, SEEK_SET) != 0)//首先要将指针移动到文件的第二块的起始位置512
        fprintf(stderr, "bitmap fseek failed!\n");

    int a[40];//刚好大小为1280bit，可以用来初始化bitmap_block的前1280bit
    memset(a,-1,sizeof(a));
    fwrite(a,sizeof(a),1,fp);

        //然后我们还有2bit需要置1
    int b=0;
    int mask = 1;
	mask <<= 30;    //1281
	b |= mask;

	mask <<= 1;     //1280
	b |= mask;
    fwrite(&b, sizeof(int), 1, fp);

        //接着是包含这1282bit的块剩余的部分置0
    int c[87];
    memset(c,0,sizeof(c));
    fwrite(c,sizeof(c),1,fp);

        //最后要将bitmap剩余的1279块全部置0，用int数组设置，一个int为4byte
    int rest_of_bitmap=(1279*512)/4;
    int d[rest_of_bitmap];
    memset(d,0,sizeof(d));
    fwrite(d,sizeof(d),1,fp);

     printf("initial bitmap_block success!\n");

    //3. 初始化data_block   大小：10240-1282 块
    fseek(fp, FS_BLOCK_SIZE * (BITMAP_BLOCK + 1), SEEK_SET);
    struct data_block *root = malloc(sizeof(struct data_block));
    root->size = 0;
	root->nNextBlock = -1;
	root->data[0] = '\0';
    fwrite(root, sizeof(struct data_block), 1, fp); //写入磁盘，初始化完成

    printf("initial data_block success!\n");

    fclose(fp);

    printf("super_bitmap_data_blocks init success!\n");
}
