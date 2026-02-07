#ifndef THERMAL_SIMULATOR_H
#define THERMAL_SIMULATOR_H
/*
 * This header provides the public interface for the HotSpot
 * trace-level thermal simulator. It allows other parts of an
 * application to initiate the simulator, run the main simulation
 * loop, and perform final calculations and cleanup.
 */

// This standard C/C++ wrapper allows the header to be safely
// included in C++ code. The C++ compiler will be instructed
// not to "mangle" the names of these C functions.
#ifdef __cplusplus
extern "C"
{
#endif

    // #define MAX_DIM 64
    //     double grid_temp[MAX_DIM][MAX_DIM][MAX_DIM];
    //     int xdim, ydim, layers;
    extern int n;
    /**
     * @brief Initializes the thermal model and all related data structures.
     *
     * This function parses command-line arguments, reads configuration files,
     * allocates memory for the thermal model (block or grid), and sets up
     * the initial thermal state. It must be called before any other
     * simulation functions.
     *
     * @param argc The argument count, typically from main().
     * @param argv The argument vector, typically from main().
     */
    void
    initiation(int argc, char **argv);

    /**
     * @brief Runs the main transient simulation loop.
     *
     * @param act
     * This function reads power values from the trace file line by line,
     * computes the corresponding transient temperatures for each time step,
     * and writes the output to the temperature trace file. It also accumulates
     * power values for the final steady-state calculation.
     */
    void simulation(int *act);

    /**
     * @brief Computes final steady-state temperatures and cleans up resources.
     *
     * This function calculates the average power dissipation over the entire
     * trace and computes the final steady-state temperatures. It handles
     * natural convection iterations if enabled, dumps steady-state results
     * to files if specified, and frees all dynamically allocated memory.
     */
    void stop(void);
    //----------------------------------------------------------------------------------------------------------------------------
    int get_grid_layer_count(void);

    /**
     * @brief Returns the number of rows in the thermal grid.
     * @return The row count, or 0 if the grid model is not used.
     */
    int get_grid_row_count(void);

    /**
     * @brief Returns the number of columns in the thermal grid.
     * @return The column count, or 0 if the grid model is not used.
     */
    int get_grid_col_count(void);

    /**
     * @brief Gets the maximum temperature recorded for a specific grid cell.
     *
     * This function returns the peak temperature observed at the given
     * (layer, row, col) coordinate throughout the entire transient simulation.
     *
     * @param layer The layer index.
     * @param row The row index.
     * @param col The column index.
     * @return The maximum temperature in Kelvin, or 0.0 on error.
     */
    double get_max_grid_temperature(int layer, int row, int col);
    //---------------------------------------------------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif // THERMAL_SIMULATOR_H