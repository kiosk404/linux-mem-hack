#!/usr/bin/env python3
# verify_pointer_v2.py

import struct
import sys
import re

def read_pointer(pid, address):
    """读取指针值（64位）"""
    try:
        with open(f'/proc/{pid}/mem', 'rb') as f:
            f.seek(address)
            data = f.read(8)
            return struct.unpack('Q', data)[0]
    except Exception as e:
        print(f"    读取指针失败: {e}")
        return None

def read_int32(pid, address):
    """读取 int32 值"""
    try:
        with open(f'/proc/{pid}/mem', 'rb') as f:
            f.seek(address)
            data = f.read(4)
            return struct.unpack('I', data)[0]
    except Exception as e:
        print(f"    读取值失败: {e}")
        return None

def get_game_segments(pid):
    """获取所有 game 相关的内存段"""
    maps_file = f'/proc/{pid}/maps'
    segments = []
    
    with open(maps_file, 'r') as f:
        for line in f:
            if 'game' in line:
                parts = line.split()
                addr_range = parts[0]
                perms = parts[1]
                path = parts[-1] if len(parts) > 5 else ''
                
                start, end = addr_range.split('-')
                segments.append({
                    'start': int(start, 16),
                    'end': int(end, 16),
                    'perms': perms,
                    'path': path
                })
    
    return segments

def find_rw_segments(pid):
    """查找所有可读写的 game 段"""
    segments = get_game_segments(pid)
    rw_segments = [s for s in segments if 'rw-p' in s['perms']]
    return rw_segments

def verify_pointer_chain(pid, base_addr, offsets):
    """验证指针链"""
    print(f"\n[*] 验证指针链: base=0x{base_addr:x}, offsets={[hex(o) for o in offsets]}")
    current_addr = base_addr
    
    for i, offset in enumerate(offsets[:-1]):
        addr_to_read = current_addr + offset
        print(f"  [{i+1}] 读取地址: 0x{addr_to_read:x} (base+0x{offset:x})")
        
        ptr = read_pointer(pid, addr_to_read)
        if ptr is None:
            print(f"      ❌ 无法读取指针")
            return None
        
        print(f"      -> 指针值: 0x{ptr:x}")
        current_addr = ptr
    
    # 最后一层偏移
    final_offset = offsets[-1]
    final_addr = current_addr + final_offset
    print(f"  [{len(offsets)}] 最终地址: 0x{final_addr:x} (+0x{final_offset:x})")
    
    # 读取血量值
    health = read_int32(pid, final_addr)
    if health is not None:
        print(f"      ✅ 血量值: {health}")
        return final_addr
    else:
        print(f"      ❌ 无法读取血量值")
        return None

if __name__ == "__main__":
    import os
    
    # 获取游戏 PID
    pid_str = os.popen("pidof game").read().strip()
    if not pid_str:
        print("[-] 游戏未运行")
        sys.exit(1)
    
    pid = int(pid_str)
    print(f"[+] 游戏 PID: {pid}")
    
    # 列出所有 game 的可读写段
    rw_segments = find_rw_segments(pid)
    
    if not rw_segments:
        print("[-] 未找到可读写段")
        sys.exit(1)
    
    print(f"\n[+] 找到 {len(rw_segments)} 个可读写段:")
    for i, seg in enumerate(rw_segments, 1):
        print(f"  [{i}] 0x{seg['start']:x}-0x{seg['end']:x} ({seg['perms']})")
    
    # PINCE 的 game[4] 可能指的是第4个段（从0开始是第5个）
    # 或者是第4个可读写段
    # 让我们尝试所有可能性
    
    print("\n" + "="*60)
    print("开始测试所有可能的基址...")
    print("="*60)
    
    # 测试指针: game[4]+28->0->30
    test_offsets = [
        [0x28, 0x0, 0x30],      # game[4]+28->0->30
        [0x28, 0x40, 0x0],      # game[4]+28->40->0
        [0x28, 0x50],           # game[4]+28->50
        [0x8, 0x20, 0x50],      # game[4]+8->20->50
    ]
    
    success = False
    
    for seg_idx, segment in enumerate(rw_segments, 1):
        base = segment['start']
        print(f"\n{'='*60}")
        print(f"测试段 [{seg_idx}]: 基址 0x{base:x}")
        print(f"{'='*60}")
        
        for offset_idx, offsets in enumerate(test_offsets, 1):
            print(f"\n--- 测试指针组合 {offset_idx}: {' -> '.join([hex(o) for o in offsets])} ---")
            result = verify_pointer_chain(pid, base, offsets)
            
            if result:
                print(f"\n🎉 找到有效指针！")
                print(f"   基址: 0x{base:x} (段 {seg_idx})")
                print(f"   偏移: {' -> '.join([hex(o) for o in offsets])}")
                print(f"   血量地址: 0x{result:x}")
                success = True
                break
        
        if success:
            break
    
    if not success:
        print("\n❌ 所有指针都无效，可能需要重新扫描")
