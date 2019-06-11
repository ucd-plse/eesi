import argparse

parser = argparse.ArgumentParser()
parser.add_argument('bugs', help="Path to bugs file")
args = parser.parse_args()

def confidence(unchecked, checked):
	return float(checked) / (float(unchecked) + float(checked))

with open(args.bugs, "r") as f:
	lines = [x.strip().split() for x in f.readlines()]

lines.sort(key=lambda x: confidence(x[2], x[3]), reverse=True)

for l in lines:
	print(",".join(l))
