#python3 /home/balmer/radio/KendryteK210/tool/ktool/ktool.py --sram build/base.bin --terminal --baudrate 2000000 --port /dev/ttyUSB0
python3 /home/balmer/radio/KendryteK210/tool/ktool/ktool.py --sram build/base.bin --baudrate 2000000
python3 ../py/balmer_term.py --baudrate 2000000
