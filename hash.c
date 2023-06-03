int index_compare(const void *a, const void *b, void *udata)
{
    const struct file_index *ua = a;
    const struct file_index *ub = b;
    return strcmp(ua->path, ub->path);
}

bool index_iter(const void *item, void *udata)
{
    const struct file_index *index = item;
    printf("%s (file_dir address=%lx)\n", index->path, index->file_dir);
    return true;
}

uint64_t index_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const struct file_index *index = item;
    return hashmap_sip(index->path, strlen(index->path), seed0, seed1);
}

struct file_index *set_add_index(char *path, struct file_directory *file_dir)
{
    struct file_directory *tmp = (struct file_directory *)malloc(sizeof(struct file_directory));
    read_cpy_file_dir(tmp, file_dir);
    struct file_index *i_file = (struct file_index *)malloc(sizeof(struct file_index));
    strcpy(i_file->path, path);
    i_file->file_dir = tmp;
    return i_file;
}

struct file_index *set_find_index(char *path)
{
    struct file_index *i_file = (struct file_index *)malloc(sizeof(struct file_index));
    strcpy(i_file->path, path);
    return i_file;
}