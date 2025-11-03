import dwave_networkx as dnx
import networkx as nx
import matplotlib.pyplot as plt

# Generate a Pegasus graph with size parameter m=4
m = 3
P4 = dnx.pegasus_graph(m)

# Print information about the graph
print(f"Graph P{m}: {P4}")
print(f"Number of nodes: {P4.number_of_nodes()}")
print(f"Number of edges: {P4.number_of_edges()}")

# Visualize the graph
nx.draw(P4, with_labels=False, node_size=10)
plt.title(f"Pegasus Graph (m={m})")
plt.show()
