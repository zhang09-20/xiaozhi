#!/usr/bin/env python3
"""
验证P3文件格式的工具
"""

import struct
import sys

def verify_p3_file(filename):
    """验证P3文件格式"""
    print(f"验证P3文件: {filename}")
    
    try:
        with open(filename, 'rb') as f:
            frame_count = 0
            total_size = 0
            
            while True:
                # 读取头部
                header = f.read(4)
                if not header or len(header) < 4:
                    break
                
                # 解析头部
                packet_type, reserved, payload_size = struct.unpack('>BBH', header)
                
                # 读取payload
                payload = f.read(payload_size)
                if len(payload) != payload_size:
                    print(f"警告: 帧 {frame_count} payload不完整")
                    break
                
                frame_count += 1
                total_size += 4 + payload_size
                
                if frame_count <= 5:  # 显示前5帧的信息
                    print(f"帧 {frame_count}: type={packet_type}, reserved={reserved}, size={payload_size}")
        
        print(f"\n文件验证完成:")
        print(f"总帧数: {frame_count}")
        print(f"总大小: {total_size} 字节")
        print(f"估计时长: {frame_count * 60 / 1000:.2f} 秒")
        
        return True
        
    except Exception as e:
        print(f"验证失败: {e}")
        return False

def main():
    if len(sys.argv) != 2:
        print("用法: python verify_p3.py <p3文件>")
        return 1
    
    filename = sys.argv[1]
    success = verify_p3_file(filename)
    return 0 if success else 1

if __name__ == '__main__':
    sys.exit(main())
