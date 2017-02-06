/*
    Copyright (c) 2010 Séverin Lemaignan (slemaign@laas.fr)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version
    3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef GRAPH_H
#define GRAPH_H

#include <map>
#include <vector>
#include <set>

#include "memoryview_exceptions.h"

#include "node.h"
#include "edge.h"
#include "node_relation.h"

class MemoryView;

class Graph
{
public:
    /**
      a map of nodes. The key is the node ID.
     */
    typedef std::map<int, Node> NodeMap;

private:

    NodeMap nodes;

    typedef std::vector<Edge> EdgeVector;
    EdgeVector edges;

    /**
      Stores pointers to the currently selected nodes
      */
    std::set<Node*> selectedNodes;

public:
    Graph();

    void step(float dt);

    /**
      Renders the graph. If called with argument 'false', goes in simple mode.

      In simple mode, neither edges or special effects are rendered. Useful for picking selected
      primitive in OpenGL GL_SELECT mode.
      */
    void render(rendering_mode mode, MemoryView& env, bool debug = false);

    /**
      Returns an immutable reference to the list of nodes.
      */
    const NodeMap& getNodes() const;

    /**
      Returns a reference to a node by its id. Throws an exception if the node doesn't exist.
      */
    Node& getNode(int id);

    const Node& getConstNode(int id) const;

    /**
      Returns a random node (or nullptr if there is no node at all).
      */
    Node* getRandomNode();

    void select(Node* node);
    void deselect(Node* node);
    void clearSelect();

    /** Returns the selected node (a random one, if several are selected) or
     * nullptr if none are selected.
      */
    Node* getSelected();

    /** Returns the set of all selected nodes
     * (the set may be possibly empty)
     */
    std::set<Node*> getAllSelected() {return selectedNodes;}

    /** Returns the node currently hovered by the mouse, or nullptr if none
     */
    Node* getHovered();

    /**
      Adds a new node to the graph (if it doesn't exist yet) and returns a reference to the new node.
      */
    Node& addNode(int id, const std::string& label, const Node* neighbour = nullptr);

    /**
      Adds a new edge to the graph (if it doesn't exist yet) between rel.from and rel.to
      Returns a reference to the new edge.

      It stores as well in the Edge object the reference to the relation.
      */
    void addEdge(Node& from, Node& to);

    std::vector<const Edge*> getEdgesFor(const Node& node) const;
    Edge* getEdge(const Node& node1, const Node& node2);
    EdgeVector* getEdges() {return &edges;}

    /**
      Computes and update for each node the distance to the closest selected node.
      */
    void updateDistances();
    void recurseUpdateDistances(Node* node, Node* parent, int distance);

    int nodesCount();
    int edgesCount();

    vec2f coulombRepulsionFor(const Node& node) const;
    vec2f coulombRepulsionAt(const vec2f& pos) const;

    vec2f hookeAttractionFor(const Node& node) const;

    /** "Pseudo" gravity that attract nodes towards the center of the screen.

      This force is only applied on selected nodes.
      */
    vec2f gravityFor(const Node& node) const;

    vec2f project(float force, const vec2f& d) const;

    /** Save the current configuration of (displayed) nodes to a file
    (graph.dot)
    **/
    void saveToGraphViz(MemoryView& env);

};

#endif // GRAPH_H
