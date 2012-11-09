#Take all the files of pattern refdist_*_*.dat, sum there rows and write them to standard output

refdist = {0:0}

import glob;
for file in glob.glob('refdist_*_*.dat'):
#    print(file)
    f = open(file);
    for line in f:
        line = line.rstrip().lstrip();
        if line.startswith("#") or len(line) == 0:
            continue;
#        print line;
        parts = line.split(" ");
        if( len(parts) != 2 ):
            print len(parts);
            print line;
        assert( len(parts) == 2 );
        key = int(parts[0]);
        if key not in refdist:
            refdist[key] = 0
        refdist[key] += int(parts[1]);

for k in sorted(refdist.keys()):
    v = refdist[k];
    print(str(k) + " " + str(v));
