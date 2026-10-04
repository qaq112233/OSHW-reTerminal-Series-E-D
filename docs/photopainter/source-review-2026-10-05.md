# V1再核查：官方示例、实际驱动与诊断3

## 参考与边界

2026-10-05通过 `git ls-remote origin HEAD` 确认：微雪官方最新提交仍为
`a5e8f757ba0cafbb5586f07d3e83bda3184c0845`，与本地
`references/ESP32-S3-PhotoPainter`相同。参考保持只读，无需重复下载。
本次对照主例程 `01_Example/xiaozhi-esp32`、Arduino音频示例
`05_ArduinoExample/01_Audio_Test`和低功耗示例
`04_PowerConsumptionTest/01_Arduino_Src/01_Fac_Test`。

当前实机证据仍是诊断2：PMIC、RTC均超时，SDA/SCL软件采样均0。
**没有新实机结果，尚未证明PMIC超时根因已修复。**

## 已确认并修正

1. PhotoPainter的 `CORE_DEBUG_LEVEL=debug` 无效：实际框架
   `cores/esp32/esp32-hal-log.h`采用数值条件编译，未定义的debug按0处理。
   用实际头文件预处理复现，旧配置的 `log_e/log_i` 均展开成 `do {} while(0)`；
   改为数值3（INFO）后正常展开为log_printf。只修改PhotoPainter环境及其继承
   的bring-up。不宣称所有预编译IDF日志都受此控制，也不把缺日志当成I2C故障根因。
2. `PrepareForDeepSleep()`原来在EXT1失败后仍返回true，RTC上拉/下拉返回值
   也未检查。现在任一步失败返回false，调用方取消本次深睡。仍先关ALDO3并回读。
   APP通用取消日志同时涵盖电源和唤醒配置，避免误报；没有板型特判。

## 驱动和GPIO证据

对诊断2本地ELF反汇编：`i2c_set_pin`确实调用
`gpio_set_direction(...,7)`（INPUT_OUTPUT_OD），设置上拉并连接GPIO矩阵；
`gpio_get_level`包含GPIO>=32的分支。未找到将47/48当低32位管脚的错误。
构建为ESP32-S3、16MB Flash及qio_opi；默认variant虽把48标作RGB LED，
当前启动路径未找到驱动它的调用。上述静态检查不证明实际电压或运行时状态。

新增板内 `pmic_gpio_diagnostics.h`，只用REG_READ保留三阶段快照：

- `before_setup`：驱动配置前。
- `after_setup`：驱动配置、安装及滤波成功后；失败则NOT_CAPTURED。
- `at_report`：报告时的当前状态，包含PMIC/RTC尝试之后的状态。

记录GPIO_IN1、OUTPUT/ENABLE、pad control，以及47/48的MUX、输入使能、上下拉、
开漏、输出信号和I2C输入矩阵。高位采样用bit15/16。快照不是原子采样，也不是
电压/波形测量，OUTPUT位不代表外设实际驱动电平。新增诊断不配置GPIO、不改变
pad电压、不写eFuse、不产生手动恢复时钟。标识为 `v1-20261005-3`。

配置成功后检查IE=1、PU=1、OD=1、FUNC=1；输入矩阵应选47/48，输出信号低9位
应为SDA90/SCL89。其他位及pad control保留原值，不能凭十六进制值擅自改电源。

## 微雪PMIC低功耗流程的遗漏边界

主例程 `components/pmicpower/power_bsp.cpp::Custom_PmicPortInit()`在XPowers
begin之后才将GPIO21/3设输入，没有PMIC访问前的IRQ脉冲。但低功耗示例
`01_Fac_Test.ino::setup()`确实先调用 `axp2101_irq_init()`：GPIO21拉低100ms，
拉高后等待200ms，再额外等待2秒后访问PMIC。其
`bsp_fac.h::axp_basic_sleep_start()`还设置IRQ唤醒、enableSleep，并关闭包括
ALDO3的若干输出；这是配套的PMIC睡眠/唤醒行为。

不能将“主例程没有脉冲”外推成“所有官方示例都没有”。当前适配没有启用这组
PMIC睡眠行为，板卡刷入前的寄存器状态未知，是否需要恢复睡眠状态仍待证据。
本轮未将GPIO21脉冲当作已证明的超时修复，也未把该脉冲加入启动路径。

## 检查与交付

- SDA47/SCL48、地址0x34、ID寄存器0x03及ID0x4A与官方一致；V1 ALDO3到
  EPD_VCC的连接不变。复用的六色索引与rotation 2静态对照通过。
- 刷新后POWER_OFF/BUSY再关ALDO3、网络等待前关电、深睡前回读及180秒故障
  保护入口保留；PMIC失联仍无法保证物理关电。
- 两个固件构建通过。已有第三方touch配置、日志宏重定义等警告仍存在，不宣称
  零警告。没有新增实机PMIC、显示、朝向或物理关电验收结果。
- 真实adapter对mock SDK的主机测试新增高位、冻结快照、矩阵地址和只读检查；
  PMIC启动、registry、sleep schedule、AXP状态、六色及rail入口检查通过；
  烧录包8项测试通过。GPIO/PMIC测试不模拟电气行为。
- 新包目录 `build/photopainter-v1-2026-10-05-pmic-diagnostic-3`；下一次先刷
  bring-up并提供GPIO三阶段和PMIC/RTC完整日志，重复故障后断开全部供电。
