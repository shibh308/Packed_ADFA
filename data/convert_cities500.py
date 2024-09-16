import sys
filepath = sys.argv[1]
with open(filepath, 'r') as f:
    for l in f.readlines():
        sp = l.split('\t')
        asciiname = sp[2]
        print(asciiname)