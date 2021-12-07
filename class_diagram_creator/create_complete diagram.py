import pydot
import os
import re

name_re = re.compile("\"\{(.+)\\\\n")

dot_nodes_dict:dict[str, pydot.Node] = dict()
dot_edges_dict:dict[(str,str),pydot.Edge] = dict()

duplication_whitelist:set[str] = set()
duplicate_count:dict[str, int] = dict()

with open("duplicates_whitelist.txt", "r") as dwf:
    for line in dwf.readlines():
        line_cleaned = line.strip()
        if len(line_cleaned) != 0 and not line_cleaned.startswith("//"):
            duplication_whitelist.add(pydot.quote_if_necessary(line_cleaned))

for root, dirs, files in os.walk("./input", topdown=False):
    for name in files:
        dot_file = os.path.join(root, name)
        print(f"parsing {name}")

        dot_graph:pydot.Dot = pydot.graph_from_dot_file(dot_file)[0]
        
        node_mapping:dict[str,str] = dict()

        nodes:list[pydot.Node] = dot_graph.get_node_list()
        edges:list[pydot.Edge] = dot_graph.get_edge_list()
        for node in nodes:

            attr:dict = node.get_attributes()
            attr.pop("fillcolor", None)
            attr.pop("style", None)
            attr.pop("URL", None)
            if "label" in attr:
                label:str = attr["label"]
                new_name = pydot.quote_if_necessary(name_re.match(label).group(1).replace("::", "_"))
                
                if new_name in duplication_whitelist:
                    count = duplicate_count.get(new_name, 0)
                    duplicate_count[new_name] = count + 1
                    new_dup_name = pydot.quote_if_necessary(new_name.strip("\"") + f"_{count}")
                    node_mapping[node.get_name()] = new_dup_name
                    node.set_name(new_dup_name)
                else:
                    node_mapping[node.get_name()] = new_name
                    node.set_name(new_name)

            if node.get_name() in dot_nodes_dict:
                present_node = dot_nodes_dict[node.get_name()]
                if "label" in present_node.get_attributes() and "label" in node.get_attributes():
                    present_label = present_node.get_attributes()["label"]
                    label = node.get_attributes()["label"]
                    if label > present_label:
                        dot_nodes_dict[node.get_name()] = node
            else:
                dot_nodes_dict[node.get_name()] = node
        
        for edge in edges:
            points = (node_mapping[edge.get_source()], node_mapping[edge.get_destination()])
            edge.obj_dict['points'] = (node_mapping[edge.get_source()], node_mapping[edge.get_destination()])
            if points not in dot_edges_dict:
                dot_edges_dict[points] = edge

graph = pydot.Dot('complete_class_graph', graph_type=dot_graph.get_type(), bgcolor="transparent")

for node_name, node in dot_nodes_dict.items():
    graph.add_node(node)

for edge_points, edge in dot_edges_dict.items():
    graph.add_edge(edge)

# todo remove duplicate nodes

graph.write_raw('output_raw.dot')
graph.write_png('output.png')
