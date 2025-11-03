#!/usr/bin/env python3
import re
import sys
import subprocess
import os
import json
import xml.etree.ElementTree as ET

OUTPUT_FOLDER = "output"

def parse_vcg(vcg_filename):
    """
    Parse the VCG file to extract nodes and edges along with their labels.
    Returns a dictionary of nodes and a list of edges.
    Each node is stored as { title: label }.
    Each edge is a tuple: (source, target, label).
    """
    with open(vcg_filename, "r") as f:
        vcg_data = f.read()

    # Pattern for nodes.
    # Matches: node: { title: "t0_0" label: "t0_0 (18)"  color: lightgreen }
    node_pattern = re.compile(
        r'node:\s*\{\s*title:\s*"([^"]+)"\s+label:\s*"([^"]+)"', re.MULTILINE)
    nodes = node_pattern.findall(vcg_data)
    # Remove duplicates by using a dictionary keyed by title
    unique_nodes = {}
    for title, label in nodes:
        unique_nodes[title] = label

    # Pattern for edges.
    # Matches: edge: { thickness: 2 sourcename:"t0_0" targetname: "t0_1"  label: "(2)" }
    edge_pattern = re.compile(
        r'edge:\s*\{\s*[^}]*sourcename:\s*"([^"]+)"\s*targetname:\s*"([^"]+)"\s*label:\s*"([^"]+)"',
        re.MULTILINE)
    edges = edge_pattern.findall(vcg_data)
    return unique_nodes, edges

def vcg_to_dot(vcg_filename, dot_filename):
    nodes, edges = parse_vcg(vcg_filename)
    with open(dot_filename, "w") as f:
        f.write("digraph G {\n")
        # Write nodes with label attribute.
        for title, label in nodes.items():
            f.write(f'  "{title}" [label="{label}"];\n')
        # Write edges with label attribute.
        for source, target, label in edges:
            f.write(f'  "{source}" -> "{target}" [label="{label}"];\n')
        f.write("}\n")
    print(f"DOT conversion complete! Saved as '{dot_filename}'")

def dot_to_png(dot_filename, png_filename):
    try:
        subprocess.run(["dot", "-Tpng", dot_filename, "-o", png_filename], check=True)
        print(f"PNG image generated as '{png_filename}'")
    except subprocess.CalledProcessError as e:
        print("Error generating PNG with dot:", e)

def vcg_to_xml(vcg_filename, xml_filename):
    nodes, edges = parse_vcg(vcg_filename)
    graph_elem = ET.Element("graph")
    nodes_elem = ET.SubElement(graph_elem, "nodes")
    for title, label in nodes.items():
        ET.SubElement(nodes_elem, "node", title=title, label=label)
    edges_elem = ET.SubElement(graph_elem, "edges")
    for source, target, label in edges:
        ET.SubElement(edges_elem, "edge", sourcename=source, targetname=target, label=label)
    tree = ET.ElementTree(graph_elem)
    tree.write(xml_filename, encoding="utf-8", xml_declaration=True)
    print(f"XML conversion complete! Saved as '{xml_filename}'")

def vcg_to_json(vcg_filename, json_filename):
    nodes, edges = parse_vcg(vcg_filename)
    # Convert nodes to a list of dictionaries
    nodes_list = [{"title": title, "label": label} for title, label in nodes.items()]
    edges_list = [{"source": s, "target": t, "label": l} for s, t, l in edges]
    graph_data = {"nodes": nodes_list, "edges": edges_list}
    with open(json_filename, "w") as f:
        json.dump(graph_data, f, indent=4)
    print(f"JSON conversion complete! Saved as '{json_filename}'")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python vcg_to_all.py input.vcg")
        sys.exit(1)
    
    vcg_filename = sys.argv[1]
    base, _ = os.path.splitext(os.path.basename(vcg_filename))
    
    # Create output folder if it doesn't exist
    if not os.path.exists(OUTPUT_FOLDER):
        os.makedirs(OUTPUT_FOLDER)
    
    # Build file paths using the same base name.
    dot_filename = os.path.join(OUTPUT_FOLDER, base + ".dot")
    png_filename = os.path.join(OUTPUT_FOLDER, base + ".png")
    xml_filename = os.path.join(OUTPUT_FOLDER, base + ".xml")
    json_filename = os.path.join(OUTPUT_FOLDER, base + ".json")
    
    # Perform conversions
    vcg_to_dot(vcg_filename, dot_filename)
    dot_to_png(dot_filename, png_filename)
    vcg_to_xml(vcg_filename, xml_filename)
    vcg_to_json(vcg_filename, json_filename)
