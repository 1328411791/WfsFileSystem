int index_compare(const void *a, const void *b, void *udata)
{
    const struct file_index *ua = a;
    const struct file_index *ub = b;
    return strcmp(ua->path, ub->path);
}

bool index_iter(const void *item, void *udata)
{
    const struct file_index *index = item;
    printf("%s (StartBlock=%ld)\n", index->path, index->nStartBlock);
    return true;
}

uint64_t index_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const struct file_index *index = item;
    return hashmap_sip(index->path, strlen(index->path), seed0, seed1);
}