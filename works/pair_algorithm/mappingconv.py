#!/usr/bin/env python3
import sys
import re

xdim = 4
ydim = 4
zdim = 4

def read_mapping(input_filename):
    """
    Reads a mapping file with columns:
      taskno    X    Y    Z
    Returns a list of tuples: (taskno, x, y, z)
    """
    tasks = []
    with open(input_filename, 'r') as fin:
        header = fin.readline()  # Skip header line
        for line in fin:
            line = line.strip()
            if not line:
                continue
            # Split by whitespace (handles spaces and tabs)
            parts = line.split()
            if len(parts) < 4:
                continue
            taskno = parts[0]
            try:
                x = int(parts[1])
                y = int(parts[2])
                z = int(parts[3])
            except ValueError:
                continue
            tasks.append((taskno, x, y, z))
    return tasks

def sort_key(task):
    """
    Given a task tuple (taskno, x, y, z), split taskno by the dot
    and return a tuple (graph_number, task_number) to sort on.
    
    For example, for "1.10", this returns (1, 10).
    """
    taskno, _, _, _ = task
    try:
        major, minor = taskno.split('.')
        return (int(major), int(minor))
    except Exception:
        return (0, 0)

def combine_mapping(tasks):
    """
    Sort tasks by their parsed taskno (graph and task) and then reassign
    a new sequential task index (starting at 0) while preserving the original coordinates.
    
    Returns a list of tuples: (new_index, x, y, z)
    """
    tasks_sorted = sorted(tasks, key=sort_key)
    combined = []
    for new_id, (taskno, x, y, z) in enumerate(tasks_sorted):
        combined.append((new_id, x, y, z))
    return combined

def write_mapping(mapping, output_filename):
    """
    Writes the combined mapping to the output file in the format:
      task tileno
      0 tileno
      1 tileno
      ...
    where tileno is computed as x * y * z.
    """
    with open(output_filename, 'w') as fout:
        fout.write("#task tileno\n")
        for new_task, x, y, z in mapping:
            tileno = x + (y*xdim)+ (ydim*xdim*z)
            fout.write(f"{new_task} {tileno}\n")


def parse_dot(dot_text):
    # REGEX patterns:
    # Node definition: e.g.  "t0_0" [label="t0_0 (18)"];
    node_pattern = re.compile(r'("t\d+_\d+")\s+\[label="([^"]+)"\];')
    # Edge definition: e.g. "t0_0" -> "t0_1" [label="(2)"];
    edge_pattern = re.compile(r'("t\d+_\d+")\s*->\s*("t\d+_\d+")\s+\[label="\((\d+)\)"\];')
    
    nodes = {}
    # We use a set to avoid duplicates
    for m in node_pattern.finditer(dot_text):
        node_name_quoted, label = m.groups()
        # remove the quotes from the node name
        node_name = node_name_quoted.strip('"')
        nodes[node_name] = label  # you can store the full label if needed later
        
    # Parse edges
    edges = []
    for m in edge_pattern.finditer(dot_text):
        src_quoted, tgt_quoted, weight = m.groups()
        src = src_quoted.strip('"')
        tgt = tgt_quoted.strip('"')
        weight = int(weight)
        edges.append((src, tgt, weight))
        
    return nodes, edges

def node_sort_key(node_name):
    # Node names are in the form tX_Y, where X and Y are integers.
    # We extract those so that we can sort first by graph id then by task id.
    match = re.match(r't(\d+)_(\d+)', node_name)
    if match:
        graph_id, task_id = match.groups()
        return (int(graph_id), int(task_id))
    return (0, 0)

def write_app_file(nodes, edges, output_filename):
    # Create a mapping from original node name to global index
    sorted_nodes = sorted(nodes.keys(), key=node_sort_key)
    mapping = { node_name: idx for idx, node_name in enumerate(sorted_nodes) }
    ntasks = len(mapping)
    
    with open(output_filename, 'w') as fout:
        # Write the header
        fout.write("# Video Object Plane Decoder (VOPD) application\n")
        fout.write("#[ntasks]\n")
        fout.write(f"{ntasks}\n\n")
        fout.write("#[graph]\n")
        
        # Write the edges with remapped node indices.
        for src, tgt, weight in edges:
            # Only output edge if both nodes exist in our mapping
            if src in mapping and tgt in mapping:
                src_id = mapping[src]
                tgt_id = mapping[tgt]
                fout.write(f"{src_id} {tgt_id} {weight}\n")

def main():
    # if len(sys.argv) != 3:
    #     print("Usage: python convert_tasknos.py input_mapping.txt output_mapping.txt")
    #     sys.exit(1)
    input_filename = "./output/example.dot"
    output_filename = "./mapping/task.app"
    
    with open(input_filename, 'r') as fin:
        dot_text = fin.read()
    
    nodes, edges = parse_dot(dot_text)
    write_app_file(nodes, edges, output_filename)
    print(f"Conversion complet3ed. APP file saved as '{output_filename}'.")
    
    input_filename = "./mapping/mapFile.txt"
    output_filename = "./mapping/taskToTile.txt"

    tasks = read_mapping(input_filename)
    combined_mapping = combine_mapping(tasks)
    write_mapping(combined_mapping, output_filename)
    print(f"Combined mapping created in '{output_filename}'.")

if __name__ == "__main__":
    main()
