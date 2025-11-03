import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'
import tensorflow as tf
import keras as kr
import numpy as np
import model as md
from train import actor_network, critic_network, compute_embeddings  # Ensure these do not run training code
from keras._tf_keras.keras.utils import deserialize_keras_object  # Already used in your combinedModel

class combinedModel(kr.Model):
    def __init__(self, actor, critic, **kwargs):
        super(combinedModel, self).__init__(**kwargs)
        self.actor = actor
        self.critic = critic
    
    def build(self, input_shape):
        if not self.actor.build:
            self.actor.build(input_shape)
        if not self.critic.build:
            self.critic.build(input_shape)
        super(combinedModel,self).build(input_shape)

    def call(self, inputs):
        mapping_seq, probability = self.actor(inputs)
        cost_prediction = self.critic(inputs)
        return mapping_seq, probability, cost_prediction

    def get_config(self):
        config = super().get_config()
        config.update({
            "actor": tf.keras.utils.serialize_keras_object(self.actor),
            "critic": tf.keras.utils.serialize_keras_object(self.critic)
        })
        return config

    @classmethod
    def from_config(cls, config):
        actor_config = config.pop("actor")
        critic_config = config.pop("critic")
        actor = tf.keras.utils.deserialize_keras_object(actor_config)
        critic = tf.keras.utils.deserialize_keras_object(critic_config)
        return cls(actor=actor, critic=critic, **config)    

def load_combined_model(save_path="RL/saved_models/combined_model.keras"):
    custom_objects = {
        "combinedModel": combinedModel,
        "actor_network": actor_network,
        "critic_network": critic_network,
        "encoder": md.encoder,
        "a_decoder": md.a_decoder,
        "globalSelfAttention": md.GlobalSelfAttention,
        "ffd": md.ffd,
        "pointingMechanism": md.PointingMechanism,
        "cost_prediction": md.cost_prediction,
    }
    combined_model = tf.keras.models.load_model(save_path, custom_objects=custom_objects)
    print("Combined model loaded from:", save_path)
    return combined_model

def run_inference(core_graph_adj, key_dim, combined_model):
    embeddings = compute_embeddings(core_graph_adj, key_dim)
    mapping_seq, probability, cost_prediction = combined_model(embeddings)
    predicted_cost, pointing_distribution = cost_prediction
    print("Mapping sequence:", mapping_seq.numpy())
    print("Action probabilities:", probability.numpy())
    print("Predicted cost:", predicted_cost.numpy())
    return mapping_seq, probability, cost_prediction

if __name__ == "__main__":
    from train import core_graph_adj, key_dim  # Ensure these variables are defined in train.py
    combined_model = load_combined_model("RL/saved_models/combined_model.keras")
    run_inference(core_graph_adj, key_dim, combined_model)
