# REDSFS

## REally Dang Simple File System...

Writted out of frustration for the very useful but over complicated SPIFFS module. 

No frills, or caching or magic care taking. The code has been written to ensure concise, simple editing or debugging to enable a simple block
file system on a micro like the ESP8266/8285/32, being built by an x86 executable and rolled out in flash.


## Running the redsimg utility

Create file reds.img with size 2K and import a directory

`./redsimg -c 2048 -f reds.img -i import_dir/`

Export files from reds.img to directory

`./redsimg -f reds.img -e export_dir/`

Test read and write

`./redsimg -c 2048 -f reds.img -t`
