#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

#define LOCKED_HEALTH 100

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("用法: %s <PID> <地址>\n", argv[0]);
        return 1;
    }

    int target_pid = atoi(argv[1]);
    long target_addr;
    sscanf(argv[2], "%lx", &target_addr);
    
    // 附加进程
    if (ptrace(PTRACE_ATTACH, target_pid, NULL, NULL) == -1) {
        perror("附加失败");
        return 1;
    }
    
    waitpid(target_pid, NULL, 0);
    
    // 读取原数据（保留其他字节）
    long original_data = ptrace(PTRACE_PEEKDATA, target_pid, (void*)target_addr, NULL);

    printf("当前血量: %ld\n", original_data);
    
    // 只修改低4字节为100
    long data_to_write = LOCKED_HEALTH;
    
    // 写入数据
    if (ptrace(PTRACE_POKEDATA, target_pid, (void*)target_addr, (void*)data_to_write) == -1) {
        perror("写入失败");
        ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
        return 1;
    }
    
    printf("血量已修改为 100\n");
    
    // 分离进程
    ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
    
    return 0;
}
