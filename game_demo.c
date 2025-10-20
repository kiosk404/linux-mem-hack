#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

// 这是一个全局变量，在 Data/BSS 段存储
// 作弊者会寻找并修改它
volatile int player_health = 100;
volatile int damage_counter = 0; // 用于增加血量变化的复杂度

void take_damage() {
    // 模拟受到伤害
    if (player_health > 0) {
        player_health -= (rand() % 5 + 1); // 随机减少 1 到 5 点血
        damage_counter++;
    }
}

void heal() {
    // 偶尔进行少量恢复，防止游戏进程立刻结束
    if (player_health < 100) {
        player_health += (rand() % 3);
        if (player_health > 100) {
            player_health = 100;
        }
    }
}

int main() {
    // 设置随机数种子
    srand(time(NULL));

    // 获取并打印当前进程ID (PID)，这是作弊工具附加进程的关键
    pid_t pid = getpid();
    printf("--- 简易游戏Demo (PID: %d) ---\n", pid);

    // 打印血量变量的内存地址，这是作弊者定位的起点
    printf("玩家血量 (player_health) 地址: %p\n", &player_health);
    printf("--------------------------------\n");

    // 主循环：模拟游戏运行
    while (player_health > 0) {
        
        take_damage();
        
        // 每 10 次伤害，稍微恢复一些
        if (damage_counter % 10 == 0) {
            heal();
        }
        
        printf("当前血量: %d\n", player_health);
        
        // 睡眠一秒，模拟游戏运行速率
        sleep(1);
    }

    printf("\n游戏结束 (Game Over)! 玩家血量降为 0。\n");
    return 0;
}
