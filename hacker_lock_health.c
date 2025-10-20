#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

// ========== 配置常量 (根据分析结果设置) ==========

// 1. 目标变量在 ELF 文件中的静态偏移量
// 根据计算结果: 0x6343a6adc010 - 0x6343a6ad9000 = 0x3010
#define STATIC_OFFSET 0x3010 

// 2. 目标进程的可执行文件名（请根据您的实际路径修改）
// 假设 game.c 编译后的可执行文件名为 'game'，且位于 ./game/ 目录下
#define TARGET_PROCESS_NAME "/home/kiosk/Projects/linux-mem-hack/game" 

// 3. 要锁定的血量值 (101)
#define LOCKED_HEALTH 101

// 查找目标进程的加载基址（Base Address）
long find_base_address(pid_t pid) {
    char maps_path[256];
    char line[1024];
    FILE *fp;
    long base_address = 0;

    // 构造 /proc/<pid>/maps 路径
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);

    fp = fopen(maps_path, "r");
    if (fp == NULL) {
        // 如果文件不存在或权限不足，通常是因为没有使用 sudo 运行外挂
        perror("Error opening /proc/maps (Did you run with sudo?)");
        return 0;
    }

    // 逐行读取 maps 文件，寻找目标可执行文件的第一个映射区域
    while (fgets(line, sizeof(line), fp) != NULL) {
        // 检查行尾是否包含目标程序名
        if (strstr(line, TARGET_PROCESS_NAME) != NULL) {
            // 解析地址范围的起始地址，即基址
            if (sscanf(line, "%lx-", &base_address) == 1) {
                // 找到第一个映射地址即为加载基址
                break;
            }
        }
    }

    fclose(fp);
    return base_address;
}

// 通过 ptrace 写入内存
int write_memory(pid_t pid, long addr, long value) {
    // PTRACE_POKEDATA 用于向进程内存写入一个机器字长的数据（通常是 8 字节）
    if (ptrace(PTRACE_POKEDATA, pid, (void *)addr, (void *)value) == -1) {
        // 写入失败可能是进程已退出
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <PID of the game>\n", argv[0]);
        fprintf(stderr, "NOTE: Please make sure to update TARGET_PROCESS_NAME in the source code.\n");
        return 1;
    }

    pid_t game_pid = atoi(argv[1]);
    long base_address;
    long target_address;

    printf("[+] Attaching to process %d...\n", game_pid);

    // 1. 使用 ptrace 附加进程 (需要权限)
    if (ptrace(PTRACE_ATTACH, game_pid, NULL, NULL) == -1) {
        perror("Error attaching process (You must run this with 'sudo')");
        return 1;
    }

    // 等待进程停止 (ptrace attachment会暂停目标进程)
    waitpid(game_pid, NULL, 0);
    printf("[+] Successfully attached.\n");

    // 2. 查找基址
    base_address = find_base_address(game_pid);
    if (base_address == 0) {
        fprintf(stderr, "[-] Could not find base address for the game executable. Check TARGET_PROCESS_NAME in code.\n");
        ptrace(PTRACE_DETACH, game_pid, NULL, NULL);
        return 1;
    }

    // 3. 计算目标血量地址：新基址 + 静态偏移量
    target_address = base_address + STATIC_OFFSET;

    printf("[*] Base Address: 0x%lx\n", base_address);
    printf("[*] Static Offset: 0x%x\n", STATIC_OFFSET);
    printf("[*] Target Health Address: 0x%lx\n", target_address);
    printf("-----------------------------------------\n");
    printf("[+] Locking health at %d...\n", LOCKED_HEALTH);

    // 4. 持续写入目标值 (锁血循环)
    long health_value = LOCKED_HEALTH; 

    // 恢复进程运行
    ptrace(PTRACE_CONT, game_pid, NULL, NULL);

    while (1) {
        // 尝试写入内存
        if (write_memory(game_pid, target_address, health_value) == -1) {
            fprintf(stderr, "[-] Target process seems to have terminated or lost PTRACE access. Exiting.\n");
            break;
        }
        // 每 10 毫秒写入一次
        usleep(10000); 
    }
    
    // 5. 分离进程
    if (ptrace(PTRACE_DETACH, game_pid, NULL, NULL) == -1) {
        // 忽略 DETACH 错误，因为进程可能已经退出
    } else {
        printf("[+] Successfully detached from the game.\n");
    }

    return 0;
}
