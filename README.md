# WfsFileSystem

使用 FUSE 的文件系统

# 实现要求
WFS目前已经实现了最简单最基础的功能，但仍有许多不足之处有待于你的改进：

|  序号   | 表头  | 完成情况 |
|  ----  | ----  |----|
| 1  | 文件目录里缺乏文件权限、文件主、时间日期等信息，请予以补全。 | 完成 |
| 2  | 文件的大小目前最大只有一个磁盘块大小，请实现文件的大小可以是多块。 | |
| 3  | WFS目前只支持一级目录，即所以有的文件均在根目录下，请在WFS下实现多级目录。 | |
| 4  | 按照目前WFS目前的设计，文件的访问是一种顺序访问的方式，请改进WFS的数据访问方式为随机访问。 | |


# install info
1. install libfuse
2. cp diskimg to /tmp dir.then make this code.next use WFS to mount dir (like tt)  

# some script

`./WFS <dir>` set mount disk into dir

`fusermount -u <dir>` unmount disk

# 做了
1. √


