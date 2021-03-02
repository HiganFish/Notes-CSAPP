深入理解计算机系统 样例程序 C++

# code
## TinyWeb

从C改为了C++ 替换了部分char*为std::string

也借此尝试了下C++20的format库

没有使用csapp.h  rio_t自己用一个Buffer类简单封装了下

# 第一章 计算机系统漫游

![](http://lsmg-img.oss-cn-beijing.aliyuncs.com/csapp/1-3%E7%BC%96%E8%AF%91%E7%B3%BB%E7%BB%9F.png)

预处理->编译->汇编->链接

```c++
// hello.cpp
#include <cstdio>
#define PI 3.14

int main()
{
    // 1111
    //
    printf("%f\n", PI);
    printf("hello, world!\n");
    return 0;
}
```

预处理
宏替换和注释消失 但是空行还在
`g++ -E hello.cpp -o hello.i`
`tail -9 hello.i`
```c++
# 5 "hello.cpp"
int main()
{


    printf("%f\n", 3.14);
    printf("hello, world!\n");
    return 0;
}
```

编译 生成汇编
`g++ -S hello.i`
```s

        .file   "hello.cpp"
        .text
        .section        .rodata
.LC1:
        .string "%f\n"
.LC2:
        .string "hello, world!"
        .text
        .globl  main
        .type   main, @function
main:
.LFB0:
        .cfi_startproc
        pushq   %rbp
        .cfi_def_cfa_offset 16
        .cfi_offset 6, -16
        movq    %rsp, %rbp
        .cfi_def_cfa_register 6
        subq    $16, %rsp
        movq    .LC0(%rip), %rax
        movq    %rax, -8(%rbp)
        movsd   -8(%rbp), %xmm0
        leaq    .LC1(%rip), %rdi
        movl    $1, %eax
        call    printf@PLT
        leaq    .LC2(%rip), %rdi
        call    puts@PLT
        movl    $0, %eax
        leave
        .cfi_def_cfa 7, 8
        ret
        .cfi_endproc
.LFE0:
        .size   main, .-main
        .section        .rodata
        .align 8
.LC0:
        .long   1374389535
        .long   1074339512
        .ident  "GCC: (Ubuntu 7.5.0-3ubuntu1~18.04) 7.5.0"
        .section        .note.GNU-stack,"",@progbits

```
汇编 生成 可重定位二进制目标程序
gcc编译器 -c参数 起到的作用是编译和汇编 -S参数 仅仅是编译

`g++ -c hello.s`

链接 生成执行文件

`g++ hello.o -o hello`


在不同进程间切换 交错执行的执行-上下文切换
操作系统保持进程运行所需的所有状态信息 这种状态-上下文

线程共享代码和全局数据

# 第一部分 程序结构和执行
## 第二章 信息存储

孤立地讲,单个位不是非常有用. 然而, 当把位组合在一起, 再加上某种解释,即赋予不同可能位组合不同的含义, 我们就能表示任何有限集合的元素.

## 第五章 优化程序性能

编写高效的程序
- 选择一组适当的算法和数据结构
- 编写出编译器能够有效优化以转变为高效的可执行程序的源代码
- 大项目的并行计算


整数运算使用乘法的结合律和交换律, 当溢出的时候, 结果依然是一致的.
但是浮点数使用结合律和交换律的时候, 在溢出时, 结果却不是一致的.

整数的表示虽然只能编码一个相对较小的数值范围, 但是这种标识是精确地
浮点数能编码一个较大的数值范围, 但是这种表示只是近似的.

## 第十一章 网络编程

从网络上接收到的数据经过网络适配器 IO总线 内存总线复制到内存, 通常使用的DMA传送. 反过来就是内存到网络的方式

我们的实例抓住了互联网络思想的精髓, 封装是关键


应改为IP地址, 确切地说是Ipv4地址定义一个特殊的类型, 不过标准成立后已经太迟了. 不过看着ipv6的地址表示, 获取标准成功了? 提供了一个看起来特殊的类型

```c++
typedef uint32_t in_addr_t;
struct in_addr
{
in_addr_t s_addr;
};


struct in6_addr
{
union
{
uint8_t	__u6_addr8[16];
uint16_t __u6_addr16[8];
uint32_t __u6_addr32[4];
} __in6_u;
};
```

socket函数返回的套接字被操作系统默认为是主动实体(客户端), 通过listen告知操作系统这是被动实体(服务器)

**getaddrinfo函数**

```c++
#include <netdb.h>

struct addrinfo
{
    int              ai_flags; // hints 
    int              ai_family; // hints AF_INET 限制返回ipv4 AF_INET6限制返回ipv6 不指定则二者都有
    int              ai_socktype; // hints
    int              ai_protocol; // hints
    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname; // 只有设置了AI_CANONNAME 这里将保存host的官方名字
    struct addrinfo *ai_next; // 链表
};

// node: 可以是域名 点分十进制ip nullptr 
//      AI_PASSIVE 告知返回的套接字地址可能用于listen套接字 此时应该设置为nullptr
// service: 端口号, 服务名http  
//      AI_NUMERICSERV 强制此参数为端口号
// addrinfo 设置后可以控制返回的result

// AI_ADDRINFO 只有当本机设置了ipv4才查询ipv4地址 ipv6亦然
int getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints,
                       struct addrinfo **result);

void freeaddrinfo(struct addrinfo *res);

const char *gai_strerror(int errcode);
```

调用`getaddrinfo`生成result链表后, 遍历链表直到socket和bind调用成功(socket和connect调用成功)


**getnameinfo函数**

```c++
// host 存储主机名 IP地址
// serv 存储端口号 服务名
// NI_NUMERICHOST 直接返回IP地址 不解析,  NI_NUMERICSERV 字节返回端口号 不解析服务名
int getnameinfo(const struct sockaddr *addr, socklen_t addrlen,
                       char *host, socklen_t hostlen,
                       char *serv, socklen_t servlen, int flags);
```

EOF并不存在对应的字符, 其是由内核检测到的一种条件