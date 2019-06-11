import networkx as nx
from networkx.drawing.nx_pydot import read_dot, write_dot
from networkx.algorithms.dag import descendants

G = nx.DiGraph(read_dot("/home/daniel/kernel-fullprop.dot"))
DAG = nx.DiGraph()

# Need to convert the graph to a DAG
components = nx.strongly_connected_components(G)
contraction = dict()
for c in components:
    component_iter = iter(c)
    contract_to = next(component_iter)
    contraction[contract_to] = contract_to
    while True:
        try:
            element = next(component_iter)
            assert element not in contraction
            contraction[element] = contract_to
        except StopIteration:
            break

    DAG.add_node(contract_to)

for (u, v) in G.edges:
    DAG.add_edge(contraction[u], contraction[v])

blacklist = {
        "vsnprintf(bottom)",
        "sprintf(bottom)",
        "snprintf(bottom)",
        "scnprintf(bottom)"
        }
nodes_to_remove = set()
for n in DAG.nodes:
    if n in blacklist:
        nodes_to_remove.add(n)
for n in nodes_to_remove:
    DAG.remove_node(n)

num_descendants = []
for n in DAG.nodes:
    if (DAG.in_degree(n) == 0):
        num_descendants.append((n, len(descendants(DAG, n))))
num_descendants.sort(key=lambda x: x[1], reverse=True)

covered_nodes_added = []
covered_nodes = set()
for (n, _) in num_descendants:
    d = descendants(DAG, n)
    prev_cover = len(covered_nodes)
    covered_nodes = covered_nodes.union(d)
    new_cover = len(covered_nodes)
    covered_nodes_added.append((n, new_cover - prev_cover))
covered_nodes_added.sort(key=lambda x: x[1])

num_ext4_descendants = []
for n in DAG.nodes:
    if (DAG.in_degree(n) == 0):
        desc = descendants(DAG, n)
        ext4_descendants = set()
        for d in desc:
            if "ext4" in d:
                ext4_descendants.add(d)
        num_ext4_descendants.append((n, len(ext4_descendants)))

num_ext4_descendants.sort(key=lambda x: x[1])
for n in num_ext4_descendants:
    print(n)

# Uncomment to print out the DAG
# write_dot(G, "dag.dot")