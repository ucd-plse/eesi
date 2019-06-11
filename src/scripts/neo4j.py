"""
Script for converting error propagation .dot to a csv
suitable for importing into Neo4J with LOAD CSV

fn1,spec1,fn2,spec2
malloc,==0,foo,<0
...

LOAD CSV WITH HEADERS FROM "file:///test.csv" as line
MERGE (u:Fn {name: line.fn1, spec: line.spec1})
MERGE (v:Fn {name: line.fn2, spec: line.spec2})
CREATE (u)-[:RET {spec: line.spec1}]->(v)

match (n:Fn {spec: "==0"}) set n:zero remove n:Fn
match (n:Fn {spec: "<0"}) set n:ltz remove n:Fn
match (n:Fn {spec: ">0"}) set n:gtz remove n:Fn
match (n:Fn {spec: "<=0"}) set n:lez remove n:Fn
match (n:Fn {spec: ">=0"}) set n:gez remove n:Fn
match (n:Fn {spec: "!=0"}) set n:nez remove n:Fn
match (n:Fn) detach delete n
"""

import networkx as nx
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--dot', help="Path to input dot file", required=True)
parser.add_argument('--csv', help="Path to output csv file", required=True)
args = parser.parse_args()

G = nx.drawing.nx_pydot.read_dot(args.dot)

csv = open(args.csv, "w")
csv.write("fn1,spec1,fn2,spec2\n")
for e in G.edges:
  fn1 = e[0]
  fn2 = e[1]
  spec1 = fn1.split()[1]
  spec2 = fn2.split()[1]
  fn1 = fn1.split()[0]
  fn2 = fn2.split()[0]
  fn1 = fn1.split("(")[0]
  fn2 = fn2.split("(")[0]
  csv.write("{},{},{},{}\n".format(fn1, spec1, fn2, spec2))
csv.close()
