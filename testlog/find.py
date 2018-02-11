"""Simulation Results error correction

This modules filters through long files of simulation data produced by COOJA
and searches for missing lines which could corrupt data of data scripts

__author__ = "Konstantinos Peratinos" """

from sets import Set
elem =  set()
tempzset = set()
## The number of Nodes in the simulation -- Change before running
nc=40

for i in range (1,nc+1) :
	elem.add(str(i))
for l in range(1,7):
	# Multiple sim logs with 1 execution
	fn = 'log_sim'+str(nc)+str(l)+'.txt'#+"_new.txt"
	#fn ='test3.txt'
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
	not_joined = 0
	z = 0
	with open (fn +"_new.txt",'w') as newf:
		with open(fn) as f:
			i = 1
			for z,line in enumerate(f):
					temp =line.split()
					Split_id = temp[0].split(':')
					id = Split_id[1]
					#complete += int(fraction[0])
					#offslot += int(fraction[1])
					if i == 1 or i == nc+1 :
						curr = temp[1]
						tempzset = set(elem)
						tempzset.remove(id)
						i = 1
					# CoRner Case if First message is from a node that did not join
					elif curr == temp[1]:
						tempzset.remove(id)
						if i == nc:
							if (z/nc)%2 == 0:
								r_succ = r_succ+1
								tempzset.clear()
							else:
								w_succ = w_succ+1
								tempzset.clear()
					else :
						tempo= buffer.split(':')
						if len(tempzset) == 1:
							tempo[1] = tempzset.pop()
							tempzset.clear()
							print  str(z) + " miss " + tempo[1]
							print ":".join(tempo)
							newf.write(":".join(tempo))
							curr = temp[1]
							tempzset.clear()
							tempzset = set(elem)
							i = 1
							tempzset.remove(id)
						else :
							print "damn" + str(z) + " curr" + curr + " temp[1]" + temp[1]
							print tempzset
						if (z/nc)%2 == 0:
							r_fail = r_fail+1
						else:
							w_fail = w_fail+1
						## Skip next lines, since 1 fail is ennough
						'''for k in range(nc -i) :
							try:
								next(f)
								z += 1
							except StopIteration :
								break;'''
						#i = 0
					i = i+1
					newf.write(line)
					buffer = line
			'''	else :
					not_joined += 1
					# Corner Cases When last message is from a node that did not join
					if i == nc and (z/nc)%2 == 0 :
						r_succ = r_succ+1
					elif i == nc :
						w_succ = w_succ+1
					elif i == 0:
						i = -2'''

			line_count=z+1 - not_joined

		print 'Successful Writes:' + str(r_succ) + ' Successful Reads:' + str(w_succ)
		print 'Failed Writes:' + str(r_fail) + ' Failed Reads:' + str(w_fail)
		#print 'Avg Complete:' + str(complete/float(line_count)) + ' Avg Offslot:' + str(offslot/float(line_count))
		#print 'Compl/offslot Ratio:' + str(complete/float(offslot)) +' Not joined:'+ str(not_joined)
