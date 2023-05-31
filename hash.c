
#define MAX_HASHMAP 50
#define MOD 31

#define RED "\033[1;31m"
#define GREEN "\033[1;32m"

struct hash_map map[MAX_HASHMAP];
int map_size = 0;

long to_hash(char *str)
{
    int i = 1;
    long ret = 0;
    while (*str != '\0')
    {
        ret += *str * i;
        i++;
        str++;
    }
    return ret;
}

// 根据目录，返回目录下的文件块号 1 查找成功， -1 查找失败
int find_hash(char *path, long *blk, struct file_directory *file_dir)
{
    long hashcode = to_hash(path);
    int index = hashcode % MOD;
    for (; index <= MAX_HASHMAP; index++)
    {
        if (map[index].hashcode == hashcode && strcmp(map[index].path, path) == 0)
        {
            break;
        }
        if (index >= MAX_HASHMAP)
        {
            return -1;
        }
        if (map[index].flag == 0)
        {
            return -1;
        }
    }
    *blk = map[index].nStartBlock;
    file_dir = map[index].file_dir;
    return 1;
}

int add_hash(char *path, long nStartBlock, int flag, struct file_directory *file_dir)
{
    if (find_hash(path, &nStartBlock, file_dir) == 1)
    {
        printf(RED "file already exists %s \n", path);
        return 1;
    }
    if (map_size >= MAX_HASHMAP)
    {
        printf(RED "hash map is full\n");
        return;
    }
    long hashcode = to_hash(path);
    int index = hashcode % MOD;
    // 判断是否冲突
    while (map[index].flag != 0)
    {
        index++;
        if (index >= MAX_HASHMAP)
        {
            printf(RED "hash map is full\n");
            return -1;
        }
    }
    map[index].hashcode = hashcode;
    map[index].nStartBlock = nStartBlock;
    map[index].flag = flag;
    strcpy(map[index].path, path);
    map[index].file_dir = file_dir;
    map_size++;
}

void init_hashmap()
{
    for (int i = 0; i < MAX_HASHMAP; i++)
    {
        map[i].flag = 0;
    }
}
