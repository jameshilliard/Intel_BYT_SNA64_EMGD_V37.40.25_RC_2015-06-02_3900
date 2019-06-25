#!/bin/sh

if [ $# -lt 1 ];
then
	echo "Error: Script takes at least 1 parameter, the path to user_config.c to convert and optionally the path to the image_data.h. (If second parameter is omitted, it will assume it can use the image_data.h from the same directory where user_config.c is from)."
	return	
fi

if ! [ -f $1 ];
then
	echo "Error: File $1 does not exist."
	return	
fi

filename=$1
image_data=$2

path=${filename%/*}
path_to_img_data=$path/image_data.h
file=${filename##*/}
pref=${file%.*}
echo $pref

if [ $# == 2 ];
then
	if ! [ -f $2 ];
	then 
		echo "Error: File $2 does not exist."
		return
        fi
    	path_to_img_data=$2
fi

os=`uname -m`

if [ $os == "x86_64" ]
then
	bit=64
else 
	bit=32
fi

echo $path_to_img_data
\cp -f $1 ./user_config.c && \cp -f $path_to_img_data ./image_data.h && make && ./fw_tool -write_binary && \cp -f emgd_$bit.bin $pref.bin


