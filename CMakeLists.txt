#############################################################[Chinese Manual]###################################################################
# *****工程路径不要包含中文字符和空格,否则可能会报错！*****
# 第一步： 确认机器上已经安装了MRS的工具链(假设当前平台为Linux,工具链路径为"/path/to/toolchain/RISC-V Embedded GCC12",工具链相关前缀为"riscv-wch-elf-")
# 第二步: 设置工具链路径,例如修改替换下方的set(TOOLCHAIN_FOLDER "/path/to/toolchain/RISC-V Embedded GCC12")
# 第三步: 设置工具链中相关可执行文件的前缀,例如修改替换下方的set(TOOLCHAIN_PREFIX "riscv-wch-elf-")
# 第四步: 执行 cd your/path/of/project
# 第五步: 执行 cmake -B build -G "Unix Makefiles"
# 第六步: 执行 cmake --build build
# 注意: 在Windows平台运行,需要a.安装GNU MAKE并将其路径(bin目录)添加到系统环境变量,之后执行cmake -B build -G "Unix Makefiles"(推荐)
#       或者b.安装MinGW,将其路径(bin目录)添加到系统环境变量,之后执行cmake -B build -G "MinGW Makefiles"
# 如有相关疑问,可通过support@mounriver.com联系我们
#################################################################################################################################################


#############################################################[English Manual]###################################################################
# Step 1: Confirm that the MRS toolchain has been installed on the machine. Assuming the current platform is Linux, 
#         the toolchain path is "/path/to/toolchain/RISC-V Embedded GCC12", and the toolchain-related prefix is "riscv-wch-elf-"
# Step 2: Set the toolchain path, for example, modify and replace the following "set (TOOLCHAIN_FOLDER "/path/to/toolchain/RISC-V Embedded GCC12")"
# Step 3: Set the prefix of the executable files in the toolchain,for example, modify and replace the following "set(TOOLCHAIN_PREFIX "riscv-wch-elf-")"
# Step 4: Call "cd your/path/of/project"
# Step 5: Call "cmake -B build -G "Unix Makefiles""
# Step 6: Call "cmake --build build"
# Note:   To run on Windows, you need: a. Install GNU MAKE and add its path (the bin directory) to the system environment PATH. 
#         Then call the execution of "cmake -B build -G "Unix Makefiles""(Recommanded).
#         or b. Install MinGW and add its path (the bin directory) to the system environment PATH. 
#         Then call the execution of "cmake -B build -G "MinGW Makefiles"".
# If you have any questions, please contact us at support@mounriver.com
#################################################################################################################################################


# Set CMake minimum require
cmake_minimum_required(VERSION 3.16)

# Set CMake sysytem name
set(CMAKE_SYSTEM_NAME Generic)


# Skipping compiler detection and use custom toolchain
SET(CMAKE_C_COMPILER_WORKS TRUE)
SET(CMAKE_CXX_COMPILER_WORKS TRUE)
SET(CMAKE_ASM_COMPILER_WORKS TRUE)

# Use MRS toolchian to compile
set(TOOLCHAIN_FOLDER "d:/mounriver-studio/runtime/MounRiver_Studio2/resources/app/resources/win32/components/WCH/Toolchain/RISC-V Embedded GCC15")
set(TOOLCHAIN_PREFIX "riscv32-wch-elf-")
set(CMAKE_C_COMPILER "${TOOLCHAIN_FOLDER}/bin/riscv32-wch-elf-gcc.exe")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_FOLDER}/bin/riscv32-wch-elf-g++.exe")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_FOLDER}/bin/riscv32-wch-elf-gcc.exe")
set(CMAKE_OBJCOPY "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}objcopy.exe")
set(CMAKE_OBJDUMP "${TOOLCHAIN_FOLDER}/bin/riscv32-wch-elf-objdump.exe")
set(CMAKE_SIZE "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}size.exe")
set(CMAKE_AR "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}ar.exe")

set(PROJECT_NAME "SimulateCDC")
set(TARGET_ARTIFACT "SimulateCDC.elf")

set(CMAKE_C_OUTPUT_EXTENSION .o)
set(CMAKE_CXX_OUTPUT_EXTENSION .o)

project("${PROJECT_NAME}" LANGUAGES C ASM CXX)

# Use relative path
add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=..)


set(SRC_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/User/CherryUSB/core/usbd_core.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/User/CherryUSB/port/ch32/ch32fs/usb_dc_usbfs.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/User/USB/usb_msc.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/User/USB/usb_dc_low_level_init.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/User/ch32l103_it.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/User/main.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/User/system_ch32l103.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Startup/startup_ch32l103.S"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_adc.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_bkp.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_can.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_crc.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_dbgmcu.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_dma.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_exti.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_flash.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_gpio.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_i2c.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_iwdg.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_lptim.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_misc.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_opa.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_pwr.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_rcc.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_rtc.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_spi.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_tim.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_usart.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32l103_wwdg.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Debug/debug.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Core/core_riscv.c"
)

# CH32L103 只有 USBFS，端口固定为 ch32fs
add_compile_definitions(CONFIG_CH32_USBFS)

# CherryUSB 设备侧类驱动：通配符收集，后续新增类文件无需改 CMake（CONFIGURE_DEPENDS 自动重配置）。
# 未引用的类经 --gc-sections 链接时剔除，不进固件。
# usbh_* 为主机侧类，依赖完整 host 协议栈，CH32L103 USBFS 仅 device 模式，不编译。
file(GLOB_RECURSE CHERRYUSB_CLASS_SRC CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/User/CherryUSB/class/*.c")
list(FILTER CHERRYUSB_CLASS_SRC EXCLUDE REGEX "usbh_")
list(APPEND SRC_FILES ${CHERRYUSB_CLASS_SRC})
set(CMAKE_C_FLAGS " -march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -I\"${CMAKE_CURRENT_SOURCE_DIR}/Debug/\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/Core/\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/User\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/inc\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/User/CherryUSB\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/User/CherryUSB/common\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/User/CherryUSB/core\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/User/CherryUSB/class\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/User/CherryUSB/class/cdc\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/User/CherryUSB/class/hid\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/User/CherryUSB/class/msc\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/User/CherryUSB/port/ch32/ch32fs\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/User/USB\" -std=gnu99")
set(CMAKE_CXX_FLAGS " -march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -std=gnu++11 -fabi-version=0")
set(CMAKE_ASM_FLAGS " -march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -I\"${CMAKE_CURRENT_SOURCE_DIR}/Startup/\" -x assembler-with-cpp")

add_executable("${TARGET_ARTIFACT}" ${SRC_FILES})

set_target_properties("${TARGET_ARTIFACT}" PROPERTIES LINK_FLAGS " -march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -T \"${CMAKE_CURRENT_SOURCE_DIR}/Ld/Link.ld\"   -nostartfiles -Xlinker --gc-sections -Xlinker --print-memory-usage  -Wl,-Map,\"SimulateCDC.map\" --specs=nano.specs --specs=nosys.specs -o \"SimulateCDC.elf\" ")

target_link_libraries("${TARGET_ARTIFACT}" )

add_custom_command(TARGET "${TARGET_ARTIFACT}"
        POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} ARGS -O ihex  "SimulateCDC.elf" "SimulateCDC.hex"
        COMMAND ${CMAKE_OBJDUMP} ARGS  --all-headers --demangle --disassemble -M xw "SimulateCDC.elf" > "SimulateCDC.lst"
        COMMAND ${CMAKE_SIZE} ARGS  --format=berkeley "SimulateCDC.elf"
)

