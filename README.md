# C-timecop

## DESCRIPTION

A C implementation providing "time travel" and "time freezing" capabilities, inspired by [php-timecop.](https://raw.github.com/hnw/php-timecop).

## INSTALL
```
make 
LD_PRELOAD=`pwd`/timecop.so TC_FREEZE_TIME=1000000000 enable_time=true no_clock_gettime=true php test-php.php 
```

