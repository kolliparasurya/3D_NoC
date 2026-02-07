#include <iostream>
#include <vector>
#include <string>
#include <fstream> // For file copying
#include <iomanip>
using namespace std;

#include "thermal_simulator.h"

// Assume this function contains the core logic of your './new' program
void program_logic(int argc, char **argv)
{
    std::cout << "--- Program logic called with " << argc << " arguments ---" << std::endl;
    for (int i = 0; i < argc; ++i)
    {
        std::cout << "  argv[" << i << "]: " << argv[i] << std::endl;
    }
    // ... your actual program logic would go here ...
    std::cout << "--- Logic finished ---\n"
              << std::endl;
}

// A helper function to copy a file
bool copy_file(const std::string &source, const std::string &destination)
{
    // 1. Open the source file for reading in binary mode.
    std::ifstream src(source, std::ios::binary);

    // This check handles the case where the source file does not exist or cannot be read.
    // It is correct to fail here, as we cannot copy from a non-existent file.
    if (!src)
    {
        std::cerr << "Error: Could not open source file: " << source << std::endl;
        return false;
    }

    // 2. Open the destination file for writing in binary mode.
    // This command will automatically:
    //    a) CREATE the destination file if it does not exist.
    //    b) TRUNCATE (overwrite) the destination file if it already exists.
    std::ofstream dst(destination, std::ios::binary);

    // This checks if the destination file could be created or opened for writing.
    if (!dst)
    {
        std::cerr << "Error: Could not open destination file: " << destination << std::endl;
        return false;
    }

    // 3. Copy the entire contents of the source file buffer to the destination file buffer.
    // This is an efficient, one-line way to copy all data.
    dst << src.rdbuf();

    std::cout << "Successfully copied '" << source << "' to '" << destination << "'" << std::endl;
    return true;
}

// A helper to simulate the command line call
void run_simulation(const std::vector<std::string> &args)
{
    std::vector<char *> argv;
    for (const auto &arg : args)
    {
        // We use const_cast because argv expects `char*`, not `const char*`.
        // This is safe if program_logic doesn't modify the arguments.
        argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr); // Null-terminate the argv array
    initiation(argv.size() - 1, argv.data());
    // 1. Allocate memory for 'act' now that you know 'n'
    int *act;
    act = (int *)malloc(n * sizeof(int));
    if (act == NULL)
    {
        std::cerr << "unable to allocate memory for 'act' array\n";
    }

    // 2. IMPORTANT: You must populate 'act' with data.
    // Since your code doesn't show this, you might need to
    // read from another file. For testing, you can
    // just set all units to "active" (1).
    int k;
    // for (k = 0; k < n / 2; k++)
    //     act[k] = 1;
    // for (k = n / 2; k < n; k++)
    // {
    //     act[k] = 0; // Or read from an activity file
    // }
    for (k = 0; k < n; k++)
    {
        act[k] = 1; // Or read from an activity file
    }
    simulation(act);
}

int main(int argc, char **argv)
{
    // --- First call ---
    std::vector<std::string> args1;
    if (argv[1][0] == '0')
    {
        args1 = {
            "./a.out", "-c", "./hotspot.config", "-p", "new_core3D.ptrace",
            "-steady_file", "avg.steady", "-model_type", "grid", "-detailed_3D", "on",
            "-grid_layer_file", "NoC_layer.lcf", "-grid_map_mode", "avg",
            "-grid_steady_file", "avg.grid.steady"};
        // --- File copy ---
        run_simulation(args1);
        // std::cout << "--- Copying avg.steady to avg.init ---" << std::endl;
        // if (!copy_file("avg.steady", "avg.init"))
        // {
        //     return 1; // Exit if copy fails
        // }
        // std::cout << "--- Copy complete ---\n"
        //           << std::endl;
    }
    else if (argv[1][0] == '1')
    {
        args1 = {
            "./a.out", "-c", "./hotspot.config", "-init_file", "avg.init", "-p",
            "new_core3D.ptrace", "-grid_layer_file", "NoC_layer.lcf", "-model_type",
            "grid", "-detailed_3D", "on", "-o", "avg.ttrace",
            "-grid_transient_file", "avg.grid.ttrace", "-grid_map_mode", "avg"};
        run_simulation(args1);
        // ======================================================================
        std::cout << "\n--- Retrieving Maximum Grid Temperatures (in Kelvin) ---\n"
                  << std::endl;

        // Use the new API functions to get the grid dimensions
        int layers = get_grid_layer_count();
        int rows = get_grid_row_count();
        int cols = get_grid_col_count();
        cout << layers << " " << rows << " " << cols << " lrc" << endl;

        if (layers > 0 && rows > 0 && cols > 0)
        {
            for (int l = 0; l < layers - 2; l += 2)
            {
                std::cout << "--- Layer " << l << " ---" << std::endl;
                for (int i = 0; i < rows; i++)
                {
                    for (int j = 0; j < cols; j++)
                    {
                        // Call the getter for each cell and print it
                        double max_temp = get_max_grid_temperature(l, i, j);
                        std::cout << std::fixed << std::setprecision(2) << max_temp << "\t";
                    }
                    std::cout << std::endl;
                }
                std::cout << std::endl;
            }
        }
        else
        {
            std::cout << "Grid model was not used or dimensions are zero. No temperatures to display." << std::endl;
        }
        // ======================================================================
    }
    stop();

    return 0;
}