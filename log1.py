with open("log1.txt") as f:
	for i, line in enumerate(f):
		if i == 5:
			print line
			break
			