
#define MAX_HASHMAP 50
#define MOD 31

#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define BLACK "\033[0m"

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

void cp_file_dir(struct hash_map *hash_map, struct file_directory *file_dir)
{
    // 将file_dir的内容复制到hash_map->file_dir中
    struct file_directory *tmp = malloc(sizeof(struct file_directory));
    memcpy(tmp, file_dir, sizeof(struct file_directory));
    hash_map->file_dir = tmp;
}

// 根据目录，返回目录下的文件块号 1 查找成功， -1 查找失败
int find_hash(char *path, long *blk, struct file_directory *file_dir)
{
    printf(GREEN "find hashmap path=%s\n" BLACK, path);
    long hashcode = to_hash(path);
    int index = hashcode % MOD;
    for (; index <= MAX_HASHMAP; index++)
    {
        if (map[index].flag == 0)
        {
            return -1;
        }
        if (map[index].hashcode == hashcode && strcmp(map[index].path, path) == 0)
        {
            break;
        }
        if (index >= MAX_HASHMAP)
        {
            return -1;
        }
    }
    *blk = map[index].nStartBlock;
    // 将map[index]的内容复制到file_dir中
    read_cpy_file_dir(file_dir, map[index].file_dir);
    printf(GREEN "hashmap:%d find dir path=%s\n" BLACK, index, map[index].path);
    return 1;
}

int add_hash(char *path, long nStartBlock, int flag, struct file_directory *file_dir)
{
    printf(GREEN "add hashmap path=%s\n" BLACK, path);
    struct file_directory fing_dir;
    if (find_hash(path, &nStartBlock, &fing_dir) == 1)
    {
        printf(RED "file already exists %s \n" BLACK, path);
        return 1;
    }
    if (map_size >= MAX_HASHMAP)
    {
        printf(RED "hash map is full\n" BLACK);
        return -1;
    }
    long hashcode = to_hash(path);
    int index = hashcode % MOD;
    // 判断是否冲突
    while (map[index].flag != 0)
    {
        index++;
        if (index >= MAX_HASHMAP)
        {
            printf(RED "hash map is full\n" BLACK);
            return -1;
        }
    }
    map[index].hashcode = hashcode;
    map[index].nStartBlock = nStartBlock;
    map[index].flag = flag;
    strcpy(map[index].path, path);
    cp_file_dir(&map[index], file_dir);
    printf(GREEN "hashmap:%d add code:%ld nBlk=%d flag=%d\n" BLACK, index, hashcode, nStartBlock, flag);
    map_size++;
}

void init_hashmap()
{
    for (int i = 0; i < MAX_HASHMAP; i++)
    {
        map[i].flag = 0;
    }
}
