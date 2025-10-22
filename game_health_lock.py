#!/usr/bin/env python3
"""
game_health_hack.py - 自动锁血外挂
指针链: game[4]+28->0->30
功能: 自动检测游戏进程，解析指针链，锁定血量为100
"""

import os
import sys
import time
import struct
import signal

# 配置
TARGET_PROCESS = "game"
TARGET_HEALTH = 100
CHECK_INTERVAL = 0.1  # 100ms
POINTER_CHAIN = [0x28, 0x0, 0x30]  # game[4]+28->0->30

# 全局变量
running = True

def signal_handler(sig, frame):
    """处理 Ctrl+C 信号"""
    global running
    print("\n\n[*] 正在退出...")
    running = False
    sys.exit(0)

def get_pid(process_name):
    """获取进程 PID"""
    try:
        output = os.popen(f"pidof {process_name}").read().strip()
        if output:
            # 如果有多个进程，取第一个
            pid = int(output.split()[0])
            return pid
    except:
        pass
    return None

def get_game_base_address(pid, segment_index=4):
    """
    从 /proc/PID/maps 获取 game 的基址
    segment_index: game 模块的第几个段（通常 game[4] 是第5个段，索引从0开始）
    """
    maps_path = f"/proc/{pid}/maps"
    
    try:
        with open(maps_path, 'r') as f:
            game_segments = []
            for line in f:
                if TARGET_PROCESS in line:
                    # 解析地址范围
                    addr_range = line.split()[0]
                    start_addr = int(addr_range.split('-')[0], 16)
                    perms = line.split()[1]
                    
                    game_segments.append({
                        'address': start_addr,
                        'perms': perms,
                        'line': line.strip()
                    })
            
            if len(game_segments) <= segment_index:
                print(f"[-] 错误: 找到 {len(game_segments)} 个段，但需要访问索引 {segment_index}")
                print(f"[-] 可用的段:")
                for i, seg in enumerate(game_segments):
                    print(f"    [{i}] 0x{seg['address']:x} ({seg['perms']})")
                return None
            
            base_addr = game_segments[segment_index]['address']
            print(f"[+] game[{segment_index}] 基址: 0x{base_addr:x}")
            return base_addr
            
    except Exception as e:
        print(f"[-] 读取内存映射失败: {e}")
        return None

def read_memory_qword(pid, address):
    """读取 8 字节 (64位指针)"""
    mem_path = f"/proc/{pid}/mem"
    try:
        with open(mem_path, 'rb') as f:
            f.seek(address)
            data = f.read(8)
            if len(data) == 8:
                return struct.unpack('Q', data)[0]
    except Exception as e:
        # print(f"[-] 读取指针失败 @ 0x{address:x}: {e}")
        pass
    return None

def read_memory_dword(pid, address):
    """读取 4 字节 (int32)"""
    mem_path = f"/proc/{pid}/mem"
    try:
        with open(mem_path, 'rb') as f:
            f.seek(address)
            data = f.read(4)
            if len(data) == 4:
                return struct.unpack('I', data)[0]
    except Exception as e:
        # print(f"[-] 读取值失败 @ 0x{address:x}: {e}")
        pass
    return None

def write_memory_dword(pid, address, value):
    """写入 4 字节 (int32)"""
    mem_path = f"/proc/{pid}/mem"
    try:
        with open(mem_path, 'r+b') as f:
            f.seek(address)
            f.write(struct.pack('I', value))
            return True
    except Exception as e:
        print(f"[-] 写入失败 @ 0x{address:x}: {e}")
        return False

def resolve_pointer_chain(pid, base_address, offsets):
    """
    解析指针链
    base_address: 基址
    offsets: 偏移量列表，例如 [0x28, 0x0, 0x30]
    返回: 最终地址，如果失败返回 None
    """
    current_address = base_address
    
    # 遍历除最后一个之外的所有偏移（这些是指针）
    for i, offset in enumerate(offsets[:-1]):
        # 计算当前指针地址
        pointer_address = current_address + offset
        
        # 读取指针指向的地址
        next_address = read_memory_qword(pid, pointer_address)
        
        if next_address is None or next_address == 0:
            # print(f"[-] 指针链断裂在第 {i+1} 层 @ 0x{pointer_address:x}")
            return None
        
        # print(f"[DEBUG] 层{i+1}: 0x{pointer_address:x} -> 0x{next_address:x}")
        current_address = next_address
    
    # 最后一层是实际值的偏移
    final_address = current_address + offsets[-1]
    return final_address

def monitor_and_lock_health(pid, base_address, pointer_chain, target_health):
    """
    主循环: 监控并锁定血量
    """
    global running
    
    print(f"\n[*] 开始监控血量...")
    print(f"[*] 目标血量: {target_health}")
    print(f"[*] 检查间隔: {int(CHECK_INTERVAL * 1000)}ms")
    print(f"[*] 按 Ctrl+C 停止\n")
    print("-" * 60)
    
    last_health = None
    lock_count = 0
    
    while running:
        try:
            # 解析指针链获取血量地址
            health_address = resolve_pointer_chain(pid, base_address, pointer_chain)
            
            if health_address is None:
                print("\r[!] 指针链失效，等待重新连接...", end='', flush=True)
                time.sleep(CHECK_INTERVAL)
                continue
            
            # 读取当前血量
            current_health = read_memory_dword(pid, health_address)
            
            if current_health is None:
                print("\r[!] 无法读取血量，等待...", end='', flush=True)
                time.sleep(CHECK_INTERVAL)
                continue
            
            # 只在血量变化时打印
            if current_health != last_health:
                timestamp = time.strftime("%H:%M:%S")
                print(f"\r[{timestamp}] 当前血量: {current_health:4d} | 地址: 0x{health_address:x}", end='')
                
                # 如果血量低于目标值，等待100ms后修改
                if current_health < target_health:
                    print(f" -> 血量过低! ", end='', flush=True)
                    time.sleep(0.1)  # 延迟 100ms
                    
                    # 修改血量
                    if write_memory_dword(pid, health_address, target_health):
                        # 验证修改成功
                        new_health = read_memory_dword(pid, health_address)
                        if new_health == target_health:
                            lock_count += 1
                            print(f"已锁定为 {target_health} ✓ (第{lock_count}次)", flush=True)
                        else:
                            print(f"修改失败", flush=True)
                    else:
                        print(f"写入失败", flush=True)
                else:
                    print(flush=True)
                
                last_health = current_health
            
            # 等待下次检查
            time.sleep(CHECK_INTERVAL)
            
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"\n[-] 错误: {e}")
            time.sleep(1)
    
    print(f"\n\n[*] 总共锁定血量 {lock_count} 次")
    print("[*] 外挂已停止")

def main():
    """主函数"""
    # 检查 root 权限
    if os.geteuid() != 0:
        print("[-] 错误: 需要 root 权限访问 /proc/PID/mem")
        print("[*] 请使用: sudo python3", sys.argv[0])
        sys.exit(1)
    
    # 注册信号处理
    signal.signal(signal.SIGINT, signal_handler)
    
    print("=" * 60)
    print("      游戏血量锁定外挂 v1.0")
    print("=" * 60)
    print(f"目标进程: {TARGET_PROCESS}")
    print(f"指针链: game[4]+{hex(POINTER_CHAIN[0])}->", end='')
    print("->".join([hex(o) for o in POINTER_CHAIN[1:]]))
    print("=" * 60)
    
    # 1. 查找游戏进程
    print("\n[*] 正在搜索游戏进程...")
    pid = get_pid(TARGET_PROCESS)
    
    if pid is None:
        print(f"[-] 错误: 未找到进程 '{TARGET_PROCESS}'")
        print(f"[*] 请先启动游戏")
        sys.exit(1)
    
    print(f"[+] 找到进程: PID = {pid}")
    
    # 2. 从 /proc 读取基址
    print(f"\n[*] 从 /proc/{pid}/maps 读取内存布局...")
    base_address = get_game_base_address(pid, segment_index=4)
    
    if base_address is None:
        print(f"[-] 错误: 无法获取基址")
        sys.exit(1)
    
    # 3. 验证指针链
    print(f"\n[*] 验证指针链...")
    health_address = resolve_pointer_chain(pid, base_address, POINTER_CHAIN)
    
    if health_address is None:
        print(f"[-] 错误: 指针链验证失败")
        print(f"[*] 可能的原因:")
        print(f"    1. 游戏版本不匹配")
        print(f"    2. 指针链已过期，需要重新扫描")
        print(f"    3. 内存布局发生变化")
        sys.exit(1)
    
    # 读取初始血量
    initial_health = read_memory_dword(pid, health_address)
    if initial_health is None:
        print(f"[-] 错误: 无法读取血量值")
        sys.exit(1)
    
    print(f"[+] 指针链有效!")
    print(f"[+] 血量地址: 0x{health_address:x}")
    print(f"[+] 当前血量: {initial_health}")
    
    # 4. 进入监控循环
    monitor_and_lock_health(pid, base_address, POINTER_CHAIN, TARGET_HEALTH)

if __name__ == "__main__":
    main()
