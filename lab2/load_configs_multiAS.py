import sys
import os
import subprocess
import time

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
		if line.startswith("ip route"):
			vtysh_command+=line

	cost_command=""
	while True:
		line = ospfd.readline()
		if line.startswith("router") or not line:
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
cmd = '''
vtysh -c \'
show ip ospf route
\'
'''
while True:
	time.sleep(1)
	res = subprocess.check_output(['./go_to.sh', "NEWY", '-c', cmd])
	count = 0
	for line in res.split('\n'):
		if (line != "" and line[0] == 'N'):
			count += 1
	if count == 22:
		break
adv_command = {}
for loc_name in os.listdir(sys.argv[1]):
	if loc_name=="EXAMPLE" or loc_name==".DS_Store":
		continue
	p = subprocess.Popen(["./go_to.sh",loc_name], stdin=subprocess.PIPE)
	bgpd = open(sys.argv[1]+"/"+loc_name+"/"+"bgpd.conf.sav",'r')
	vtysh_command = '''
vtysh -c \'
conf t
'''
	while True:
		line = bgpd.readline()
		if not line:
			break
		if line.startswith("router bgp"):
			vtysh_command+=line
			bgp_line = line
			line = bgpd.readline()
			if line.startswith(" bgp router-id"):
				vtysh_command+=line
			while True:
				line = bgpd.readline()
				if not line.startswith(" neighbor") and not line.startswith(" network"):
					break
				if line.startswith(" network"):
					adv_command[loc_name] = bgp_line
					adv_command[loc_name] += line
				else:
					vtysh_command+=line
	vtysh_command+="\'"
	bgpd.close()
	p.stdin.write(vtysh_command)
	p.stdin.close()
	returncode = p.wait()
cmd = '''
vtysh -c \'
show ip bgp summary
\'
'''
while True:
        time.sleep(1)
        res = subprocess.check_output(['./go_to.sh', "NEWY", '-c', cmd])
        flag = False
        for line in res.split('\n'):
		if line.find("Active")>0 or line.find("Idle")>0:
                	flag = True
			break
        if flag:
                continue
	else:
		break
for loc_name in adv_command.keys():
	p = subprocess.Popen(["./go_to.sh",loc_name], stdin=subprocess.PIPE)
	vtysh_command = '''
vtysh -c \'
conf t
'''
	vtysh_command+=adv_command[loc_name]
	vtysh_command+="\'"
	p.stdin.write(vtysh_command)
	p.stdin.close()
	returncode = p.wait()

