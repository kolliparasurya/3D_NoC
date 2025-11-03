import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'
import tensorflow as tf
import keras as kr
from keras.src.layers.core.embedding import Embedding
from keras.src.layers.layer import Layer
from keras._tf_keras.keras.utils import serialize_keras_object, deserialize_keras_object
import model as md
import numpy as np
import node2vec as nv
    
class actor_network(kr.Model):
    def __init__(self, num_heads, key_dim,dff,dropout_rate=0.1,**kwargs):
        super().__init__()
        self.num_heads = num_heads
        self.key_dim = key_dim
        self.dff = dff
        self.dropout_rate = dropout_rate
        self.encoder = md.encoder(num_heads, key_dim, dff, dropout_rate, **kwargs)
        self.decoder = md.a_decoder(key_dim)
    def call(self, inputs):
        if len(inputs.shape) ==2:
            inputs = tf.expand_dims(inputs, axis = 0)
        action_vectors = self.encoder(inputs)
        self.mapping_seq,self.probability = self.decoder(action_vectors)
        return self.mapping_seq, self.probability
    def get_config(self):
        config = super().get_config()
        config.update({
            "num_heads": self.num_heads,
            "key_dim": self.key_dim,
            "dff": self.dff,
            "dropout_rate": self.dropout_rate
        })
        return config
    @classmethod
    def from_config(cls, config):
        return cls(**config)

class critic_network(kr.Model):
    def __init__(self, num_cores, hid_dim, num_heads, key_dim, dff, dropout_rate=0.1, **kwargs):
        super().__init__()
        self.num_cores = num_cores
        self.hid_dim = hid_dim
        self.num_heads = num_heads
        self.key_dim = key_dim
        self.dff = dff
        self.dropout_rate = dropout_rate
        self.encoder = md.encoder(num_heads, key_dim, dff, dropout_rate, **kwargs)
        self.costPrediction = md.cost_prediction(num_cores, key_dim, hidden_dim=hid_dim)
    def call(self, inputs):
        if len(inputs.shape) ==2:
            inputs = tf.expand_dims(inputs, axis = 0)
        action_vectors = self.encoder(inputs)
        self.predicted_cost = self.costPrediction(action_vectors)
        return self.predicted_cost
    def get_config(self):
        config = super().get_config()
        config.update({
            "num_cores": self.num_cores,
            "hid_dim": self.hid_dim,
            "num_heads": self.num_heads,
            "key_dim": self.key_dim,
            "dff": self.dff,
            "dropout_rate": self.dropout_rate
        })
        return config
    @classmethod
    def from_config(cls, config):
        return cls(**config)

class combinedModel(kr.Model):
    def __init__(self, actor, critic, **kwargs):
        super(combinedModel, self).__init__(**kwargs)
        self.actor = actor
        self.critic = critic

    def call(self, inputs):
        mapping_seq, probability = self.actor(inputs)
        cost_prediction = self.critic(inputs)
        return mapping_seq, probability, cost_prediction

    def get_config(self):
        config = super().get_config()
        config.update({
            "actor": serialize_keras_object(self.actor),
            "critic": serialize_keras_object(self.critic)
        })
        return config

    @classmethod
    def from_config(cls, config):
        actor_config = config.pop("actor")
        critic_config = config.pop("critic")
        actor = deserialize_keras_object(actor_config)
        critic = deserialize_keras_object(critic_config)
        return cls(actor=actor, critic=critic, **config)

def saved_combined_model(combined_model, save_path="RL/saved_models/combined_model.keras"):
    os.makedirs(os.path.dirname(save_path), exist_ok=True)
    combined_model.save(save_path)
    print("Combined model saved to:", save_path)

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
    map = tf.unstack(mapping_sequence)[0].numpy()
    map_ind = [np.where(map==i)[0][0] for i in range(0,num_cores)]
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

core_graph_edges = [[0,1,20],[0,2,40],[2, 3, 50],[2, 4, 80],[3, 4, 10]]
core_graph = [[1,2],[0],[0,3,4],[2,4],[2,3]]
mesh_topology = [[1,3],[0,2,4],[1,5],[0,4],[1,3,5],[2,4]]
num_cores = len(core_graph)
num_routers = len(mesh_topology)
core_graph_adj = np.zeros((num_cores, num_cores))
for i in range(0, num_cores):
        for j in core_graph[i]:
                core_graph_adj[i][j] = 1
mesh_topology_hops = count_mesh_totpology_hops(mesh_topology)
num_heads = 3
key_dim = 512
dff = 1024
dropout_rate = 0.1
hid_dim = 2048
K = 10
epoch = 1
actor_lr = 0.01
critic_lr = 0.02

if __name__ == '__main__':
    actor = actor_network(num_heads,key_dim, dff, dropout_rate)
    critic = critic_network(num_cores, hid_dim, num_heads, key_dim, dff, dropout_rate)
    actor_optimizer = kr.optimizers.Adam(actor_lr)
    critic_optimizer = kr.optimizers.Adam(critic_lr)

    combined_model = combinedModel(actor, critic)
    dummy_input = tf.random.uniform((num_cores, key_dim))
    combined_model(dummy_input)
    for i in range(epoch):
        probabilites = []
        rewards = []
        baseline_values = []
        with tf.GradientTape() as gtape:
            for j in range(K):
                embeddings = compute_embeddings(core_graph_adj, key_dim)
                mapping_sequence, probability = actor(embeddings)
                probabilites.append(probability)
                cost_actor = communication_cost_calc(core_graph_edges=core_graph_edges,mesh_topoloy_hops=mesh_topology_hops, mapping_sequence=mapping_sequence)
                with tf.GradientTape() as tape:
                    cost_critic = critic(embeddings)[0][0]
                    reward = -1 * cost_actor
                    rewards.append(reward)
                    baseline_values.append(cost_critic)
                    TD = reward - cost_critic
                    loss = tf.square(TD)
                critic_grads = tape.gradient(loss, critic.trainable_variables)
                critic_optimizer.apply_gradients(zip(critic_grads, critic.trainable_variables))
            probabilites = tf.math.log(tf.stack(probabilites))
            actor_losses = []
            for k in range(K):
                actor_loss = - probabilites[k] * (rewards[k] - baseline_values[k])
                actor_losses.append(actor_loss)
            tot_actor_loss = tf.reduce_mean(actor_losses)
        actor_grads = gtape.gradient(tot_actor_loss, actor.trainable_variables)
        actor_optimizer.apply_gradients(zip(actor_grads, actor.trainable_variables))
    combined_model = combinedModel(actor, critic)
    dummy_input = tf.random.uniform((num_cores, key_dim))
    combined_model(dummy_input)
    combined_model.save("RL/saved_models/combined_model.keras")
    print("Combined model saved!")