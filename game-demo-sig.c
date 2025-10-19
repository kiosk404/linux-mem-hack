#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>

// 这是一个全局变量，在 Data/BSS 段存储
// 作弊者会寻找并修改它
volatile int player_health = 100;
volatile int damage_counter = 0; // 用于增加血量变化的复杂度

// 信号处理函数：收到信号时触发伤害
void signal_handler(int signum) {
    if (signum == SIGUSR1) {
        if (player_health > 0) {
            int damage = rand() % 10 + 1; // 随机减少 1 到 10 点血
            player_health -= damage;
            damage_counter++;
            printf("\n>>> 受到攻击! 减少 %d 点血量 <<<\n", damage);
            
            if (player_health <= 0) {
                player_health = 0;
                printf("\n游戏结束 (Game Over)! 玩家血量降为 0。\n");
                exit(0);
            }
        }
    }
}

int main() {
    // 设置随机数种子
    srand(time(NULL));

    // 注册信号处理函数
    signal(SIGUSR1, signal_handler);

    // 获取并打印当前进程ID (PID)，这是作弊工具附加进程的关键
    pid_t pid = getpid();
    printf("--- 简易游戏Demo (PID: %d) ---\n", pid);

    // 打印血量变量的内存地址，这是作弊者定位的起点
    printf("玩家血量 (player_health) 地址: %p\n", &player_health);
    printf("--------------------------------\n");
    printf("发送信号以造成伤害: kill -SIGUSR1 %d\n", pid);
    printf("--------------------------------\n");

    // 主循环：持续打印当前血量
    while (1) {
        printf("当前血量: %d\n", player_health);
        
        // 睡眠一秒，模拟游戏运行速率
        sleep(1);
    }

    return 0;
}