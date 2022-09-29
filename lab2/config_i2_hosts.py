import sys
import os
import subprocess

loc_name_list = ["NEWY","CHIC","WASH","ATLA","KANS","HOUS","SALT","LOSA","SEAT"]
itf_name_list = ["newy","chic","wash","atla","kans","hous","salt","losa","seat"]
cnt = 1
for loc_name in loc_name_list: 
	p = subprocess.Popen(["./go_to.sh",loc_name+"-host"], stdin=subprocess.PIPE)
	p.stdin.write("ifconfig "+itf_name_list[cnt-1]+" 4.10"+str(cnt)+".0.1/24 up\n") 
	p.stdin.write("route add default gw 4.10"+str(cnt)+".0.2 "+itf_name_list[cnt-1]+"\n")
	p.stdin.close()
	returncode = p.wait()
	cnt+=1
