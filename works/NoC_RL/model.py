import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'
import tensorflow as tf
import keras as kr
from keras.src.layers.core.embedding import Embedding
from keras.src.layers.layer import Layer
from keras.src.layers.attention.multi_head_attention import MultiHeadAttention
from keras.src.layers.normalization.layer_normalization import LayerNormalization
from keras.src.layers.merging.add import Add
import numpy as np


class GlobalSelfAttention(Layer):
    mha=None
    layernorm = None
    add = None
    def __init__(self,num_heads: int, key_dim: int, **kwargs):
        super().__init__()
        self.mha = MultiHeadAttention(num_heads, key_dim, **kwargs)
        self.layernorm = LayerNormalization()
        self.add = Add()
    def call(self, x):
        if len(x.shape)==2:
            x = tf.expand_dims(x, axis=0)
        attn_output = self.mha(
            query = x, 
            value = x,
            key = x)
        x = self.add([x, attn_output])
        x = self.layernorm(x)
        return x

class ffd(Layer):
    def __init__(self, dmodel, dff, dropout_rate):
        super().__init__()
        self.seq = kr.Sequential([
            kr.layers.Dense(dff, activation='relu'),
            kr.layers.Dense(dmodel),
            kr.layers.Dropout(dropout_rate)
        ])
        self.add = kr.layers.Add()
        self.layer_norm = kr.layers.LayerNormalization()
    def call(self, x):
        x = self.add([x, self.seq(x)])
        x = self.layer_norm(x)
        return x

class encoder(Layer):
    def __init__(self, num_heads, dmodel, dff, dropout_rate, **kwargs):
        super().__init__()

        self.multi_self_attention = GlobalSelfAttention(
            num_heads = num_heads,
            key_dim=dmodel,
            dropout = dropout_rate,
            **kwargs
        )
        self.ffd = ffd(dmodel, dff, dropout_rate)

    def call(self, x):
        x = self.multi_self_attention(x)
        x = self.ffd(x)
        return x
    
class PointingMechanism(Layer):
    def __init__(self, dmodel):
        super(PointingMechanism, self).__init__()
        self.W_ref = kr.layers.Dense(dmodel)
        self.w_q = kr.layers.Dense(dmodel)
        self.v = kr.layers.Dense(1)
        self.temperature = 1.0
    
    def call(self, action_vectors, query_vector, mask):
        """
        Args:
            action_vectors: (batch_size, num_elements, d_model) - Input elements (cities)
            query_vector: (batch_size, d_model) - The current query vector (state representation)
            mask: (batch_size, num_elements) - Mask to prevent revisiting cities
        
        Returns:
            probabilities: (batch_size, num_elements) - Softmax probabilities over input elements
        """
        action_transformed = self.W_ref(action_vectors)
        query_transformed = tf.expand_dims(self.w_q(query_vector), axis =1)
        scores = self.v(tf.nn.tanh(action_transformed+query_transformed))
        scores = tf.squeeze(scores, axis=-1)
        scores = tf.where(mask, -1e9, scores)
        probabilities = tf.nn.softmax(scores/self.temperature, axis = -1)
        return probabilities

class a_decoder(Layer):
    def __init__(self, dmodel):
        super(a_decoder, self).__init__()
        self.pointing_mechanism = PointingMechanism(dmodel=dmodel)
        
        self.W1 = kr.layers.Dense(dmodel)
        self.W2 = kr.layers.Dense(dmodel)
        self.W3 = kr.layers.Dense(dmodel)
        self.dmodel = dmodel
    
    def compute_query(self, last_3_actions):
        """Computes query vector based on the last 3 actions."""
        que = tf.nn.relu(
            self.W1(last_3_actions[:, -1, :]) +
            self.W2(last_3_actions[:, -2, :]) +
            self.W3(last_3_actions[:, -3, :])
        )
        return que

    def call(self, action_vectors):
        batch_size = tf.shape(action_vectors)[0]
        num_elements = tf.shape(action_vectors)[1]
        dmodel = tf.shape(action_vectors)[2]
        
        query_state = action_vectors[:, 0, :]
        mask = tf.zeros((batch_size, num_elements), dtype=tf.bool)
        selected_indices = []
        last_actions = tf.zeros((batch_size, 3, dmodel))
        
        for _ in range(num_elements):
            probabilities = self.pointing_mechanism(action_vectors, query_state, mask)
            next_index = tf.argmax(probabilities, axis=-1)
            probability = tf.gather(probabilities, next_index, axis=1)
            batch_indices = tf.range(batch_size, dtype=next_index.dtype)
            indices = tf.stack([batch_indices, next_index], axis=1)
            mask = tf.tensor_scatter_nd_update(
                mask,
                indices,
                tf.cast(tf.ones_like(next_index), tf.bool)
            )
            selected_indices.append(next_index)
            next_action = tf.gather(action_vectors, next_index, batch_dims=1)
            next_action = tf.expand_dims(next_action, axis=1)
            last_actions = tf.concat([last_actions[:, 1:, :], next_action], axis=1)
            query_state = self.compute_query(last_actions)
        mapping_seq = tf.stack(selected_indices, axis=1)
        return mapping_seq, probability

class cost_prediction(kr.Model):
    def __init__(self, num_actions, dmodel, hidden_dim):
        super().__init__()
        self.pointing_params = self.add_weight(shape=(num_actions, ), 
                                               initializer="random_normal",
                                               trainable=True,
                                               name="pointing_params")
        self.ffn = kr.Sequential([
            kr.layers.Dense(hidden_dim, activation="relu"),
            kr.layers.Dense(1)
        ])
    
    def call(self, actions):
        pointing_distribution = tf.nn.softmax(self.pointing_params)
        G = tf.reduce_sum(tf.expand_dims(pointing_distribution, axis=-1)*actions, axis=1)
        cost_prediction = self.ffn(G)
        return cost_prediction, pointing_distribution
    



