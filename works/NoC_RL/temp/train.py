import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'
import tensorflow as tf
import keras as kr
from keras.src.layers.core.embedding import Embedding
from keras.src.layers.layer import Layer
import temp.model as md
import numpy as np
import node2vec as nv
    
class actor_network(kr.Model):
    def __init__(self, num_heads, key_dim,dff,dropout_rate=0.1,**kwargs):
        super().__init__()
        self.encoder = md.encoder(num_heads, key_dim, dff, dropout_rate, **kwargs)
        self.decoder = md.a_decoder(key_dim)
    def call(self, inputs):
        if len(inputs.shape) ==2:
            inputs = tf.expand_dims(inputs, axis = 0)
        action_vectors = self.encoder(inputs)
        self.mapping_seq,self.probability = self.decoder(action_vectors)
        return self.mapping_seq, self.probability

class critic_network(kr.Model):
    def __init__(self, num_cores, hid_dim, num_heads, key_dim, dff, dropout_rate=0.1, **kwargs):
        super().__init__()
        self.encoder = md.encoder(num_heads, key_dim, dff, dropout_rate, **kwargs)
        self.costPrediction = md.cost_prediction(num_cores, key_dim, hidden_dim=hid_dim)
    def call(self, inputs):
        if len(inputs.shape) ==2:
            inputs = tf.expand_dims(inputs, axis = 0)
        action_vectors = self.encoder(inputs)
        self.predicted_cost = self.costPrediction(action_vectors)
        return self.predicted_cost

def compute_embeddings(adjacency_matrix, dmodel):
    degree_matrix = np.sum(adjacency_matrix, axis=1)
    degree_inv_sqrt = np.diag(1.0 / np.sqrt(degree_matrix + 1e-6)) 
    normalized_adj = np.dot(degree_inv_sqrt, np.dot(adjacency_matrix, degree_inv_sqrt))
    eigvals, eigvecs = np.linalg.eigh(normalized_adj)
    num_components = min(dmodel, eigvecs.shape[1])
    embeddings = eigvecs[:, :num_components]
    if embeddings.shape[1] < dmodel:
        padding = np.zeros((embeddings.shape[0], dmodel - embeddings.shape[1]))
        embeddings = np.hstack([embeddings, padding])
    return embeddings

def communication_cost_calc(core_graph_edges, mesh_topoloy_hops:np.ndarray, mapping_sequence):
    map = tf.unstack(mapping_sequence)
    map_ind = []
    for i in map:
        map_ind.append(map.index(i))
    communication_cost = 0
    for x in core_graph_edges:
        communication_cost += x[2]*mesh_topoloy_hops[map_ind[x[0]]][map_ind[x[1]]]
    return communication_cost

def hop_count(mesh_topology, num_nodes, v):
    vis = np.full(num_nodes, -1)
    h = 0
    def dfs(v:int, pr:int):
        vis[v] = h
        h += 1
        for x in mesh_topology[v]:
            if x==pr or vis[x]!=-1 or vis[x]<h:
                continue
            dfs(x, v)
        return vis
    return vis


def count_mesh_totpology_hops(mesh_topology):
    num_nodes = len(mesh_topology)
    router_hop_counts = np.full((num_nodes, num_nodes), -1, dtype=int)
    for x in range(num_nodes):
        if(x!=0):
            vis = hop_count(mesh_topology, num_nodes, x)
            router_hop_counts[x] = vis
    return router_hop_counts

#fine tuning with 2-opt serch
def find_best_mapping(core_graph, M_RL, iterations, mesh_topoloy_hops):

    def opt(mapping):
        return [x - 1 for x in mapping]
    
    M = M_RL
    Costbest = communication_cost_calc(core_graph, mesh_topoloy_hops, M)

    for _ in range(iterations):
        Mnew = opt(M)
        Costnew = communication_cost_calc(core_graph, mesh_topoloy_hops, Mnew)
        if Costnew < Costbest:
            M = Mnew
            Costbest = Costnew

    return M

core_graph = [1, 2, 3, 4]
M_RL = [4, 3, 2, 1]
iterations = 10


core_graph_edges = [[1,2,20],[1,3,40],[]]
core_graph = [[],[2,3],[1],[1,4,5],[3,5],[3,4]]
mesh_topology = [[],[2,4],[1,3,5],[2,6],[1,5],[2,4,6],[3,5]]
num_cores = len(core_graph)
num_routers = len(mesh_topology)
core_graph_adj = np.zeros((num_cores-1, num_cores-1))
for i in range(0, num_cores):
    if(i!=0):
        for j in range(len(core_graph[i])):
            if(j!=0):
                core_graph_adj[i-1][j-1] = 1
mesh_topology_hops = count_mesh_totpology_hops(mesh_topology)
num_heads = 3
key_dim = 512
dff = 1024
dropout_rate = 0.1
hid_dim = 2048
K = 10
epoch = 10

actor_lr = 0.01
critic_lr = 0.02

actor = actor_network(num_heads,key_dim, dff, dropout_rate)
critic = critic_network(num_cores, hid_dim, num_heads, key_dim, dff, dropout_rate)
actor_optimizer = kr.optimizers.Adam(actor_lr)
critic_optimizer = kr.optimizers.Adam(critic_lr)

for i in range(epoch):
    probabilites = []
    rewards = []
    baseline_values = []
    for j in range(K):
        embeddings = compute_embeddings(core_graph_adj, key_dim)
        mapping_sequence, probability = actor(embeddings)
        cost_actor = communication_cost_calc(core_graph_edges=core_graph,mesh_topoloy_hops=mesh_topology_hops, mapping_sequence=mapping_sequence)
        cost_critic = critic(embeddings)
        reward = -1*cost_actor
        TD = reward - cost_critic
        probabilites.append(probability)
        rewards.append(reward)
        baseline_values.append(cost_critic)
        with tf.GradientTape() as tape:
            value = tf.constant(TD)
            loss = tf.square(value)
        critic_grads = tape.gradient(loss, critic.trainable_variables)
        critic_optimizer.apply_gradients(zip(critic_grads, critic.trainable_variables))
    probabilites = np.log(np.array(probabilites))
    with tf.GradientTape as tape:
        actor_losses = []
        for i in range(K):
            log_prob = probabilites[i]
            actor_loss = tf.multiply(tf.constant(log_prob)*tf.constant(reward[i]-baseline_values[i]))
            actor_losses.append(actor_loss)
        actor_losses = tf.convert_to_tensor(actor_losses)
        tot_actor_loss = tf.reduce_mean(actor_losses)
    actor_grads = tape.gradient(loss, actor.trainable_variables)
    actor_optimizer.apply_gradients(zip(actor_grads, actor.trainable_variables))


best_mapping = find_best_mapping(core_graph, M_RL, iterations, mesh_topology_hops)
print("Best Mapping Sequence:", best_mapping)




    





