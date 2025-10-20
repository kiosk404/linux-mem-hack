#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define RESTORE_HEALTH 100
#define LOW_HEALTH 30

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("用法: %s <PID> <地址>\n", argv[0]);
        printf("例如: %s 1234 0x404040\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]);
    long addr;
    sscanf(argv[2], "%lx", &addr);

    // 打开目标进程的内存文件
    char mem_path[64];
    sprintf(mem_path, "/proc/%d/mem", pid);

    printf("监控进程 %d 的血量 (地址: 0x%lx)\n", pid, addr);

    int fd = open(mem_path, O_RDWR);
    if (fd < 0)
    {
        printf("进程已退出\n");
        return 1;
    }

    // 读取血量
    int health;
    lseek(fd, addr, SEEK_SET);
    read(fd, &health, sizeof(int));
    printf("当前血量: %d\n", health);

    int new_health = RESTORE_HEALTH;
    lseek(fd, addr, SEEK_SET);
    write(fd, &new_health, sizeof(int));

    printf("✅ 已恢复到 %d \n\n", RESTORE_HEALTH);
    close(fd);

    return 0;
}