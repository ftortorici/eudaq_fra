#!/bin/sh
#git restore --source linux2osx /Users/myaccount/eudaq/eudaq_rino/user/fers/hardware/include/MultiPlatform.h
#git restore --source linux2osx /Users/myaccount/eudaq/eudaq_rino/user/fers/hardware/include/FERS_LLusb.h

linuxusb='/usr/lib/x86_64-linux-gnu/libusb-1.0.so'
osxusb='/usr/local/Cellar/libusb/1.0.26/lib/libusb-1.0.dylib'

comando='%s+'$linuxusb'+'$osxusb'+'
#echo $comando

vim -c $comando -c ':wq' /Users/myaccount/eudaq/eudaq_rino/build/user/fers/module/CMakeFiles/module_fers.dir/build.make
vim -c $comando -c ':wq' /Users/myaccount/eudaq/eudaq_rino/build/user/fers/module/CMakeFiles/module_fers.dir/link.txt

#vim /Users/myaccount/eudaq/eudaq_rino/build/user/fers/hardware/CMakeFiles/static_fers.dir/build.make
#vim /Users/myaccount/eudaq/eudaq_rino/build/user/fers/hardware/CMakeFiles/static_fers.dir/link.txt
