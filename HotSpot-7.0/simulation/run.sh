# Let us now look at an example of how to model stacked 3-D chips. Let
# us use a simple, 3-block floorplan file 'floorplan1.flp' in addition to
# the more detailed original 'floorplan2.flp'. In the chip we will model, layer 0 is
# power dissipating silicon with a floorplan of 'floorplan1.flp', followed
# by a layer of non-dissipating (passive) TIM. This is then followed by
# another layer of active silicon with a floorplan of 'floorplan2.flp' and
# another layer of passive TIM. Such a layer configuration is described
# in 'example.lcf'. Note that the floorplan files of all layers are specified
# in the LCF instead of via the command line
#-f new_core_layer0.flp 

# ../hotspot -c hotspot.config -p new_core3D.ptrace -model_type grid -detailed_3D on -grid_layer_file NoC_layer.lcf -steady_file gcc.steady -grid_steady_file gcc.grid.steady


# #../hotspot -c hotspot.config -p new_core3D.ptrace -steady_file ./outputs/avg.steady -model_type grid -detailed_3D on -grid_layer_file NoC_layer.lcf -grid_map_mode avg -grid_steady_file ./outputs/avg.grid.steady

# # Copy steady-state results over to initial temperatures
# cp gcc.steady gcc.init

# # Transient simulation
# ../hotspot -c hotspot.config -init_file gcc.init -p new_core3D.ptrace -grid_layer_file NoC_layer.lcf -materials_file example.materials -model_type grid -detailed_3D on -o gcc.ttrace -grid_transient_file gcc.grid.ttrace
# ../hotspot -c hotspot.config -init_file gcc.init -p new_core3D.ptrace -o gcc.ttrace -model_type grid -grid_layer_file NoC_layer.lcf -materials_file example.materials -model_type grid -detailed_3D on 


# Visualize Heat Map of Layer 0 with Perl and with Python script
# ../scripts/split_grid_steady.py outputs/avg.grid.steady 6 6 6
# ../scripts/grid_thermal_map.py new_core_layer0.flp outputs/avg_layer0.grid.steady 6 6 outputs/layer0.png
# ../scripts/grid_thermal_map.pl new_core_layer1.flp outputs/avg_layer1.grid.steady 6 6 > outputs/layer1.svg
# ../scripts/grid_thermal_map.pl new_core_layer2.flp outputs/avg_layer2.grid.steady 6 6 > outputs/layer2.svg
# ../scripts/grid_thermal_map.pl new_core_layer3.flp outputs/avg_layer3.grid.steady 6 6 > outputs/layer3.svg
# ../scripts/grid_thermal_map.pl new_core_layer4.flp outputs/avg_layer4.grid.steady 6 6 > outputs/layer4.svg
# ../scripts/grid_thermal_map.pl new_core_layer5.flp outputs/avg_layer5.grid.steady 6 6 > outputs/layer5.svg

# ../hotspot -c ./hotspot.config -p new_core3D.ptrace -steady_file avg.steady -model_type grid -detailed_3D on -grid_layer_file NoC_layer.lcf -grid_map_mode avg -grid_steady_file avg.grid.steady 
cp avg.steady avg.init
# ../hotspot -c ./hotspot.config -init_file avg.init -p new_core3D.ptrace -grid_layer_file NoC_layer.lcf -model_type grid -detailed_3D on -o avg.ttrace -grid_transient_file avg.grid.ttrace -grid_map_mode avg