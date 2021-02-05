# Destroy existing namespace
ndctl destroy-namespace -f all

# Create goal to create a region in App Direct mode (interval by default)
ipmctl create -f -goal persistentmemorytype=appdirect # or ipmctl create -goal persistentmemorytype=appdirectnotinterleaved

# Show goal
ipmctl show -goal

# Reboot then create namespace
ndctl create-namespace 

# Check out the pmem device created
ls -l /dev/pmem*

# Create filesystem and mount it on the pmem device
# mkfs.ext4 -f /dev/pmem<x>

# Create a directory
# mkdir /mnt/mypmem

# Mount filesystem with -o dax option
# mount -o dax /dev/pmem<x> /mnt/mypmem
