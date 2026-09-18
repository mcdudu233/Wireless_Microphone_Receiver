# USB 读卡器容量和 Windows 卡顿修复

读卡器向主机提供整张 TF 卡的原始扇区，包括分区表、FAT 和数据区。
`READ CAPACITY(10)` 的扇区数必须是 `SD_MMC.cardSize()/sectorSize()`。
当前 Arduino-ESP32 的 `SD_MMC.numSectors()` 使用
`totalBytes()/sectorSize()`，只计算 FAT 可分配的数据空间，不能用作
原始卡容量或原始扇区读写的边界。容量必须整除扇区大小，并能装入
32 bit 扇区计数；无效几何信息禁止交给 USB。

2026-09-18 故障设备：`303A:8001`，序列号 `90706915A248-SD`，
Windows 的 `PhysicalDrive2` / `I:`。通过只读磁盘几何查询得到旧固件
报告容量为 16,074,113,024 字节；Windows 分区查询得到分区偏移
32,256 字节、长度 16,107,667,968 字节，分区末端超过报告的整盘末端。
系统日志也记录了该盘的 `disk` 事件 51。根目录读取可完成，但有明显
等待。非管理员进程不能读取原始物理盘，因此未执行原始盘写入或修复。

修复后使用物理容量检查所有原始读写，保留文件系统空间统计作为
本机容量/使用量显示。MSC 就绪和容量回复同时检查 USB 介质所有权、
弹出状态、挂载和扇区几何信息。`SYNCHRONIZE CACHE(10)` 在介质
就绪时成功完成：当前 `writeRAW()` 是同步写入，返回前已完成所有
相关扇区写入，固件没有待刷新的 USB 写缓存。

MSC 软件传输缓冲最初从 512 字节增至 4096 字节并完成实测，随后
按用户要求提高到 7680 字节（7.5 KiB），接近 8 KiB。全速端点仍
为 64 字节最大包；7.5 KiB 是 15 个扇区 / 120 个包，位于 ESP32-S3
单次事务最多 127 包范围内，也是该约束下最大的整扇区缓冲。
一个 128 KiB 的 SCSI 读写命令分成 18 个数据传输。8192 字节需要
128 个包，不能直接使用；源码使用编译期检查保护该约束。
这是软件传输缓冲，不能与 USB 的 1 KB 硬件 FIFO 混为一谈。
约束依据：本机 ESP-IDF `soc/usb_dwc_cfg.h` 和
[TinyUSB DWC2 问题记录](https://github.com/hathach/tinyusb/issues/3825)。

PlatformIO 还显式添加 `include/module/usb` 到头文件搜索路径。
仅用 `CFG_TUSB_CONFIG_FILE` 宏选择头文件时，当前 SCons 增量构建
未跟踪它的修改，旧 MSC 对象仍使用 512 字节。搜索路径使头文件
扫描器能够解析 `tusb_option.h` 中的 `tusb_config.h` 引用；验证固件
时须检查 ELF 的 `_mscd_epbuf` 符号大小与配置一致（7680 字节为
`0x1e00`）；已实测的 4 KiB 固件为 `0x1000`。

## 回归检查

从 Receiver 目录运行：

```powershell
python test/usb_audio/run.py --compiler-bin '<MinGW bin>' --output "$env:TEMP/receiver-usb-msc-qa"
python test/recording/run.py --compiler-bin '<MinGW bin>' --output "$env:TEMP/receiver-msc-recording-qa"
python test/storage_sync/run.py --compiler-bin '<MinGW bin>' --output "$env:TEMP/receiver-msc-sync-qa"
pio run -e debug -e release
```

USB 检查编译实际 TinyUSB device/audio/MSC 栈、生产 MSC 回调、模式
控制和描述符，只替换物理控制器和介质。通过真实 CBW、数据阶段、
CSW 检查原始容量、最后 256 个扇区、128 KB 读写逐字节一致、同步
请求、跨扇区部分写入的相邻数据保护、越界和介质读错后的恢复、
安全弹出/装载，以及四种模式之间的 400 次切换。控制器替身检查
每个 bulk 事务的包数上限；原有 18 种 USB 音频格式检查继续执行。

录音检查编译生产 `tf.cpp`，将替身的物理空间与 FAT 数据空间明确
区分，检查物理扇区数、最后扇区可读及真实边界之外被拒绝；同时
执行所有录音/崩溃导出检查。配置同步检查验证 USB 介质交接及重挂载。

2026-09-18 上述三个桌面检查及已有 USB 音频 LVGL 界面检查均通过。
将旧版 `tf.cpp` 代入相同录音检查时，物理容量断言失败，确认该回归
检查能够捕获原有容量错误。第一轮容量修复的 debug/release 构建
均通过；添加头文件搜索路径后，最终 release 完整构建通过，ELF
中的 `_mscd_epbuf` 为 `0x1000`。最终 debug 构建遵照用户要求中止，
没有验证通过。此轮通过临时 Component Manager 约束保留项目原有
依赖版本，并禁止检查新版本；没有提交无关的依赖升级。

第一轮容量修复固件已烧录并通过 esptool 写入哈希校验。实机 Windows
报告整盘 16,107,700,224 字节，恰好覆盖原有分区末尾，文件系统为
FAT32，检查期间未新增磁盘错误。首次根目录读取约 8.55 秒，后续
目录访问约 0.1--0.2 毫秒（受 Windows 缓存影响）。对唯一新建的
256 KB 临时文件使用 Windows `NO_BUFFERING`，写入额外启用
`WRITE_THROUGH` 并刷盘，读回 SHA-256 一致后只删除该临时文件。
此时旧 MSC 缓冲实际仍为 512 字节；实测写入 136.1 KiB/s，读取
371.0 KiB/s，作为启用 4 KB 后的比较基线。

4 KB release 固件已烧录并通过 esptool 哈希校验。整盘容量
仍为 16,107,700,224 字节；同样的 256 KB 非缓存文件检查读回哈希
一致，写入 286.5 KiB/s、读取 528.0 KiB/s，分别约为比较基线的
2.1 倍和 1.4 倍。根目录和子目录可访问，35 KB 既有崩溃文件读取
约 68 毫秒；这些目录/文件计时可能受 Windows 缓存影响。检查期间
未新增该盘的 `disk` 事件。SCons 的 MSC 对象依赖记录已包含生产
`tusb_config.h` 和 `tusb_config_uac.h`，确认头文件修改可以被跟踪。

最后的 7.5 KiB 缓冲调整及对应检查替身遵照用户“直接提交，不用
测试”的要求没有重新编译或执行检查，也未烧录到设备。上述构建、
读写校验和测速结果仅适用于此前的 4 KiB 版本，不能作为 7.5 KiB
版本的验证结果；设备和 `build/firmware.bin` 当前仍为 4 KiB 版本。

用户仍反馈首次打开卡时资源管理器卡顿，并明确接受暂不修复；该
问题未解决，不能把容量与速度改善表述为资源管理器卡顿完全修复。
实机已观察调试/读卡器切换后主机只枚举当前设备；全部模式组合仍
只有桌面替身验证。不同 TF 卡、物理拔插、长时间大文件读写及负载
下 USB 任务栈余量未实测，主机替身不能证明这些硬件行为。
