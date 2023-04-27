#!/bin/bash
#
dateformat=`date -d last-day +%y-%m-%d`
filevar="/home/SS_daq/N1470_control_V1.0/log_dir/HVlog_20"${dateformat}",*.txt"
output="/home/SS_daq/N1470_control_V1.0/log_dir/HVlog_20"${dateformat}".tar.gz"
tar -zcvf ${output} ${filevar} 1>/dev/null 2>&1
rm ${filevar}
chmod 777 ${output}




