import argparse

parser = argparse.ArgumentParser()
parser.add_argument('file1', help="Path to first file")
parser.add_argument('file2', help="Path to second file")
args = parser.parse_args()

f1 = open(args.file1)
f2 = open(args.file2)

f1_lines = [x.strip() for x in f1.readlines()]
f2_lines = [x.strip() for x in f2.readlines()]

for api in set(f1_lines).intersection(f2_lines):
	print(api)
