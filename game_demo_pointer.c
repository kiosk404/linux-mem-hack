#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <stdatomic.h>
#include <bits/sigaction.h>


#define MAX_HEALTH 100

/* ---------- 简化的“真实游戏”内存结构 ---------- */
typedef struct {
    int health;
    int max_health;
} HealthComponent;

typedef struct {
    char name[32];
    HealthComponent *health_comp;
} Player;

typedef struct {
    Player *player;
} GameWorld;

/* 全局根指针（模拟引擎的全局 World） */
static GameWorld *g_world = NULL;

/* 信号安全计数器：signal handler 仅递增它 */
volatile sig_atomic_t pending_hits = 0;
/* 允许用 Ctrl-C 退出 */
volatile sig_atomic_t do_exit = 0;

/* 信号处理器：只做最小的 async-signal-safe 操作 */
void sigusr1_handler(int signum) {
    if (signum == SIGUSR1) {
        if (pending_hits < 10000) pending_hits++;
    }
}

void sigint_handler(int signum) {
    (void)signum;
    do_exit = 1;
}

/* 初始化世界与唯一玩家 */
void init_world(void) {
    g_world = malloc(sizeof(GameWorld));
    if (!g_world) { perror("malloc g_world"); exit(1); }
    memset(g_world, 0, sizeof(GameWorld));

    Player *p = malloc(sizeof(Player));
    if (!p) { perror("malloc player"); exit(1); }
    memset(p, 0, sizeof(Player));
    strncpy(p->name, "Player1", sizeof(p->name)-1);

    HealthComponent *hc = malloc(sizeof(HealthComponent));
    if (!hc) { perror("malloc hc"); exit(1); }
    hc->max_health = MAX_HEALTH;
    hc->health = MAX_HEALTH;

    p->health_comp = hc;
    g_world->player = p;
}

/* 打印单玩家血条 */
void print_health_bar(Player *p) {
    int h = p->health_comp->health;
    int max = p->health_comp->max_health;
    int bar_len = 40;
    int filled = (h * bar_len) / max;

    printf("\r%-10s [", p->name);
    for (int i = 0; i < bar_len; ++i) putchar(i < filled ? '#' : ' ');
    printf("] %3d/%3d   ", h, max);
    fflush(stdout);
}

int main(void) {
    srand((unsigned)time(NULL));
    init_world();

    /* 注册信号 */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) != 0) {
        perror("sigaction SIGUSR1");
        return 1;
    }

    struct sigaction sa2;
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = sigint_handler;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = 0;
    sigaction(SIGINT, &sa2, NULL);

    /* 打印 PID 与指针链地址，便于学习与调试 */
    printf("=== Single-player game (signal-driven) ===\n");
    printf("PID: %d\n", (int)getpid());
    printf("g_world (global ptr) addr         : %p\n", (void*)&g_world);
    printf("g_world value (GameWorld *)       : %p\n", (void*)g_world);
    printf("g_world->player (Player *) addr   : %p\n", (void*)&g_world->player);
    printf("g_world->player value             : %p\n", (void*)g_world->player);
    printf("player->health_comp (HealthComp*) : %p\n", (void*)&g_world->player->health_comp);
    printf("player->health_comp value         : %p\n", (void*)g_world->player->health_comp);
    printf("player->health (int *)            : %p\n", (void*)&g_world->player->health_comp->health);
    printf("------------------------------------------\n");
    printf("Send SIGUSR1 to cause damage:  kill -USR1 %d\n", (int)getpid());
    printf("Press Ctrl-C to exit\n\n");

    /* 主循环：周期打印血条，并处理 pending_hits（每次 hit 随机 1..10） */
    while (!do_exit) {
        /* 周期性打印血条（一次处理循环后，打印当前血量） */
        if (g_world && g_world->player) {
            print_health_bar(g_world->player);
        }
        /* pause 等待信号被触发（被信号打断后继续） */
        pause();

        /* 处理所有 pending_hits（在正常上下文安全调用 rand/printf） */
        while (pending_hits > 0 && !do_exit) {
            pending_hits--;
            if (g_world && g_world->player && g_world->player->health_comp) {
                HealthComponent *hc = g_world->player->health_comp;
                if (hc->health > 0) {
                    int dmg = rand() % 4 + 1; // 每滴 1..4 点
                    hc->health -= dmg;
                    if (hc->health < 0) hc->health = 0;
                    printf("\n>>> hit: -%d HP (now %d/%d)\n", dmg, hc->health, hc->max_health);
                    fflush(stdout);
                    if (hc->health <= 0) {
                        printf("\n💀 Player died. Exiting.\n");
                        do_exit = 1;
                        break;
                    }
                }
            }
        }
    }

    printf("\nCleanup and exit.\n");
    return 0;
}
