# ipmctl create -goal
# ipmctl create -goal persistentmemorytype=appdirectnotinterleaved
ndctl create-namespace
# ls -l /dev/pmem*



# 1.	Create goal to create a region in App Direct mode on reboot
#         a.	ipmctl create –goal PersistentMemoryType=AppDirect
# 2.	Show goal
#         a.	ipmctl show -goal
# 3.	Reboot
#         a.	ndctl create-namespace -r region2
# 4.	Create namespace using ndctl. Default mode is fsdax.
#         ndctl create-namespace 
#         {
#         "dev":"namespace2.0",
#         "mode":"fsdax",
#         "map":"dev",
#         "size":"2964.94 GiB (3183.58 GB)",
#         "uuid":"41d252c8-55b3-4683-989a-d131e8136870",
#         "sector_size":512,
#         "align":2097152,
#         "blockdev":"pmem2"
#         }
# 5.	Check out the pmem device created
#         a.	ls -l /dev/pmem*
# 6.	Create filesystem and mount it on the pmem device
#         a.	mkfs.ext4 -f /dev/pmem<x>
# 7.	Create a directory
#         a.	mkdir /mnt/mypmem
# 8.	Mount filesystem with -o dax option
#         a.	mount -o dax /dev/pmem<x> /mnt/mypmem
# 9.	Create a file on the file system and memory map it