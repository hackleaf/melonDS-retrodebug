set (CMAKE_CXX_STANDARD 17)

include(FixInterfaceIncludes)

add_library(core STATIC
    ARCodeFile.cpp
    ARDatabaseDAT.cpp
    AREngine.cpp
    ARM.cpp
    ARM_InstrTable.h
    ARMInterpreter.cpp
    ARMInterpreter_ALU.cpp
    ARMInterpreter_Branch.cpp
    ARMInterpreter_LoadStore.cpp
    CP15.cpp
    CRC32.cpp
    DMA.cpp
    DMA_Timings.h
    DMA_Timings.cpp
    DSi.cpp
    DSi_AES.cpp
    DSi_Camera.cpp
    DSi_DSP.cpp
    DSi_I2C.cpp
    DSi_I2S.cpp
    DSi_NAND.cpp
    DSi_NDMA.cpp
    DSi_NWifi.cpp
    DSi_SD.cpp
    DSi_SPI_TSC.cpp
    FATIO.cpp
    FATStorage.cpp
    FIFO.h
    GBACart.cpp
    GBACartMotionPak.cpp
    GPU.cpp
    GPU_Soft.cpp
    GPU2D.cpp
    GPU2D_Soft.cpp
    GPU3D.cpp
    GPU3D_Soft.cpp
    GPU3D_Texcache.cpp
    GPU3D_Texcache.h
    Mic.cpp
    NDS.cpp
    NDSCart.cpp
    Platform.h
    ROMList.h
    ROMList.cpp
    FreeBIOS.h
    FreeBIOS.cpp
    RTC.cpp
    Savestate.cpp
    SPI.cpp
    SPI_Firmware.cpp
    SPU.cpp
    types.h
    Utils.cpp
    Utils.h
    Wifi.cpp
    WifiAP.cpp

    DSP_HLE/UcodeBase.cpp
    DSP_HLE/AACUcode.cpp
    DSP_HLE/G711Ucode.cpp
    DSP_HLE/GraphicsUcode.cpp

    NDSCart/CartCommon.cpp
    NDSCart/CartRetail.cpp
    NDSCart/CartRetailNAND.cpp
    NDSCart/CartRetailIR.cpp
    NDSCart/CartRetailBT.cpp
    NDSCart/CartSD.cpp
    NDSCart/CartHomebrew.cpp
    NDSCart/CartR4.cpp

    fatfs/ff.c
    fatfs/ffsystem.c
    fatfs/ffunicode.c
    fatfs/ffconf.h

    sha1/sha1.c
    tiny-AES-c/aes.c
    xxhash/xxhash.c

    blip-buf/blip_buf.c

    retrodebug/RetroDebug.cpp)
