"""Simulation Radio stats filtering

This modules filters through long files of simulation radio data produced by COOJA
and calculate average, Std deviation and other Data

__author__ = "Konstantinos Peratinos" """

from math import sqrt
## The number of Nodes in the simulation -- Change before running
nc=40

for l in range(1,6):
	# Multiple sim logs with 1 execution
	fn = 'Rad_sim'+str(nc)+str(l)+'.txt'#+"_new.txt"
	#fn ='test3.txt'
	print "----------------------------------------------"
	print fn

	## Different counters for TX/RX/dC
	s_tx = 0
	s_rx = 0
	s_dc = 0
	interval = 0

	## For Std dev
	dev_tx = 0
	dev_rx = 0
	dev_dc = 0

	with open(fn) as f:
		# Calculating means
		for z,line in enumerate(f):
				temp =line.split()
				interval = int(temp[4])
				s_tx += int(temp[6])
				s_rx += int(temp[9])
				s_dc += int(temp[12])
		# Line count
		lc = float(z+1)
	with open(fn) as k:
		#again for Std dev
		for k,line2 in enumerate(k):
			temp =line2.split()
			dev_tx += pow(int(temp[6]) - s_tx/lc, 2)
			dev_rx += pow(int(temp[9]) - s_rx/lc, 2)
			dev_dc += pow(int(temp[12]) - s_dc/lc, 2)

		print "2nd line Tx, 3rd RX, 4th Duty cycle 5th Interval"
		print str(s_tx/lc) + " " + str(sqrt(dev_tx/lc))
		print str(s_rx/lc)  + " " + str(sqrt(dev_rx/lc))
		print str(s_dc/lc) + " " + str(sqrt(dev_dc/lc))
		print interval
