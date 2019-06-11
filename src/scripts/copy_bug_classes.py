import argparse

parser = argparse.ArgumentParser()
parser.add_argument('fromcsv', help="Path to classified bug csv")
parser.add_argument('tocsv', help="Path to new csv")
args = parser.parse_args()

fromcsv = open(args.fromcsv)
tocsv = open(args.tocsv)

from_lines = [x.strip() for x in fromcsv.readlines()]
to_lines = [x.strip() for x in tocsv.readlines()]

from_classes = dict()
for l in from_lines:
    bug_location = l.split(',')[0]
    from_classes[bug_location] = l

for l in to_lines:
    bug_location = l.split(',')[0]
    if bug_location in from_classes:
        keep = ",".join(l.split(',')[:5])
        copy = ",".join(from_classes[bug_location].split(',')[4:])
        print("{},{}".format(keep, copy))
    else:
        print(l)
