## 将cpufreq- tools移植到ODROID-XU3

**注意：编译会出现些被忽略的小错误，不影响使用**

#### The useage of cpufreq- tools (need root):

---

**Android系统的动态链接库（.so文件）都放在/system/lib目录下**  
**Android系统的可执行文件（elf文件）放在/system/bin目录下，将elf文件放在该目录下就可以在任一目录下运行了**

Host端：

```bash
# 编译
sudo make
# 将可执行文件导入到ODROID-XU3
adb push cpufreq-info /system/bin
adb push cpufreq-set /system/bin
adb push cpufreq-aperf /system/bin
# 将库文件导入到ODROID-XU3
adb push libcpufreq.so.0.0.0 /system/lib
```

Odroid-XU3端设置：

```bash
su
mount -o rw,remount /system
ln -s libcpufreq.so.0 libcpufreq.so.0.0.0
ln -s libcpufreq.so libcpufreq.so.0.0.0
mount -o ro,remount /system

```

---

###### cpufreq-info:

`cpufreq-info`

---

###### cpufreq-set :
-d  minimum frequency,
-u  maximum frequency,
-f  specific frequency (userspace governor must be set first) and
-g  governor on a
-c  specific CPU.

```
cpufreq-set  -g userspace
cpufreq-set -c 0 -f 1.2Ghz
cpufreq-set -f 1000Mhz
```

---


The cpufrequtils package (homepage: 
http://www.kernel.org/pub/linux/utils/kernel/cpufreq/cpufrequtils.html ) 
consists of the following elements:


libcpufreq
----------

"libcpufreq" is a library which offers a unified access method for userspace
tools and programs to the cpufreq core and drivers in the Linux kernel. This
allows for code reduction in userspace tools, a clean implementation of
the interaction to the cpufreq core, and support for both the sysfs and proc
interfaces [depending on configuration, see below].


utils
-----

"cpufreq-info" determines current cpufreq settings, and provides useful
debug information to users and bug-hunters.
"cpufreq-set" allows to set a specific frequency and/or new cpufreq policies
without having to type "/sys/devices/system/cpu/cpu0/cpufreq" all the time.


debug
-----

A few debug tools helpful for cpufreq have been merged into this package,
but as they are highly architecture specific they are not built by default.


compilation and installation
----------------------------

make
su
make install

should suffice on most systems. It builds default libcpufreq,
cpufreq-set and cpufreq-info files and installs them in /usr/lib and
/usr/bin, respectively. If you want to set up the paths differently and/or
want to configure the package to your specific needs, you need to open
"Makefile" with an editor of your choice and edit the block marked
CONFIGURATION.


THANKS
------
Many thanks to Mattia Dongili who wrote the autotoolization and
libtoolization, the manpages and the italian language file for cpufrequtils;
to Dave Jones for his feedback and his dump_psb tool; to Bruno Ducrot for his
powernow-k8-decode and intel_gsic tools as well as the french language file;
and to various others commenting on the previous (pre-)releases of 
cpufrequtils.


        Dominik Brodowski
