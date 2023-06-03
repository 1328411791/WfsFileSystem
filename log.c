#pragma one
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
@author: lhy
@description: 简易日记库
*/
// 定义颜色
#define RED "\033[1;31m"
#define GREEN "\033[1;35m"
#define BLACK "\033[0m"

// Log 函数
void _Log(char *format, char *FILE, int LINE, ...)
{
    va_list args;
    va_start(args, LINE);
    printf(GREEN);
    // 输出位于代码的行数
    printf("[%s:%d]: ", FILE, LINE);
    vprintf(format, args);
    printf(BLACK);
    va_end(args);
}

// Error函数
void _Error(char *format, char *FILE, int LINE, ...)
{
    va_list args;
    va_start(args, LINE);
    printf(RED);
    // 输出调用函数位于代码的行数
    printf("[%s:%d]: ", FILE, LINE);
    vprintf(format, args);
    printf(BLACK);
    va_end(args);
}

#define Log(format, ...) _Log(format, __FILE__, __LINE__, ##__VA_ARGS__)

#define Error(format, ...) _Error(format, __FILE__, __LINE__, ##__VA_ARGS__)