#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""exFAT 假盘布局生成器(整合版)

生成 2TB~20TiB 假 U 盘所需的全部参数 + C 数组:
  - MBR / GPT(PMBR+主备头+分区表)结构
  - exFAT 引导区扇区 0 + 校验和
  - 根目录 3 个目录项(位图/Up-case/卷标)
  - FAT 头(含位图簇链)
  - 位图首字节
  - sector_read 需修改的全部值

用法:
  python gen_layout.py <容量> <mbr|gpt> [簇大小]

  容量:  512G / 2T / 4T / 20T / 512G_sec(扇区数,带 _sec 后缀)
         数字+单位,T=TiB,G=GiB
  格式:  mbr(MBR 分区,≤2TB)/ gpt(GPT 分区,任意大小)
  簇大小:512K(默认)/ 128K / 256K / 1M / 4M

输出:
  1) 终端打印全部参数 + sector_read 修改清单
  2) arrays_generated.c:可直接替换进 usb_msc.c 的 C 数组
  3) 校验和/FAT 头/位图字节等全部自动计算

依赖:仅标准库(zlib)
"""
import sys
import zlib

if sys.stdout and hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# ---------------------------------------------------------------- 工具

def parse_capacity(s):
    """'2T' -> (0x100000000 扇区),'512G' -> 扇区,'N_sec' -> N"""
    if s.lower().endswith("_sec"):
        return int(s[:-4], 0)
    unit = s[-1].upper()
    val = int(s[:-1])
    if unit == "T":
        return val << 31          # TiB = 2^31 扇区
    if unit == "G":
        return val << 21          # GiB = 2^21 扇区
    raise ValueError("单位仅支持 T/G")

def chk32(data, skip=(106, 107, 112)):
    """exFAT 校验和(右移 1 位 + 字节),默认跳过 106/107/112"""
    c = 0
    for i, b in enumerate(data):
        if i in skip:
            continue
        c = ((c & 1) and 0x80000000 or 0) + (c >> 1) + b
        c &= 0xFFFFFFFF
    return c

def c_arr(name, data):
    lines = [f"static const uint8_t {name}[{len(data)}] = {{"]
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)

# ---------------------------------------------------------------- 布局

def layout(cap_sec, fmt, spc_shift):
    """返回所有参数与字节数据"""
    spc = 1 << spc_shift
    PART_LBA = 2048                       # 1MiB 对齐
    FAT_OFF = 64

    if fmt == "mbr":
        assert cap_sec <= 0x100000000, "MBR 分区上限 2TB,请用 gpt"
        part_end = cap_sec - 1
        vol = part_end - PART_LBA + 1
    else:                                  # gpt
        part_end = cap_sec - 34            # 备份 GPT(数组32+头1)
        vol = part_end - PART_LBA + 1

    # 迭代 FatLength <-> ClusterCount
    fat_len = 65536
    for _ in range(64):
        cho = FAT_OFF + fat_len
        cc = (vol - cho) // spc
        need = ((cc + 2) * 4 + 511) // 512
        if need == fat_len:
            break
        fat_len = need

    bmp_bytes = (cc + 7) // 8
    bmp_clusters = (bmp_bytes + spc*512 - 1) // (spc*512)
    root_clu = 2 + bmp_clusters            # 位图占簇 2..(bmp_clusters+1)
    up_clu = root_clu + 1
    clu2 = PART_LBA + cho                  # 位图首簇绝对 LBA

    # ---- boot 扇区 0 ----
    boot = bytearray(512)
    boot[0:3] = bytes.fromhex("eb7690"); boot[3:11] = b"EXFAT   "
    boot[64:72]  = PART_LBA.to_bytes(8, "little")
    boot[72:80]  = vol.to_bytes(8, "little")
    boot[80:84]  = FAT_OFF.to_bytes(4, "little")
    boot[84:88]  = fat_len.to_bytes(4, "little")
    boot[88:92]  = cho.to_bytes(4, "little")
    boot[92:96]  = cc.to_bytes(4, "little")
    boot[96:100] = root_clu.to_bytes(4, "little")
    boot[100:104]= (0x12345678).to_bytes(4, "little")
    boot[104:106]= (0x0100).to_bytes(2, "little")
    boot[108] = 9; boot[109] = spc_shift; boot[110] = 1; boot[111] = 0x80
    boot[510:512]= bytes.fromhex("55aa")

    # 11 扇区校验和(扇区0 + 1-8 签名 + 9-10 零)
    region = bytearray(512*11)
    region[0:512] = boot
    for sn in range(1, 9):
        region[sn*512+508:sn*512+512] = bytes.fromhex("000055aa")
    csum = chk32(region)

    # ---- FAT 头(FAT[0]=F8,FAT[1]=FF,FAT[2..bmp_clusters+1]=链,FAT[root]=EOF,FAT[up]=EOF) ----
    fat_entries = up_clu + 1
    fat = bytearray(fat_entries * 4)
    fat[0:4] = bytes.fromhex("f8ffffff")
    fat[4:8] = bytes.fromhex("ffffffff")
    for i in range(2, bmp_clusters + 2):       # 位图链 2..bmp_clusters+1
        v = i + 1 if i < bmp_clusters + 1 else 0xFFFFFFFF
        fat[i*4:i*4+4] = v.to_bytes(4, "little")
    fat[root_clu*4:root_clu*4+4] = (0xFFFFFFFF).to_bytes(4, "little")  # 根目录 EOF
    fat[up_clu*4:up_clu*4+4] = (0xFFFFFFFF).to_bytes(4, "little")      # Up-case EOF

    # ---- 位图首字节(簇 2..up_clu 已分配) ----
    b0 = b1 = 0
    for i in range(up_clu - 1):                # 簇 2..up_clu
        bit = 2 + i
        if bit < 8: b0 |= 1 << bit
        elif bit < 16: b1 |= 1 << (bit - 8)

    # ---- root DE(位图 DE + Up-case DE + 卷标) ----
    root = bytearray(96)
    root[0] = 0x81
    root[20:24] = (2).to_bytes(4, "little")
    root[24:32] = bmp_bytes.to_bytes(8, "little")
    root[32] = 0x82
    root[36:40] = (0x4E394AE1).to_bytes(4, "little")   # 60B upcase 表校验和(固定)
    root[52:56] = up_clu.to_bytes(4, "little")
    root[56:64] = (60).to_bytes(8, "little")
    lab = "EXFAT-DEMO"
    root[64] = 0x03; root[65] = len(lab)
    root[66:66+len(lab)*2] = lab.encode("utf-16-le")

    # ---- MBR / GPT ----
    if fmt == "mbr":
        mbr = bytearray(512)
        mbr[446+0] = 0x00
        mbr[446+4] = 0x07                                  # exFAT/NTFS IFS
        mbr[446+8:446+12] = PART_LBA.to_bytes(4, "little")
        mbr[446+12:446+16] = vol.to_bytes(4, "little")
        mbr[510:512] = bytes.fromhex("55aa")
        gpt = {}
    else:
        mbr = bytearray(512)
        mbr[446+4] = 0xEE                                  # 保护 MBR
        mbr[446+8:446+12] = (1).to_bytes(4, "little")
        mbr[446+12:446+16] = (0xFFFFFFFF).to_bytes(4, "little")  # 截断
        mbr[510:512] = bytes.fromhex("55aa")

        entry = bytearray(128)
        entry[0:16] = bytes.fromhex("a2a0d0ebe5b9334487c068b6b72699c7")  # Basic Data
        entry[16:32] = bytes.fromhex("11d22e0a4b4422a1b3f0c39e8a1b2c3d")
        entry[32:40] = PART_LBA.to_bytes(8, "little")
        entry[40:48] = part_end.to_bytes(8, "little")
        for i, ch in enumerate(lab):
            entry[56+i*2:58+i*2] = ch.encode("utf-16-le")
        entries = bytearray(32*512)
        entries[0:128] = entry
        array_crc = zlib.crc32(bytes(entries)) & 0xFFFFFFFF

        def gpt_header(my, alt):
            h = bytearray(512)
            h[0:8] = b"EFI PART"
            h[8:12] = (0x10000).to_bytes(4, "little")
            h[12:16] = (92).to_bytes(4, "little")
            h[24:32] = my.to_bytes(8, "little")
            h[32:40] = alt.to_bytes(8, "little")
            h[40:48] = PART_LBA.to_bytes(8, "little")
            h[48:56] = part_end.to_bytes(8, "little")
            h[56:72] = bytes.fromhex("a1b2c3d4e5f60718293a4b5c6d7e8f90")
            h[72:80] = (2).to_bytes(8, "little")
            h[80:84] = (128).to_bytes(4, "little")
            h[84:88] = (128).to_bytes(4, "little")
            h[88:92] = array_crc.to_bytes(4, "little")
            h[16:20] = (zlib.crc32(bytes(h[:92])) & 0xFFFFFFFF).to_bytes(4, "little")
            return h

        gpt = {
            "head": gpt_header(1, cap_sec-1),
            "head_bak": gpt_header(cap_sec-1, 1),
            "entries": entries,
            "bak_start": cap_sec - 33,
            "bak_head": cap_sec - 1,
        }

    return {
        "fmt": fmt, "cap_sec": cap_sec, "part_lba": PART_LBA, "part_end": part_end,
        "vol": vol, "spc_shift": spc_shift, "spc": spc,
        "fat_off": FAT_OFF, "fat_len": fat_len, "cho": cho, "cc": cc,
        "bmp_bytes": bmp_bytes, "bmp_clusters": bmp_clusters,
        "root_clu": root_clu, "up_clu": up_clu, "clu2": clu2,
        "csum": csum, "fat": fat, "b0": b0, "b1": b1,
        "boot": boot, "root": root, "mbr": mbr, "gpt": gpt,
    }

# ---------------------------------------------------------------- 输出

def print_layout(L):
    f = L["fmt"]
    print(f"===== {f.upper()} 假盘布局 =====")
    print(f"磁盘扇区      = 0x{L['cap_sec']:X} ({L['cap_sec']*512/2**40:.2f} TiB)")
    print(f"分区          = {L['part_lba']} .. 0x{L['part_end']:X}")
    print(f"卷(VolumeLength) = 0x{L['vol']:X} ({L['vol']*512/2**40:.2f} TiB)")
    print(f"簇            = {L['spc']} 扇区 (shift {L['spc_shift']})")
    print(f"FatLength     = {L['fat_len']} (0x{L['fat_len']:X})")
    print(f"ClusterHeapOffset = {L['cho']} (0x{L['cho']:X})")
    print(f"ClusterCount  = {L['cc']} (0x{L['cc']:X})")
    print(f"位图          = {L['bmp_bytes']} B = {L['bmp_clusters']} 簇链")
    print(f"RootCluster   = {L['root_clu']}")
    print(f"簇2(位图)     = {L['clu2']}")
    print(f"根目录        = {L['clu2'] + (L['root_clu']-2)*L['spc']}")
    print(f"Up-case       = {L['clu2'] + (L['up_clu']-2)*L['spc']}")
    print(f"FAT 绝对范围  = {2048+L['fat_off']} .. {2048+L['fat_off']+L['fat_len']}")
    print(f"引导区校验和  = 0x{L['csum']:08X} (LE: {L['csum'].to_bytes(4,'little').hex(' ')})")
    print(f"FAT 头        = {len(L['fat'])} B")
    print(f"位图首字节    = 0x{L['b0']:02X} 0x{L['b1']:02X}")
    if f == "gpt":
        print(f"备份分区表    = 0x{L['gpt']['bak_start']:X} .. 0x{L['gpt']['bak_start']+31:X}")
        print(f"备份 GPT 头   = 0x{L['gpt']['bak_head']:X}")
    print()
    print("===== sector_read 修改清单 =====")
    print(f"g_msc_capacity = 0x{L['cap_sec']:X}ULL")
    print(f"FAT 分支       : sector >= 2112 && sector < 2112 + {L['fat_len']}")
    print(f"根目录分支     : sector >= {L['clu2'] + (L['root_clu']-2)*L['spc']} && < +8")
    print(f"位图分支       : sector >= {L['clu2']} && < {L['clu2'] + L['bmp_clusters']*L['spc']}")
    print(f"Up-case 分支   : sector >= {L['clu2'] + (L['up_clu']-2)*L['spc']} && < +12")
    print(f"位图首字节     : buffer[0]=0x{L['b0']:02X}" + (f" buffer[1]=0x{L['b1']:02X}" if L['b1'] else ""))
    print(f"FAT 头({len(L['fat'])}B) 与校验和 LE {L['csum'].to_bytes(4,'little').hex(' ')} 见 C 数组")
    if f == "gpt":
        print(f"备份 GPT 分支  : 0x{L['gpt']['bak_start']:X}..0x{L['gpt']['bak_start']+31:X} + 头 0x{L['gpt']['bak_head']:X}")

def gen_c_arrays(L):
    arrays = [c_arr("fake_pmbr" if L['fmt']=="gpt" else "fake_mbr", L["mbr"])]
    if L["fmt"] == "gpt":
        arrays += [
            c_arr("fake_gpt_head", L["gpt"]["head"][:92]),
            c_arr("fake_gpt_head_bak", L["gpt"]["head_bak"][:92]),
            c_arr("fake_gpt_entries", L["gpt"]["entries"][:128]),
        ]
    arrays += [
        c_arr("fake_exfat_boot", L["boot"]),
        c_arr("fake_exfat_root", L["root"]),
        c_arr("fake_exfat_upcase", bytes.fromhex(
            "ffff61004100420043004400450046004700480049004a004b004c004d004e004f"
            "0050005100520053005400550056005700580059005affff85ff")),  # 60B 压缩表
    ]
    return "\n\n".join(arrays)

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    cap_sec = parse_capacity(sys.argv[1])
    fmt = sys.argv[2].lower()
    assert fmt in ("mbr", "gpt"), "格式必须是 mbr 或 gpt"
    spc_shift = {"128K": 8, "256K": 9, "512K": 10, "1M": 11, "2M": 12, "4M": 13}.get(
        sys.argv[3].upper() if len(sys.argv) > 3 else "512K", 10)

    L = layout(cap_sec, fmt, spc_shift)
    print_layout(L)
    code = gen_c_arrays(L)
    with open("arrays_generated.c", "w", encoding="utf-8", newline="\n") as f:
        f.write(code)
    print("\n已生成 arrays_generated.c(替换 usb_msc.c 中同名数组)")

if __name__ == "__main__":
    main()
