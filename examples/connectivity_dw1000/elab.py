import sys
import re
import numpy
from matplotlib import pyplot

if len(sys.argv) < 2:
    print(f"Usage {sys.argv[0]} inputFile")
    exit(1)

receivedPatt = re.compile('received \[.*-.*\] (\d*) (\d*) (\d*) (\d*) (\d*)')

data = []

with open(sys.argv[1], 'r') as fin:
    for l in fin:
        m = receivedPatt.search(l)

        if m is None:
            continue

        vals = list(map(int, (m.group(1), m.group(2), m.group(3), m.group(4), m.group(5))))
        
        data.append(vals)

#print(data)

data = numpy.array(data).transpose()

f1 = data[0]
f2 = data[1]
f3 = data[2]
c = data[3]
n = data[4]

A = {'16MHz' : 113.77, '64MHz' : 121.74}
prf = '16MHz'

fpl = 10 * numpy.log10((f1**2 + f2**2 + f3**2) / n**2) - A[prf]
rxl = 10 * numpy.log10(c * 2**17 / n**2) - A[prf]

print(fpl)
print(rxl)
