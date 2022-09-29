import sys
import os
import subprocess

for loc_name in os.listdir(sys.argv[1]):
	if loc_name=="EXAMPLE" or loc_name==".DS_Store":
		continue
	p = subprocess.Popen(["./go_to.sh",loc_name], stdin=subprocess.PIPE)
	zebra = open(sys.argv[1]+"/"+loc_name+"/"+"zebra.conf.sav",'r')
	ospfd = open(sys.argv[1]+"/"+loc_name+"/"+"ospfd.conf.sav",'r')
	vtysh_command = '''
vtysh -c \'
conf t
'''
	while True:
		line = zebra.readline()
		if not line:
                        break
		if line.startswith("interface"):
			set_interface = line
			set_ip = zebra.readline()
			if "lo" in line and "los" not in line:
				continue
			vtysh_command+=set_interface
			vtysh_command+=set_ip
	cost_command=""
	while True:
                line = ospfd.readline()
                if line.startswith("router"):
			break
                if line.startswith("interface"):
                        set_interface = line
                        set_cost = ospfd.readline()
			if ("lo" in line and "los" not in line) or ("host" in line):
                        	continue
                        cost_command+=set_interface
                        cost_command+=set_cost
	vtysh_command+="router ospf\n"
	while True:
                line = ospfd.readline()
	        if not line:
                        break
	        if line.startswith(" network"):
			set_interface = line
                        vtysh_command+=set_interface
	vtysh_command+=cost_command
	vtysh_command+="\'"
#	print(vtysh_command)
	zebra.close()
	ospfd.close()
        p.stdin.write(vtysh_command)
        p.stdin.close()
        returncode = p.wait() 
