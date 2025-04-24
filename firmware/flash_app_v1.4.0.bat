esptool ^
--port COM7 ^
--chip esp32 ^
--baud 921600 ^
write_flash ^
--flash_mode dio ^
--flash_freq 40m ^
--flash_size detect ^
0x1000 bin/v1.4.0/bootloader.bin ^
0x8000 bin/v1.4.0/partition-table.bin ^
0x10000 bin/v1.4.0/net_tester_esp32.bin
