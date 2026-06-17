#!/system/bin/sh
MODDIR=${0%/*}

# 赋予执行权限并以 Root 身份在后台静默启动我们的守护进程
chmod +x $MODDIR/system/bin/rfcore_daemon
nohup $MODDIR/system/bin/rfcore_daemon > /dev/null 2>&1 &
