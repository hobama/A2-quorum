"""Simulation Results filtering

This modules filters through long files of simulation data produced by COOJA
and counts the number of Successful read and write rounds

__author__ = "Konstantinos Peratinos" """

import re
## The number of Nodes in the simulation -- Change before running
nc=12


## Regex expression to select all fractions
regex = re.compile(r"(\d+/\d+)")

for l in range(1,2):
	# Multiple sim logs with 1 execution
	fn = 'log_sim12.'+str(l)+'.txt'
	print "----------------------------------------------"
	print fn

	## Different counters for read/write
	r_fail=0
	w_fail =0
	w_succ = 0
	r_succ = 0

	## Counter for sucess round/offslot and lines
	complete =0
	offslot = 0
	line_count =0

	with open(fn) as f:
		i = 1
		for z,line in enumerate(f):
			temp =line.split()
			fraction = temp[8].replace(',','').split('/')
			complete += int(fraction[0])
			offslot += int(fraction[1])
			if i == 1 or i == 13:
				curr = temp[4]
				i = 1
			elif curr == temp[4]:
				if i == 12:
					if (z/nc)%2 == 0:
						r_succ = r_succ+1
					else:
						w_succ = w_succ+1
			else :
				if (z/nc)%2 == 0:
					r_fail = r_fail+1
				else:
					w_fail = w_fail+1
				## Skip next lines, since 1 fail is ennough
				for k in range(nc -i) :
					next(f)
				i = 0
			i = i+1
		line_count=z+1

	print 'Successful Writes:' + str(r_succ) + ' Successful Reads:' + str(w_succ)
	print 'Failed Writes:' + str(r_fail) + ' Failed Reads:' + str(w_fail)
	print 'Avg Complete:' + str(complete/float(line_count)) + ' Avg Offslot:' + str(offslot/float(line_count))
	print 'Compl/offslot Ratio:' + str(complete/float(offslot))
