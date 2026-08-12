# 假 U 盘容量修改指南

本目录包含 exFAT 假盘布局生成器,支持 2TB(MBR)~ 任意大小(GPT)的假 U 盘。

## 快速开始(改容量)

```bash
# 生成布局 + C 数组(终端输出全部参数)
python Tools/gen_layout.py 4T gpt
python Tools/gen_layout.py 2T mbr
python Tools/gen_layout.py 20T gpt 512K     # 指定簇大小(默认 512K)
python Tools/gen_layout.py 512G mbr

# 输出
# 1) 终端:布局参数 + sector_read 修改清单
# 2) Tools/arrays_generated.c:6 个 C 数组(替换 usb_msc.c 同名数组)
```

## 修改固件清单(按脚本输出逐一改)

| # | 位置(usb_msc.c) | 说明 |
|---|---|---|
| 1 | `g_msc_capacity` | 磁盘扇区数(如 0x100000000 = 2TiB) |
| 2 | `fake_pmbr`/`fake_mbr` | GPT 保护 MBR / MBR 分区表 |
| 3 | `fake_gpt_head`/`fake_gpt_head_bak` | GPT 主/备头(92B,CRC32 已算) |
| 4 | `fake_gpt_entries` | GPT 分区表项(128B) |
| 5 | `fake_exfat_boot` | 引导区扇区 0(VolumeLength/FatLength/ClusterCount/RootCluster) |
| 6 | `fake_exfat_root` | 根目录 3 目录项(位图 DE/Up-case DE/卷标) |
| 7 | sector_read 各分支 | FAT/位图/根目录/Up-case 地址 + 范围 |
| 8 | sector_read FAT 头 | FAT[0..N] 头字节(位图簇链) |
| 9 | sector_read 校验和 4B | 引导区校验和 LE |
| 10 | sector_read 位图首字节 | 已分配簇位图 |

> GPT 模式额外:sector_read 备份 GPT 分支(0x...DF..0x...FE + 头 0x...FF)
> MBR 模式:容量 ≤ 2TiB(分区表 32 位限制);>2TiB 必须 GPT

## 布局公式

```
ClusterHeapOffset = FatOffset(64) + FatLength
ClusterCount      = (VolumeLength - ClusterHeapOffset) / 2^shift  向下取整
FatLength         = ceil((ClusterCount+2)×4 / 512)                与上迭代求解
位图字节          = ceil(ClusterCount/8)
位图簇数          = ceil(位图字节 / (簇大小×512))                   ≥1
RootCluster       = 2 + 位图簇数        # 位图占簇 2..(位图簇数+1)
Up-case 簇        = RootCluster + 1
引导区校验和      = 右移算法,覆盖扇区 0-10,跳过 106/107/112
GPT 头/数组 CRC   = zlib CRC32(标准,非 exFAT 右移!)
```

## 关键注意事项

1. **校验和**:改了引导区任何字段,校验和全变,必须重算并更新 sector_read 里写死的 4B
2. **MBR 上限 2TiB**:`0xFFFFFFFF` 扇区;更大必须 GPT
3. **>2TiB 需要 SCSI 16 位命令**(已实现):
   - READ CAPACITY(16) 0x9E / READ(16) 0x88 / WRITE(16) 0x8A
   - **READ CAPACITY(10) 必须返回 0xFFFFFFFF**,否则 Windows 认为磁盘 ≤2TiB,不发 (16),GPT 分区超界被拒
4. **sector 参数 64 位**:备份 GPT LBA > 2^32,`usbd_msc_sector_read/write` 的 sector 已是 uint64_t
5. **位图簇数 >1**:RootCluster 自动后移,FAT 头包含位图簇链(如 20TiB:FAT[2..10]=3..11)
6. **挂载速度**:位图越大 Windows 读取越多;512KB 簇下 2TiB 位图=1 簇(最快),20TiB=10 簇(挂载仍 1s,全量读取后台)
7. **Up-case 表**:60B 压缩表固定(校验和 0x4E394AE1,与容量无关),无需重算

## 验证命令

```powershell
# 挂载检测(可靠,Test-Path 不卡)
powershell -File Tools/check_mount.ps1 20
# 状态
Get-Disk 2 | fl PartitionStyle,Size
Get-Volume -DriveLetter E
```

## 版本记录

| 分支 | 容量 | 分区 | 提交 |
|---|---|---|---|
| composite | 2TiB | MBR exFAT | a85bfa3, 7586376 |
| exfat-gpt | 4TiB | GPT exFAT | ae86c9b |
| exfat-gpt | 20TiB | GPT exFAT | aad0fcc |
