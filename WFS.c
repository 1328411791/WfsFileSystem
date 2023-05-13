#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>

#include "WFS.h"

// 该函数为读取并复制file_directory结构的内容，因为文件系统所有对文件的操作都需要先从文件所在目录
// 读取file_directory信息,然后才能找到文件在磁盘的位置，后者的数据复制给前者
void read_cpy_file_dir(struct file_directory *a, struct file_directory *b)
{
	// 读出属性
	strcpy(a->fname, b->fname);
	strcpy(a->fext, b->fext);
	a->fsize = b->fsize;
	a->nStartBlock = b->nStartBlock;
	a->flag = b->flag;
}

// 根据文件的块号，从磁盘（5M大文件）中读取数据
// 步骤：① 打开文件；② 将FILE指针移动到文件的相应位置；③ 读出数据块
int read_cpy_data_block(long blk_no, struct data_block *data_blk)
{
	FILE *fp = NULL;
	fp = fopen(disk_path, "r+");
	if (fp == NULL)
	{
		printf("错误：read_cpy_data_block：打开文件失败\n\n");
		return -1;
	}
	// 文件打开成功以后，就用blk_no * FS_BLOCK_SIZE作为偏移量
	fseek(fp, blk_no * FS_BLOCK_SIZE, SEEK_SET);
	fread(data_blk, sizeof(struct data_block), 1, fp);
	if (ferror(fp))
	{ // 看读取有没有出错
		printf("错误：read_cpy_data_block：读取文件失败\n\n");
		return -1;
	}
	fclose(fp);
	return 0; // 读取成功则关闭文件，然后返回0
}

// 根据文件的块号，将data_block结构写入到相应的磁盘块里面
// 步骤：① 打开文件；② 将FILE指针移动到文件的相应位置；③写入数据块
int write_data_block(long blk_no, struct data_block *data_blk)
{
	FILE *fp = NULL;
	fp = fopen(disk_path, "r+");
	if (fp == NULL)
	{
		printf("错误：write_data_block：打开文件失败\n\n");
		return -1;
	}
	fseek(fp, blk_no * FS_BLOCK_SIZE, SEEK_SET);
	fwrite(data_blk, sizeof(struct data_block), 1, fp);
	if (ferror(fp))
	{ // 看读取有没有出错
		printf("错误：write_data_block：读取文件失败\n\n");
		return -1;
	}
	fclose(fp);
	return 0; // 写入成功则关闭文件，然后返回0
}

// 路径分割，获得path所指对象的名字、后缀名，返回父目录文件的起始块位置，path是要创建的文件（目录）的目录
// 步骤：① 先检查是不是二级目录（通过找第二个'/'就可以了），如果是，那么在'/'处截断字符串；
//		  如果不是，则跳过这一步
//	   ② 然后要获取这个父目录的file_directory
//	   ③ 再从path中获取文件名和后缀名（获取的时候还要判断文件名和后缀名的合法性）
// 其中par_size是父目录文件的大小
int divide_path(char *name, char *ext, const char *path, long *par_dir_stblk, int flag, int *par_size)
{
	printf("divide_path：函数开始\n\n");
	char *tmp_path, *m, *n;
	tmp_path = strdup(path); // 用来记录最原始的路径
	struct file_directory *attr = malloc(sizeof(struct file_directory));

	m = tmp_path;
	if (!m)
		return -errno; // 路径为空
	m++;			   // 跳过第一个'/'

	n = strchr(m, '/'); // 看是否有二级路径

	// 如果找到二级路径的'/'，并且要创建的是目录，那么不允许，返回-1
	if (n != NULL && flag == 2)
	{
		printf("错误：divide_path:二级路径下不能再创建目录，函数结束返回-EPERM\n\n");
		return -EPERM;
	}
	else if (n != NULL)
	{
		printf("divide_path:要创建的是二级路径下的文件\n\n");
		*n = '\0';
		n++; // 此时n指向要创建的文件名的第一个字母
		m = n;
		if (get_fd_to_attr(tmp_path, attr) == -1)
		{ // 读取该path的父目录，确认这个父目录是存在的
			printf("错误：divide_path：找不到二级路径的父目录，函数结束返回-ENOENT\n\n");
			free(attr);
			return -ENOENT;
		}
	}
	printf("divide_path检查：tmp_path=%s\nm=%s\nn=%s\n\n", tmp_path, m, n);
	// 如果找不到二级路径'/'，说明要创建的对象是： /目录 或 /文件
	// 那么这个路径的父目录为根目录，直接读出来
	if (n == NULL)
	{
		printf("divide_path:要创建的是根目录下的对象\n\n");
		if (get_fd_to_attr("/", attr) == -1)
		{
			printf("错误：divide_path：找不到根目录，函数结束返回-ENOENT\n\n");
			free(attr);
			return -ENOENT;
		}
	}

	// 记录完要创建的对象的名字，如果该对象是文件，还要记录后缀名（有的文件没有后缀名）
	if (flag == 1)
	{
		printf("divide_path:这是文件，有后缀名\n\n");
		n = strchr(m, '.');
		if (n != NULL)
		{
			*n = '\0'; // 截断tmp_path
			n++;	   // 此时n指针指向后缀名的第一位
		}
	}

	// 要创建对象，还要检查：文件名（目录名），后缀名的长度是否超长
	if (flag == 1)
	{ // 如果创建的是文件
		if (strlen(m) > MAX_FILENAME + 1)
		{
			free(attr);
			return -ENAMETOOLONG;
		}
		else if (strlen(m) > MAX_FILENAME)
		{
			if (*(m + MAX_FILENAME) != '~')
			{
				free(attr);
				return -ENAMETOOLONG;
			}
		}
		else if (n != NULL) // 如果有后缀名
		{
			if (strlen(n) > MAX_EXTENSION + 1)
			{
				free(attr);
				return -ENAMETOOLONG;
			}
			else if (strlen(n) > MAX_EXTENSION)
			{
				if (*(n + MAX_EXTENSION) != '~')
				{
					free(attr);
					return -ENAMETOOLONG;
				}
			}
		}
	}
	else if (flag == 2) // 如果创建的是目录
	{
		if (strlen(m) > MAX_DIR_IN_BLOCK)
		{
			free(attr);
			return -ENAMETOOLONG;
		}
	}

	*name = '\0';
	*ext = '\0';
	if (m != NULL)
		strcpy(name, m);
	if (n != NULL)
		strcpy(ext, n);

	printf("已经获取到父目录的file_directory（attr），检查一下：\n\n");
	printf("attr:fname=%s，fext=%s，fsize=%ld，nstartblock=%ld，flag=%d\n\n", attr->fname,
		   attr->fext, attr->fsize, attr->nStartBlock, attr->flag);
	// 把开始块信息赋值给par_dir_stblk
	*par_dir_stblk = attr->nStartBlock;
	// 这里要获取父目录文件的大小
	*par_size = attr->fsize;
	printf("divide_path：检查过要创建对象的文件（目录）名，并没有问题\n\n");
	printf("divide_path：分割后的父目录名：%s\n文件名：%s\n后缀名：%s\npar_dir_stblk=%ld\n\n", tmp_path, name, ext, *par_dir_stblk);
	printf("divide_path：函数结束返回\n\n");
	free(attr);
	free(tmp_path);
	if (*par_dir_stblk == -1)
		return -ENOENT;

	return 0;
}

// 遍历父目录下的一个文件块内的所有文件和目录，如果已存在同名文件或目录，返回-EEXIST
// 注意file_dir指针是调用函数的地方传过来的，直接++就可以了
int exist_check(struct file_directory *file_dir, char *p, char *q, int *offset, int *pos, int size, int flag)
{
	printf("exist_check：现在开始检查该data_blk是否存在重名的对象\n\n");
	while (*offset < size)
	{
		if (flag == 0)
			*pos = *offset;
		// 如果文件名、后缀名（无后缀名）皆匹配
		else if (flag == 1 && file_dir->flag == 1)
		{
			if (strcmp(p, file_dir->fname) == 0)
			{
				if ((*q == '\0' && strlen(file_dir->fext) == 0) || (*q != '\0' && strcmp(q, file_dir->fext) == 0))
				{
					printf("错误：exist_check：存在重名的文件对象，函数结束返回-EEXIST\n\n");
					return -EEXIST;
				}
			}
		}
		// 如果目录名匹配
		else if (flag == 2 && file_dir->flag == 2 && strcmp(p, file_dir->fname) == 0)
		{
			printf("错误：exist_check：存在重名的目录对象，函数结束返回-EEXIST\n\n");
			return -EEXIST;
		}
		file_dir++;
		*offset += sizeof(struct file_directory);
	}
	printf("exist_check：函数结束返回\n\n");
	return 0;
}

// 在bitmap中标记第start_blk块是否被使用（flag=0,1)
int set_blk_use(long start_blk, int flag)
{
	printf("set_blk_use：函数开始");
	if (start_blk == -1)
	{
		printf("错误：set_blk_use：你和我开玩笑？start_blk为-1，函数结束返回\n\n");
		return -1;
	}

	int start = start_blk / 8;		// 因为每个byte有8bits，要找到start_blk在bitmap的第几个byte中，要除以8
	int left = 8 - (start_blk % 8); // 计算在bitmap中的这个byte的第几位表示该块
	unsigned char f = 0x00;
	unsigned char mask = 0x01;
	mask <<= left; // 构造相应位置1的掩码

	FILE *fp = NULL;
	fp = fopen(disk_path, "r+");
	if (fp == NULL)
		return -1;

	fseek(fp, FS_BLOCK_SIZE + start, SEEK_SET); // super_block占了FS_BLOCK_SIZE个bytes
	unsigned char *tmp = malloc(sizeof(unsigned char));
	fread(tmp, sizeof(unsigned char), 1, fp); // 把相应的byte读出来
	f = *tmp;

	// 将该位置1，其他位不动
	if (flag)
		f |= mask;
	// 将该位置0，其他位不动
	else
		f &= ~mask;

	*tmp = f;
	fseek(fp, FS_BLOCK_SIZE + start, SEEK_SET);
	fwrite(tmp, sizeof(unsigned char), 1, fp);
	fclose(fp);
	printf("set_blk_use：bitmap状态设置成功,函数结束返回\n\n");
	free(tmp);
	return 0;
}

// 判断该path中是否含有目录和文件，如果为空则返回1，不为空则返回0
int path_is_emp(const char *path)
{
	printf("path_is_emp：函数开始\n\n");
	struct data_block *data_blk = malloc(sizeof(struct data_block));
	struct file_directory *attr = malloc(sizeof(struct file_directory));
	// 读取属性到attr里
	if (get_fd_to_attr(path, attr) == -1)
	{
		printf("错误：path_is_emp：get_fd_to_attr失败，path所指对象的file_directory不存在，函数结束返回\n\n");
		free(data_blk);
		free(attr);
		return 0;
	}
	long start_blk;
	start_blk = attr->nStartBlock;
	// 传入路径为文件
	if (attr->flag == 1)
	{
		printf("错误：path_is_emp：传入的path是文件的path，不能进行判断，函数结束返回\n\n");
		free(data_blk);
		free(attr);
		return 0;
	}
	// 读出块信息
	if (read_cpy_data_block(start_blk, data_blk) == -1)
	{
		printf("错误：path_is_emp：read_cpy_data_block读取块信息失败,函数结束返回\n\n");
		free(data_blk);
		free(attr);
		return 0;
	}

	struct file_directory *file_dir = (struct file_directory *)data_blk->data;
	int pos = 0;
	// 遍历目录下文件，假如存在使用中的文件或目录，非空
	while (pos < data_blk->size)
	{
		if (file_dir->flag != 0)
		{
			printf("path_is_emp：判断完毕，检测到该目录下有文件，目录不为空，函数结束返回\n\n");
			free(data_blk);
			free(attr);
			return 0;
		}
		file_dir++;
		pos += sizeof(struct file_directory);
	}
	printf("path_is_emp：判断完毕，函数结束返回\n\n");
	free(data_blk);
	free(attr);
	return 1;
}

// 将file_directory的数据赋值给path相应文件或目录的file_directory	(!!!!!)
int setattr(const char *path, struct file_directory *attr, int flag)
{
	printf("setattr：函数开始\n\n");
	int res, par_size;
	struct data_block *data_blk = malloc(sizeof(struct data_block));
	char *m = malloc(15 * sizeof(char)), *n = malloc(15 * sizeof(char));
	long start_blk;

	// path所指文件的file_directory存储在该文件所在的父目录的文件块里面，所以要进行路径分割
	if ((res = divide_path(m, n, path, &start_blk, flag, &par_size)))
	{
		printf("错误：setattr：divide_path失败，函数返回\n\n");
		free(m);
		free(n);
		return res;
	}

	// 下面就是寻找文件块的位置了，当然如果目录太大有可能要继续读下一个块
	// 获取到父目录文件的起始块位置，我们就读出这个起始块
	if (read_cpy_data_block(start_blk, data_blk) == -1)
	{
		printf("错误：setattr：寻找文件块位置do_while循环内：读取文件块失败，函数结束返回\n\n");
		res = -1;
		free(data_blk);
		return res;
	}
	// start_blk=data_blk->nNextBlock;
	struct file_directory *file_dir = (struct file_directory *)data_blk->data;
	int offset = 0;
	while (offset < data_blk->size)
	{
		// 循环遍历块内容
		// 找到该文件
		if (file_dir->flag != 0 && strcmp(m, file_dir->fname) == 0 && (*n == '\0' || strcmp(n, file_dir->fext) == 0))
		{
			printf("setattr：找到相应路径的file_directory，可以进行回写了\n\n");
			// 设置为attr指定的属性
			read_cpy_file_dir(file_dir, attr);
			res = 0;
			free(m);
			free(n);
			// 将修改后的目录信息写回磁盘文件
			if (write_data_block(start_blk, data_blk) == -1)
			{
				printf("错误：setattr：while循环内回写数据块失败\n\n");
				res = -1;
			}
			free(data_blk);
			return res;
		}
		// 读下一个文件
		file_dir++;
		offset += sizeof(struct file_directory);
	}
	// 如果在这一个块找不到该文件，我们继续寻找该目录的下一个块，不过前提是这个目录文件有下一个块
	// }while(data_blk->nNextBlock!=-1 && data_blk->nNextBlock!=0);
	printf("setattr：赋值成功，函数结束返回\n\n");
	// 找遍整个目录都没找到该文件就直接返回-1
	return -1;
}

// 从next_blk起清空data_blk后续块
void ClearBlocks(long next_blk, struct data_block *data_blk)
{
	printf("ClearBlocks：函数开始\n\n");
	while (next_blk != -1)
	{
		set_blk_use(next_blk, 0);				 // 在bitmap中设置相应位没有被使用
		read_cpy_data_block(next_blk, data_blk); // 为了读取这个块的后续块，先读出这个块
		next_blk = data_blk->nNextBlock;
	}
	printf("ClearBlocks：函数结束返回\n\n");
}

// 找到num个连续空闲块，返回空闲块区的起始块号start_blk，返回找到的连续空闲块个数（否则返回找到的最大值）
// 这里我采用的是首次适应法（就是找到第一片大小大于num的连续区域，并将这片区域的起始块号返回）
int get_empty_blk(int num, long *start_blk)
{
	printf("get_empty_blk：函数开始\n\n");
	// 从头开始找，跳过super_block、bitmap和根目录块，现在偏移量为1282（从第 0 block 偏移1282 blocks，指向编号为1282的块（其实是第1283块））
	*start_blk = 1 + BITMAP_BLOCK + 1;
	int tmp = 0;
	// 打开文件
	FILE *fp = NULL;
	fp = fopen(disk_path, "r+");
	if (fp == NULL)
		return 0;
	int start, left;
	unsigned char mask, f; // 8bits
	unsigned char *flag;

	// max和max_start是用来记录可以找到的最大连续块的位置
	int max = 0;
	long max_start = -1;

	// 要找到一片连续的区域，我们先检查bitmap
	// 要确保start_blk的合法性(一共10240块，则块编号最多去到10239)
	// 每一次找不到连续的一片区域存放数据，就会不断循环，直到找到为止
	printf("get_empty_blk：现在开始寻找一片连续的区域\n\n");
	while (*start_blk < TOTAL_BLOCK_NUM)
	{
		start = *start_blk / 8;		 // start_blk每个循环结束都会更新到新的磁盘块空闲的位置
		left = 8 - (*start_blk % 8); // 1byte里面的第几bit是空的
		mask = 1;
		mask <<= left;

		fseek(fp, FS_BLOCK_SIZE + start, SEEK_SET); // 跳过super block，跳到bitmap中start_blk指定位置（以byte为单位）
		flag = malloc(sizeof(unsigned char));		// 8bits
		fread(flag, sizeof(unsigned char), 1, fp);	// 读出8个bit
		f = *flag;

		// 下面开始检查这一片连续存储空间是否满足num块大小的要求
		for (tmp = 0; tmp < num; tmp++)
		{
			// mask为1的位，f中为1，该位被占用，说明这一片连续空间不够大，跳出
			if ((f & mask) == mask)
				break;
			// mask最低位为1，说明这个byte已检查到最低位，8个bit已读完
			if ((mask & 0x01) == 0x01)
			{ // 读下8个bit
				fread(flag, sizeof(unsigned char), 1, fp);
				f = *flag;
				mask = 0x80; // 指向8个bit的最高位
			}
			// 位为1,右移1位，检查下一位是否可用
			else
				mask >>= 1;
		}
		// 跳出上面的循环有两种可能，一种是tmp==num，说明已经找到了连续空间
		// 另外一种是tmp<num说明这片连续空间不够大，要更新start_blk的位置
		// tmp为找到的可用连续块数目
		if (tmp > max)
		{
			// 记录这个连续块的起始位
			max_start = *start_blk;
			max = tmp;
		}
		// 如果后来找到的连续块数目少于之前找到的，不做替换
		// 找到了num个连续的空白块
		if (tmp == num)
			break;

		// 只找到了tmp个可用block，小于num，要重新找，更新起始块号
		*start_blk = (tmp + 1) + *start_blk;
		tmp = 0;
		// 找不到空闲块
	}
	*start_blk = max_start;
	fclose(fp);
	int j = max_start;
	int i;
	// 将这片连续空间在bitmap中标记为1
	for (i = 0; i < max; i++)
	{
		if (set_blk_use(j++, 1) == -1)
		{
			printf("错误：get_empty_blk：set_blk_use失败，函数结束返回\n\n");
			free(flag);
			return -1;
		}
	}
	printf("get_empty_blk：申请空间成功，函数结束返回\n\n");
	free(flag);
	return max;
}

/***************************************************************************************************************************/
// 三个功能函数:getattr,create_file_dir,remove_file_dir

// 根据文件的路径，到相应的目录寻找该文件的file_directory，并赋值给attr
int get_fd_to_attr(const char *path, struct file_directory *attr)
{
	// 先要读取超级块，获取磁盘根目录块的位置
	struct data_block *data_blk;
	data_blk = malloc(sizeof(struct data_block));

	// 把超级块读出来
	if (read_cpy_data_block(0, data_blk) == -1)
	{
		printf("get_fd_to_attr：读取超级块失败，函数结束返回\n\n");
		free(data_blk);
		return -1;
	}
	struct super_block *sb_blk;
	sb_blk = (struct super_block *)data_blk;
	long start_blk;
	start_blk = sb_blk->first_blk;

	char *tmp_path, *m; // tmp_path用来临时记录路径，然后m,n两个指针是用来定位文件名和
	tmp_path = strdup(path);
	m = tmp_path;

	// 如果路径为空，则出错返回1
	if (!tmp_path)
	{
		printf("错误：get_fd_to_attr：路径为空，函数结束返回\n\n");
		free(sb_blk);
		return -1;
	}

	// 如果路径为根目录路径(注意这里的根目录是指/home/linyueq/homework/diskimg/???，/???之前的路径是被忽略的)
	if (strcmp(tmp_path, "/") == 0)
	{
		attr->flag = 2; // 2代表路径
		attr->nStartBlock = start_blk;
		free(sb_blk);
		printf("get_fd_to_attr：这是一个根目录，直接构造file_directory并返回0，函数结束返回\n\n");
		return 0;
	}
	// 检查完字符串既不为空也不为根目录,则要处理一下路径，而路径分为2种（我们只做2级文件系统）
	// 一种是文件直接放在根目录下，则路径形为：/a.txt，另一种是在一个目录之下，则路径形为:/hehe/a.txt
	// 我们路径处理的目标是让tmp_path记录diskimg下的目录名（如果path含目录的话），m记录文件名，n记录后缀名

	// 先往后移一位，跳过第一个'/'，然后检查一下这个路径是不是有两个'/'，如果有，说明路径是/hehe/a.txt形
	m++;

	// struct data_block *data_blk=malloc(sizeof(struct data_block));

	// 读取根目录文件的信息，失败的话返回-1
	if (read_cpy_data_block(start_blk, data_blk) == -1)
	{
		free(data_blk);
		printf("错误：get_fd_to_attr：读取根目录文件失败，函数结束返回\n\n");
		return -1;
	}

	// 强制类型转换，读取根目录中文件的信息，根目录块中装的都是struct file_directory
	struct file_directory *file_dir = (struct file_directory *)data_blk->data;
	int i = 0;
	while (i < 512 / sizeof(struct file_directory))
	{
		char tempfilename[20];
		strcpy(tempfilename, file_dir->fname);
		if (file_dir->fext[0] != 0)
		{
			strcat(tempfilename, ".");
			strcat(tempfilename, file_dir->fext);
		}
		if (strcmp(m, tempfilename) == 0)
		{					// found speafied file
			attr->flag = 1; // for file
			strcpy(attr->fname, file_dir->fname);
			strcpy(attr->fext, file_dir->fext);

			attr->atime = file_dir->atime;
			attr->mtime = file_dir->mtime;
			attr->mode = file_dir->mode;
			attr->uid = file_dir->uid;

			attr->fsize = file_dir->fsize;
			attr->nStartBlock = file_dir->nStartBlock;
			free(data_blk);
			return (0);
		}
		file_dir++;
		i++;
	}

	// 循环结束都还没找到，则返回-1
	printf("get_fd_to_attr：在父目录下没有找到该path的file_directory\n\n");
	free(data_blk);
	return -1;
}

// 创建path所指的文件或目录的file_directory，并为该文件（目录）申请空闲块，创建成功返回0，创建失败返回-1
// mkdir和mknod这两种操作都要用到
int create_file_dir(const char *path, int flag)
{
	printf("调用了create_file_dir，创建的类型是：%d，创建的路径是：%s\n\n", flag, path);
	int res, par_size;
	long par_dir_blk;													 // 存放父目录文件的起始块，经过exist_check后会指向父目录文件的最后一块（因为创建文件肯定实在最后一块增加file_directory的）
	char *m = malloc(15 * sizeof(char)), *n = malloc(15 * sizeof(char)); // 用于存放文件名和扩展名
	// 拆分路径，找到父级目录起始块
	if ((res = divide_path(m, n, path, &par_dir_blk, flag, &par_size)))
	{
		free(m);
		free(n);
		printf("错误：create_file_dir:divide_path时出错\n\n");
		return res = -1;
	}

	struct data_block *data_blk = malloc(sizeof(struct data_block));
	struct file_directory *file_dir = malloc(sizeof(struct file_directory));
	int offset = 0;
	int pos;

	// 从目录块中读取目录信息到data_blk
	if (read_cpy_data_block(par_dir_blk, data_blk) == -1)
	{
		free(data_blk);
		free(file_dir);
		free(m);
		free(n);
		printf("错误：create_file_dir:从目录块中读取目录信息到data_blk时出错\n\n");
		return -ENOENT;
	}

	file_dir = (struct file_directory *)data_blk->data;
	offset = 0;
	pos = data_blk->size;

	// 遍历父目录下的所有文件和目录，如果已存在同名文件或目录，返回-1
	// 一个文件一定是连续存放的
	if ((res = exist_check(file_dir, m, n, &offset, &pos, data_blk->size, flag)))
	{
		free(data_blk);
		free(file_dir);
		free(m);
		free(n);
		printf("错误：create_file_dir:exist_check检测到该文件（目录）已存在，或出错\n\n");
		return res = -1;
	}

	printf("create_file_dir:没有重名的文件或目录，开始创建文件\n\n");

	// 经过exist_check函数，offset应该指向匹配到的文件的file_dir的位置，如果没找到该文件，则offset=data_blk->size
	file_dir += offset / sizeof(struct file_directory);

	long *tmp = malloc(sizeof(long));
	// 假如exist_check函数里面并没有改变pos的值，那么说明flag!=0
	if (pos == data_blk->size)
	{
		if (data_blk->size > MAX_DATA_IN_BLOCK)
		{
			// 当前块放不下目录内容,you should add some code here
		}
		else
		{ // 块容量足够，直接加size
			data_blk->size += sizeof(struct file_directory);
		}
	}
	else
	{ // flag=0
		printf("create_file_dir:flag为0\n\n");
		offset = 0;
		file_dir = (struct file_directory *)data_blk->data;

		while (offset < pos)
			file_dir++;
	}
	// 给新建的file_directory赋值
	strcpy(file_dir->fname, m);
	if (flag == 1 && *n != '\0')
		strcpy(file_dir->fext, n);
	file_dir->fsize = 0;
	file_dir->flag = flag;

	time_t now_time;
	time(&now_time);

	file_dir->atime = now_time;
	file_dir->mtime = now_time;
	file_dir->mode = S_IFREG | 0777;
	file_dir->uid = getuid();

	// 为新建的文件申请一个空闲块
	if ((res = get_empty_blk(1, tmp)) == 1)
		file_dir->nStartBlock = *tmp;
	else
	{
		printf("错误：create_file_dir：为新建文件申请数据块时失败，函数结束返回\n\n");
		free(data_blk);
		free(file_dir);
		free(m);
		free(n);
		return -errno;
	}
	printf("tmp=%ld\n\n", *tmp);
	free(tmp);
	// 将要创建的文件或目录信息写入上层目录中
	write_data_block(par_dir_blk, data_blk);
	data_blk->size = 0;
	data_blk->nNextBlock = -1;
	strcpy(data_blk->data, "\0");

	// 文件起始块内容为空
	write_data_block(file_dir->nStartBlock, data_blk);

	printf("m=%s,n=%s\n\n", m, n);

	free(data_blk);
	free(m);
	free(n);
	printf("create_file_dir：创建文件成功，函数结束返回\n\n");
	return 0;
}

// 删除path所指的文件或目录的file_directory和文件的数据块，成功返回0，失败返回-1
int remove_file_dir(const char *path, int flag)
{
	printf("remove_file_dir：函数开始\n\n");
	struct file_directory *attr = malloc(sizeof(struct file_directory));
	// 读取文件属性
	if (get_fd_to_attr(path, attr) == -1)
	{
		free(attr);
		printf("错误：remove_file_dir：get_fd_to_attr失败，函数结束返回\n\n");
		return -ENOENT;
	}
	printf("检查attr:fname=%s，fext=%s，fsize=%ld，nstartblock=%ld，flag=%d\n\n", attr->fname,
		   attr->fext, attr->fsize, attr->nStartBlock, attr->flag);
	// flag与指定的不一致，则返回相应错误信息
	if (flag == 1 && attr->flag == 2)
	{
		free(attr);
		printf("错误：remove_file_dir：要删除的对象flag不一致，删除失败，函数结束返回\n\n");
		return -EISDIR;
	}
	else if (flag == 2 && attr->flag == 1)
	{
		free(attr);
		printf("错误：remove_file_dir：要删除的对象flag不一致，删除失败，函数结束返回\n\n");
		return -ENOTDIR;
	}
	// 清空该文件从起始块开始的后续块
	struct data_block *data_blk = malloc(sizeof(struct data_block));
	if (flag == 1)
	{
		long next_blk = attr->nStartBlock;
		ClearBlocks(next_blk, data_blk);
	}
	else if (!path_is_emp(path)) // 只能删除空的目录，目录非空返回错误信息
	{
		free(data_blk);
		free(attr);
		printf("remove_file_dir：要删除的目录不为空，删除失败，函数结束返回\n\n");
		return -ENOTEMPTY;
	}

	attr->flag = 0; // 被删除的文件或目录的file_directory设置为未使用
	if (setattr(path, attr, flag) == -1)
	{
		printf("remove_file_dir：setattr失败，函数结束返回\n\n");
		free(data_blk);
		free(attr);
		return -1;
	}
	printf("remove_file_dir：删除成功，函数结束返回\n\n");
	free(data_blk);
	free(attr);
	return 0;
}

/***************************************************************************************************************************/

// 文件系统初始化函数，载入文件系统的时候系统需要知道这个文件系统的大小（以块为单位）
static void *WFS_init(struct fuse_conn_info *conn)
{
	(void)conn;

	FILE *fp = NULL;
	fp = fopen(disk_path, "r+");
	if (fp == NULL)
	{
		fprintf(stderr, "错误：打开文件失败，文件不存在，函数结束返回\n");
		return 0;
	}
	// 先读超级块
	struct super_block *super_block_record = malloc(sizeof(struct super_block));
	fread(super_block_record, sizeof(struct super_block), 1, fp);
	// 用超级块中的fs_size初始化全局变量
	TOTAL_BLOCK_NUM = super_block_record->fs_size;
	fclose(fp);
	return 0;
}

/*struct stat {
		mode_t     st_mode;       //文件对应的模式，文件，目录等
		ino_t      st_ino;       //inode节点号
		dev_t      st_dev;        //设备号码
		dev_t      st_rdev;       //特殊设备号码
		nlink_t    st_nlink;      //文件的连接数
		uid_t      st_uid;        //文件所有者
		gid_t      st_gid;        //文件所有者对应的组
		off_t      st_size;       //普通文件，对应的文件字节数
		time_t     st_atime;      //文件最后被访问的时间
		time_t     st_mtime;      //文件内容最后被修改的时间
		time_t     st_ctime;      //文件状态改变时间
		blksize_t st_blksize;    //文件内容对应的块大小
		blkcnt_t   st_blocks;     //文件内容对应的块数量
	  };*/

// 该函数用于读取文件属性（通过对象的路径获取文件的属性，并赋值给stbuf）
static int WFS_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
	int res = 0;
	struct file_directory *attr = malloc(sizeof(struct file_directory));

	// 非根目录
	if (get_fd_to_attr(path, attr) == -1)
	{
		free(attr);
		printf("WFS_getattr：get_fd_to_attr时发生错误，函数结束返回\n\n");
		return -ENOENT;
	}

	memset(stbuf, 0, sizeof(struct stat)); // 将stat结构中成员的值全部置0
	if (attr->flag == 2)
	{ // 从path判断这个文件是		一个目录	还是	一般文件
		printf("WFS_getattr：这个file_directory是一个目录\n\n");
		stbuf->st_mode = S_IFDIR | 0666; // 设置成目录,S_IFDIR和0666（8进制的文件权限掩码），这里进行或运算
										 // stbuf->st_nlink = 2;//st_nlink是连到该文件的硬连接数目,即一个文件的一个或多个文件名。说白点，所谓链接无非是把文件名和计算机文件系统使用的节点号链接起来。因此我们可以用多个文件名与同一个文件进行链接，这些文件名可以在同一目录或不同目录。
	}
	else if (attr->flag == 1)
	{
		printf("WFS_getattr：这个file_directory是一个文件\n\n");
		// 该文件是	一般文件
		stbuf->st_mode = S_IFREG | attr->mode;
		stbuf->st_size = attr->fsize;
		stbuf->st_atime = attr->atime;
		stbuf->st_mtime = attr->mtime;
		stbuf->st_uid = attr->uid;
		// stbuf->st_nlink = 1;
	}
	else
	{
		printf("WFS_getattr：这个文件（目录）不存在，函数结束返回\n\n");
		;
		res = -ENOENT;
	} // 文件不存在

	printf("WFS_getattr：getattr成功，函数结束返回\n\n");
	free(attr);
	return res;
}

// 创建文件
static int WFS_mknod(const char *path, mode_t mode, dev_t dev)
{
	return create_file_dir(path, 1);
}

// 删除文件
static int WFS_unlink(const char *path)
{
	return remove_file_dir(path, 1);
}

// 打开文件时的操作
static int WFS_open(const char *path, struct fuse_file_info *fi)
{

	return 0;
}

// 读取文件时的操作
// 根据路径path找到文件起始位置，再偏移offset长度开始读取size大小的数据到buf中，返回文件大小
// 其中，buf用来存储从path读出来的文件信息，size为文件大小，offset为读取时候的偏移量，fi为fuse的文件信息
// 步骤：① 先读取该path所指文件的file_directory；② 然后根据nStartBlock读出文件内容
static int WFS_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("WFS_read：函数开始\n\n");
	struct file_directory *attr = malloc(sizeof(struct file_directory));

	// 读取该path所指对象的file_directory
	if (get_fd_to_attr(path, attr) == -1)
	{
		free(attr);
		printf("错误：WFS_read：get_fd_to_attr失败，函数结束返回\n\n");
		return -ENOENT;
	}
	// 如果读取到的对象是目录，那么返回错误（只有文件会用到read这个函数）
	if (attr->flag == 2)
	{
		free(attr);
		printf("错误：WFS_read：对象为目录不是文件，读取失败，函数结束返回\n\n");
		return -EISDIR;
	}

	struct data_block *data_blk = malloc(sizeof(struct data_block));
	// 根据文件信息读取文件内容
	if (read_cpy_data_block(attr->nStartBlock, data_blk) == -1)
	{
		free(attr);
		free(data_blk);
		printf("错误：WFS_read：读取文件起始块内容失败，函数结束返回\n\n");
		return -1;
	}

	// 查找文件数据块,读取并读入buf中
	// 要保证 读取的位置 和 加上size后的位置 在文件范围内
	if (offset < attr->fsize)
	{
		if (offset + size > attr->fsize)
			size = attr->fsize - offset;
	}
	else
		size = 0;

	char *pt = data_blk->data; // 先读出offset所在块的数据
	pt += offset;			   // 将数据指针移动到offset所指的位置

	strncpy(buf, pt, size);

	printf("WFS_read：文件读取成功，函数结束返回\n\n");
	free(attr);
	free(data_blk);
	return size;
}

// 修改文件,将buf里大小为size的内容，写入path指定的起始块后的第offset
// 步骤：① 找到path所指对象的file_directory；② 根据nStartBlock和offset将内容写入相应位置；
static int WFS_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("WFS_write：函数开始\n\n");
	struct file_directory *attr = malloc(sizeof(struct file_directory));
	// 打开path所指的对象，将其file_directory读到attr中
	get_fd_to_attr(path, attr);

	// 然后检查要写入数据的位置是否越界
	if (offset > attr->fsize)
	{
		free(attr);
		printf("WFS_write：offset越界，函数结束返回\n\n");
		return -EFBIG;
	}

	long start_blk = attr->nStartBlock;
	if (start_blk == -1)
	{
		printf("WFS_write：该文件为空（无起始块），函数结束返回\n\n");
		free(attr);
		return -errno;
	}

	int p_offset = offset; // p_offset用来记录修改前最后一个文件块的位置
	struct data_block *data_blk = malloc(sizeof(struct data_block));

	read_cpy_data_block(start_blk, data_blk);
	char *pt = data_blk->data;
	// 找到offset所在块中offset位置
	pt += offset;

	int towrite = size;
	int writen = 0;

	strncpy(pt, buf, towrite); // 写入长度为towrite的内容
	buf += towrite;			   // 移到字符串待写处
	data_blk->size += towrite; // 该数据块的size增加已写数据量towrite
	writen += towrite;		   // buf中已写的数据量

	size = writen;
	write_data_block(start_blk, data_blk);
	// 修改被写文件file_directory的文件大小信息为:写入起始位置+写入内容大小
	attr->fsize = p_offset + size;
	if (setattr(path, attr, 1) == -1)
		size = -errno;

	printf("WFS_write：文件写入成功，函数结束返回\n\n");
	free(attr);
	free(data_blk); // free(next_blk);
	return size;
}
/*
//关闭文件
static int WFS_release (const char *, struct fuse_file_info *)
{
	return 0;
}*/

// 创建目录
static int WFS_mkdir(const char *path, mode_t mode)
{
	return create_file_dir(path, 2);
}

// 删除目录
static int WFS_rmdir(const char *path)
{
	return remove_file_dir(path, 2);
}

// 进入目录
static int WFS_access(const char *path, int flag)
{
	/*int res;
	struct file_directory* attr=malloc(sizeof(struct file_directory));
	if(get_fd_to_attr(path,attr)==-1)
	{
		free(attr);return -ENOENT;
	}
	if(attr->flag==1) {free(attr);return -ENOTDIR;}*/
	return 0;
}

// 终端中ls -l读取目录的操作会使用到这个函数，因为fuse创建出来的文件系统是在用户空间上的
// 这个函数用来读取目录，并将目录里面的文件名加入到buf里面
// 步骤：① 读取path所对应目录的file_directory，找到该目录文件的nStartBlock；
//	   ② 读取nStartBlock里面的所有file_directory，并用filler把 (文件+后缀)/目录 名加入到buf里面
static int WFS_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) //,enum use_readdir_flags flags)
{
	struct data_block *data_blk;
	struct file_directory *attr;
	data_blk = malloc(sizeof(struct data_block));
	attr = malloc(sizeof(struct file_directory));

	if (get_fd_to_attr(path, attr) == -1)
	{ // 打开path指定的文件，将文件属性读到attr中
		free(attr);
		free(data_blk);
		return -ENOENT;
	}

	long start_blk = attr->nStartBlock;
	// 如果该路径所指对象为文件，则返回错误信息
	if (attr->flag == 1)
	{
		free(attr);
		free(data_blk);
		return -ENOENT;
	}

	// 如果path所指对象是路径，那么读出该目录的数据块内容
	if (read_cpy_data_block(start_blk, data_blk))
	{
		free(attr);
		free(data_blk);
		return -ENOENT;
	}

	// 无论是什么目录，先用filler函数添加 . 和 ..
	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	// 按顺序查找,并向buf添加目录内的文件和目录名
	struct file_directory *file_dir = (struct file_directory *)data_blk->data;
	int pos = 0;
	char name[MAX_FILENAME + MAX_EXTENSION + 2]; // 2是因为文件名和扩展名都有nul字符
	while (pos < data_blk->size)
	{
		strcpy(name, file_dir->fname);
		if (strlen(file_dir->fext) != 0)
		{
			strcat(name, ".");
			strcat(name, file_dir->fext);
		}
		if (file_dir->flag != 0 && name[strlen(name) - 1] != '~' && filler(buf, name, NULL, 0, 0)) // 将文件名添加到buf里面
			break;
		file_dir++;
		pos += sizeof(struct file_directory);
	}
	free(attr);
	free(data_blk);
	return 0;
}

// 所有文件的操作都要放到这里，fuse会帮我们在相应的linux操作中执行这些我们写好的函数
static struct fuse_operations WFS_oper = {
	.init = WFS_init,		// 初始化
	.getattr = WFS_getattr, // 获取文件属性（包括目录的）

	.mknod = WFS_mknod,	  // 创建文件
	.unlink = WFS_unlink, // 删除文件
	.open = WFS_open,	  // 无论是read还是write文件，都要用到打开文件
	.read = WFS_read,	  // 读取文件内容
	.write = WFS_write,	  // 修改文件内容
	//.release    = WFS_release,//和open相对，关闭文件

	.mkdir = WFS_mkdir,		// 创建目录
	.rmdir = WFS_rmdir,		// 删除目录
	.access = WFS_access,	// 进入目录
	.readdir = WFS_readdir, // 读取目录
};

int main(int argc, char *argv[])
{
	umask(0);
	return fuse_main(argc, argv, &WFS_oper, NULL);
}

/*
  通过上述的分析可以知道，使用FUSE必须要自己实现对文件或目录的操作， 系统调用也会最终调用到用户自己实现的函数。
  用户实现的函数需要在结构体fuse_operations中注册。而在main()函数中，用户只需要调用fuse_main()函数就可以了，剩下的复杂工作可以交给FUSE。
*/