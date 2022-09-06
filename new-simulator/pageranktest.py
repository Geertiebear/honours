import networkit as nk

G = nk.Graph(130)
G.addEdge(1, 2)
G.addEdge(0, 1)
G.addEdge(1, 0)
G.addEdge(1, 3)
G.addEdge(1, 4)
G.addEdge(1, 5)
G.addEdge(4, 1)
G.addEdge(5, 1)
G.addEdge(5, 2)
G.addEdge(5, 129)
G.addEdge(2, 6)

result = nk.centrality.PageRank(G)
result.run()
print(result.scores())
