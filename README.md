# koku-recover-images

**only works on a 64-bit linux system**

this tool tries to recover **unfragmented** JPEGs, PNGs, TIFFs, GIFs, WEBPs from a binaryfile.\
if possible it verifies the validity of the file:

    * required data is present
    * data has correct values if the image format has something i can check..
    * comparing CRCs if the image format has them..

## installation

not configurated yet..

## build

```bash
mkdir build
cd build
cmake --config Release ..
make
```

## usage

create a folder on a place with lots of freespace, in this folder execute:

`koku-recover-images path_to_disk_image`

## history

somebody had a broken external mechanical hardisk 2TiB,\
i was still able to create a disk-image ~8GiB were unreadable.

[photorec](https://github.com/cgsecurity/testdisk) was running very slowly..

i was able to recover ~525450 JPEGs very quickly with [recoverjpeg](https://github.com/samueltardieu/recoverjpeg)\
(i am not sure why this number was so high..)

inspired by this, i wrote this program to learn a little bit about image formats and to be able to recover more than JPEGs.\
i could restore ~277043 images (~600GiB)

(
    all numbers above include thumbnails, there were ~3 images for each single image
)

i also let [photorec](https://github.com/cgsecurity/testdisk) run for ~1 month, it restored some files i couldn't restore with [recoverjpeg](https://github.com/samueltardieu/recoverjpeg) or this program.

