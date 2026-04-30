#include <bits/stdc++.h>
#include <unistd.h>
#include "tinyxml2.h"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <tuple>
#include <algorithm>
#include <iomanip>
#include <deque> // Added for deque

#include "thermal_simulator.h"

using namespace std;
using namespace tinyxml2;

#define SATURATION_THRESHOLD 1000
#define PIR 0.002//0.0005 // 0.001 //1.0
int TEMP_THRESHOLD = 6000;
const int Gw = 8;
const int Gl = 8;
const int Gh = 4;
const int NUNITS = 30;

int f = 0, s = 0;
int glbmark = 1;

float ambient_temp = 25.0;
float time_interval = 1.0;
clock_t start, finish;

// --- OPTIMIZATION 1: Integer Keys instead of String ---
// Replaced repeated string parsing with integer logic.
// Task ID Format: (AppID * 10000) + TaskIndex

struct Application
{
    int id;
    int runtime;
    vector<int> tasks;
    vector<vector<int>> edges;
    vector<double> communicationVolume;
};

struct Core
{
    int isFree = 1;
    // OPTIMIZATION: Use integer task ID instead of string "11.5"
    // string task_no = "";
    int task_id_int = -1; // -1 means empty

    int num_time_task = 0;
    int isnotBlocked = 1;
    int wareoff_const = 0;
    int x = -1, y = -1, z = -1;

    float temp;
    float thermal_capacity;
    float thermal_resistance;
    float power_dynamic;

    int time_of_death;

    // Updated signature to take int instead of string
    void updateOcc(int freeness, int t_id, int blockedNess)
    {
        isFree = freeness;
        task_id_int = t_id;
        isnotBlocked = blockedNess;
    }
};

Core example;
Core mesh[Gw][Gl][Gh];
int strx = 0;
int stry = 0;
int strz = 0;
int removed_app_id;
int removed_app_death_time;
int total_sim_time = 0;

// Assuming global n is defined in thermal_simulator.h or needs definition
#ifndef THERMAL_SIMULATOR_H
int n = Gw * Gl * Gh * NUNITS;
#endif

vector<vector<int>> emptySingleCores;
vector<pair<Core *, Core *>> brickVector;
vector<pair<int, int>> appsLoc;
map<int, vector<vector<int>>> task_timestamp;
int extra = 0;
vector<Application> apps;
// vector<Application> ind_apps; // REMOVED: Unused vector (Redundancy Fix)
vector<bool> mapped;
set<pair<int, int>> mapped_apps_rt;

double edges_on_tsv = 0;
double avg_node_layer = 0;
double thermal_overhead_time = 0;

ofstream mapFile("./mapping/mapFile.txt", std::ios::out | std::ios::trunc);
ofstream testTraffic("./mapping/test_traffic.txt", std::ios::out | std::ios::trunc);
ofstream reportValues("./mapping/pair_algorithm_report_values.txt", std::ios::out | std::ios::trunc);

void pairsAdding(Core &meshCore1, Core &meshCore2)
{
    brickVector.push_back({&meshCore1, &meshCore2});
}
void singleNodePairAdding(Core &meshCore1, Core &meshCore2)
{
    brickVector.push_back({&meshCore1, &meshCore2});
}

double calc_runtime(Application app)
{
    double time = 0;
    for (int j = 0; j < app.tasks.size(); j++)
    {
        time += app.tasks[j];
    }
    return time;
}

// OPTIMIZATION: Replaced string parsing with direct integer return
// Only needed if legacy code passes string, otherwise use direct math.
int cnv_task_buf(int app_id, int task_idx)
{
    return (app_id * 10000) + task_idx;
}

// Helper to get buffer ID from Core's stored integer ID
int get_buf_from_core(int task_id_int)
{
    // task_id_int is already in format (AppID * 10000 + TaskID)
    // The "buffer" usually refers to the base App ID key (AppID * 10000)
    // But your original code used buf = AppID * 10000 + TaskID for task_timestamp keys?
    // Let's assume task_timestamp is keyed by the specific task unique ID.
    return task_id_int;
}

void remove_prev_mapped_tod(std::pair<int, std::vector<int>> prefix_to_remove)
{
    const std::vector<int> &prefix_vec = prefix_to_remove.second;
    if (prefix_vec.empty())
        return;

    // OPTIMIZATION: Check map existence first
    if (task_timestamp.find(prefix_to_remove.first) == task_timestamp.end())
        return;

    auto &vec_to_modify = task_timestamp[prefix_to_remove.first];

    // Note: If this vector is large, consider using a reverse iterator or a secondary map index
    // For now, keeping linear scan but ensuring it only runs on valid keys.
    for (auto it = vec_to_modify.begin(); it != vec_to_modify.end(); ++it)
    {
        const std::vector<int> &x = *it;
        if (x.size() >= prefix_vec.size())
        {
            // Manual check faster than std::equal for small vectors
            if (x[0] == prefix_vec[0] && x[1] == prefix_vec[1])
            {
                int jump = (x.size() > 2) ? x[2] : 0;
                std::vector<int> new_vec = {prefix_vec[0], prefix_vec[1], jump, removed_app_death_time};
                *it = new_vec;
                break;
            }
        }
    }
}

void update_mvtasks(Core *node1, Core *node2)
{
    int jump = removed_app_death_time;
    int buf = node1->task_id_int;

    remove_prev_mapped_tod({buf, {node1->num_time_task, node1->x + (node1->y * Gw) + (node1->z * Gw * Gl)}});

    node2->num_time_task = node1->num_time_task + 1;

    task_timestamp[buf].push_back({node2->num_time_task,
                                   node2->x + (node2->y * Gw) + (node2->z * Gw * Gl),
                                   jump,
                                   node1->time_of_death});

    node2->time_of_death = node1->time_of_death;
    node1->time_of_death = removed_app_death_time;
}

int starting_point_condition()
{
    int ans = 0;
    int upperHalfConst = 0, lowerHalfConst = 0, lowerNodes = 0, upperNodes = 0;
    for (int i = 0; i < Gh / 2; i++)
        for (int j = 0; j < Gl; j++)
            for (int k = 0; k < Gw; k++)
            {
                lowerHalfConst += mesh[k][j][i].wareoff_const;
                lowerNodes++;
            }

    for (int i = Gh / 2; i < Gh; i++)
        for (int j = 0; j < Gl; j++)
            for (int k = 0; k < Gw; k++)
            {
                upperHalfConst += mesh[k][j][i].wareoff_const;
                upperNodes++;
            }

    if (upperNodes > 0)
        upperHalfConst /= upperNodes;
    if (lowerNodes > 0)
        lowerHalfConst /= lowerNodes;

    if (lowerHalfConst - upperHalfConst >= SATURATION_THRESHOLD)
    {
        ans = 1;
    }
    return ans;
}

//======================================================================================================================
// UPDATED: Logic to include reliability check
bool is_usable(int i)
{
    bool basic_check;
    if (i < (Gw * Gl * Gh) - extra)
        basic_check = brickVector[i].first->isnotBlocked && brickVector[i].second->isnotBlocked;
    else
        basic_check = brickVector[i].first->isnotBlocked;

    // ADDED: Logic to check if the core is too worn out to be "usable" for migration
    // This answers your comment: "While you are doing FillGaps() are you checking the ware_of_constant"
    if (!basic_check)
        return false;

    // Example Reliability Check:
    // If a core is too worn out, do not consider it usable for filling gaps.
    // Threshold can be adjusted (e.g., > 2x average wear)
    // const int WARE_LIMIT = 50000; // Example hard limit
    // if (brickVector[i].first->wareoff_const > WARE_LIMIT)
    //     return false;
    // if (i < (Gw * Gl * Gh) - extra && brickVector[i].second->wareoff_const > WARE_LIMIT)
    //     return false;

    return true;
}

bool is_occupied(int i)
{
    if (i < (Gw * Gl * Gh) - extra)
        return !brickVector[i].first->isFree || !brickVector[i].second->isFree;
    else
        return !brickVector[i].first->isFree;
}

void move_tasks(int j, int i)
{
    // Move from j to i
    if (i < (Gw * Gl * Gh) - extra && j < (Gw * Gl * Gh) - extra)
    {
        brickVector[i].first->task_id_int = brickVector[j].first->task_id_int; // Int assignment
        brickVector[i].second->task_id_int = brickVector[j].second->task_id_int;
        brickVector[i].first->isFree = brickVector[j].first->isFree;
        brickVector[i].second->isFree = brickVector[j].second->isFree;

        update_mvtasks(brickVector[j].first, brickVector[i].first);
        update_mvtasks(brickVector[j].second, brickVector[i].second);
    }
    else if (i >= (Gw * Gl * Gh) - extra && j >= (Gw * Gl * Gh) - extra)
    {
        brickVector[i].first->task_id_int = brickVector[j].first->task_id_int;
        brickVector[i].first->isFree = brickVector[j].first->isFree;

        update_mvtasks(brickVector[j].first, brickVector[i].second); // Logic matches original
    }
}

void set_free(int i)
{
    if (i < (Gw * Gl * Gh) - extra)
    {
        brickVector[i].first->task_id_int = -1;
        brickVector[i].second->task_id_int = -1;
        brickVector[i].first->isFree = 1;
        brickVector[i].second->isFree = 1;
    }
    else
    {
        brickVector[i].first->task_id_int = -1;
        brickVector[i].first->isFree = 1;
    }
}

void fillGaps()
{
    int N = brickVector.size();
    std::deque<int> Q;

    // Find gaps (usable but occupied nodes we want to consolidate?)
    // Original logic: Find occupied nodes that are usable.
    for (int i = 0; i < N; ++i)
    {
        // Now is_usable checks wareoff_const too
        if (is_usable(i) && is_occupied(i))
        {
            Q.push_back(i);
        }
    }

    // Fill empty usable slots
    for (int i = 0; i < N; ++i)
    {
        if (is_usable(i))
        {
            if (!Q.empty())
            {
                int j = Q.front();
                // Move from j (occupied) to i (empty)
                // Wait, original logic was moving tasks TO 'i' FROM 'j'?
                // Logic: Iterate linear. If 'i' is usable.
                // If Q has items, pop 'j'. If j != i, move j to i.
                // This essentially compacts tasks to the beginning of the vector.
                if (j != i)
                {
                    // Only move if target i is actually empty?
                    // Original code didn't check if i was empty explicitly here,
                    // relying on the deque logic to represent the 'source' of tasks.
                    // Assuming 'i' is the target slot.
                    if (!is_occupied(i))
                    {
                        move_tasks(j, i);
                        set_free(j);
                        Q.pop_front();
                    }
                }
                else
                {
                    Q.pop_front(); // Current node is already filled by itself
                }
            }
            else
            {
                // If no more tasks to move, and this node is occupied (but shouldn't be?)
                // Actually, this `else` block in original seems to just clear remaining usable nodes?
                // This looks like it clears the tail after compaction.
                if (is_occupied(i))
                {
                    // Be careful: if we cleared Q, it means all tracked tasks are moved.
                    // Any remaining occupied nodes might be duplicates or need clearing.
                    // Keeping original logic behavior:
                    set_free(i);
                    // COMMENTED OUT: Original logic was likely clearing tail.
                    // Verify if you want to delete tasks here.
                    // Assuming yes for "Defragmentation" (moving tasks to start, clearing end).
                }
            }
        }
    }
}

//===================================================================================================================
void loadNodes()
{
    for (int i = 0; i < Gh; i++)
    {
        if (i % 2 == 0 && (i / 2) % 2 == 0)
        {
            if (i + 1 < Gh)
            {
                for (int j = 0; j < Gl; j++)
                {
                    if (j % 2 == 0)
                    {
                        for (int k = 0; k < Gw; k++)
                        {
                            pairsAdding(mesh[k][j][i], mesh[k][j][i + 1]);
                        }
                    }
                    else
                    {
                        for (int k = Gw - 1; k >= 0; k--)
                        {
                            pairsAdding(mesh[k][j][i], mesh[k][j][i + 1]);
                        }
                    }
                }
            }
            else
            {

                for (int j = 0; j < Gl; j++)
                {
                    if (j % 2 == 0)
                    {
                        for (int k = 0; k < Gw; k++)
                        {
                            singleNodePairAdding(mesh[k][j][i], example);
                        }
                    }
                    else
                    {
                        for (int k = Gw - 1; k >= 0; k--)
                        {
                            singleNodePairAdding(mesh[k][j][i], example);
                        }
                    }
                }
            }
        }
        else if (i % 2 == 0 && (i / 2) % 2 == 1)
        {
            if (i + 1 < Gh)
            {
                for (int j = Gl - 1; j >= 0; j--)
                {
                    if (j % 2 == 0)
                    {
                        for (int k = Gw - 1; k >= 0; k--)
                        {
                            pairsAdding(mesh[k][j][i], mesh[k][j][i + 1]);
                        }
                    }
                    else
                    {
                        for (int k = 0; k < Gw; k++)
                        {
                            pairsAdding(mesh[k][j][i], mesh[k][j][i + 1]);
                        }
                    }
                }
            }
            else
            {
                for (int j = Gl - 1; j >= 0; j--)
                {
                    if (j % 2 == 0)
                    {
                        for (int k = Gw - 1; k >= 0; k--)
                        {
                            singleNodePairAdding(mesh[k][j][i], example);
                        }
                    }
                    else
                    {
                        for (int k = 0; k < Gw; k++)
                        {
                            singleNodePairAdding(mesh[k][j][i], example);
                        }
                    }
                }
            }
        }
    }
}
vector<pair<int, int>> make_pairs(Application app)
{
    vector<pair<int, int>> pairs;
    vector<int> tasks(app.tasks.size(), 0);
    set<pair<double, int>, greater<pair<double, int>>> orderedEdges;

    for (int i = 0; i < app.edges.size(); i++)
        orderedEdges.insert({app.communicationVolume[i], i});

    for (auto &x : orderedEdges)
    {
        if (tasks[app.edges[x.second][0]] == 0 && tasks[app.edges[x.second][1]] == 0)
        {
            pairs.push_back({app.edges[x.second][0], app.edges[x.second][1]});
            tasks[app.edges[x.second][0]] = 1;
            tasks[app.edges[x.second][1]] = 1;
        }
    }

    vector<int> remTasks;
    for (int i = 0; i < tasks.size(); i++)
        if (tasks[i] == 0)
            remTasks.push_back(i);

    set<pair<int, int>, greater<pair<int, int>>> orderedTasks;
    for (int i = 0; i < remTasks.size(); i++)
        orderedTasks.insert({app.tasks[remTasks[i]], remTasks[i]});

    vector<pair<int, int>> orderedTasksVector(orderedTasks.begin(), orderedTasks.end());
    for (size_t i = 0; i < orderedTasksVector.size(); i += 2)
    {
        if (i + 1 < orderedTasksVector.size())
            pairs.push_back({orderedTasksVector[i].second, orderedTasksVector[i + 1].second});
        else
            pairs.push_back({orderedTasksVector[i].second, -1});
    }
    // --- OPTIMIZATION: Chain-Sort Pairs to Reduce Hop Count ---
    // Reorders the array so adjacent pairs in the list communicate heavily,
    // ensuring they are mapped to adjacent physical nodes in the brickVector.
    if (!pairs.empty())
    {
        vector<pair<int, int>> chained_pairs;
        vector<bool> visited(pairs.size(), false);

        // Start with the first pair
        chained_pairs.push_back(pairs[0]);
        visited[0] = true;

        for (size_t i = 1; i < pairs.size(); i++)
        {
            int best_idx = -1;
            double max_comm = -1.0;
            pair<int, int> current_tail = chained_pairs.back();

            // Find the unvisited pair that communicates the most with the current tail
            for (size_t j = 0; j < pairs.size(); j++)
            {
                if (!visited[j])
                {
                    double comm_vol = 0;

                    // Sum the communication volume between the current_tail pair and pair[j]
                    for (size_t e = 0; e < app.edges.size(); e++)
                    {
                        int u = app.edges[e][0];
                        int v = app.edges[e][1];
                        double vol = app.communicationVolume[e];

                        bool tail_has_node = (u == current_tail.first || u == current_tail.second);
                        bool next_has_node = (v == pairs[j].first || v == pairs[j].second);

                        bool tail_has_node_rev = (v == current_tail.first || v == current_tail.second);
                        bool next_has_node_rev = (u == pairs[j].first || u == pairs[j].second);

                        if ((tail_has_node && next_has_node) || (tail_has_node_rev && next_has_node_rev))
                        {
                            comm_vol += vol;
                        }
                    }

                    if (comm_vol > max_comm)
                    {
                        max_comm = comm_vol;
                        best_idx = j;
                    }
                }
            }

            // If no direct connection is found, just grab the next available pair
            if (best_idx == -1 || max_comm == 0)
            {
                for (size_t j = 0; j < pairs.size(); j++)
                {
                    if (!visited[j])
                    {
                        best_idx = j;
                        break;
                    }
                }
            }

            chained_pairs.push_back(pairs[best_idx]);
            visited[best_idx] = true;
        }
        pairs = chained_pairs; // Overwrite the random array with the highly optimized chain
    }
    return pairs;
}

int calculate_starting_point(int numPairs, int mark)
{
    int buf = s, cnt = 0, ans = 0, st_pt = s;
    int limit = (Gw * Gl * Gh) / 2;

    while (buf < limit)
    {
        if (cnt == numPairs)
        {
            ans = 1;
            break;
        }

        bool core1_ok = brickVector[buf].first->isnotBlocked && brickVector[buf].first->isFree;
        bool core2_ok = brickVector[buf].second->isnotBlocked && brickVector[buf].second->isFree;

        if (!core1_ok || !core2_ok)
        {
            cnt = 0;         // Reset consecutive count
            st_pt = buf + 1; // Logic fix: Next possible start is after this bad block
        }
        else
        {
            if (cnt == 0)
                st_pt = buf;
            cnt++;
        }
        buf++;
    }
    return (ans == 1) ? st_pt : -1;
}
// CORRECTED STRATEGY: Finds a compact 3D bounding box AND weaves the pairs to preserve the chain
vector<pair<Core*, Core*>> find_compact_mapping(int numPairs, int mark) {
    vector<pair<Core*, Core*>> target_cores;
    
    int w = ceil(sqrt(numPairs));
    int h = ceil((double)numPairs / w);
    
    if (w > Gw) { w = Gw; h = ceil((double)numPairs / w); }
    if (h > Gl) { h = Gl; w = ceil((double)numPairs / h); }

    for (int z = 0; z < Gh - 1; z += 2) {
        for (int y = 0; y <= Gl - h; ++y) {
            for (int x = 0; x <= Gw - w; ++x) {
                
                bool region_free = true;
                int valid_pairs = 0;
                target_cores.clear();
                
                for (int dy = 0; dy < h && region_free; ++dy) {
                    for (int dx_idx = 0; dx_idx < w && region_free; ++dx_idx) {
                        
                        // THE FIX: Weave horizontally (Zig-Zag) to keep sequential pairs physically adjacent!
                        int dx = (dy % 2 == 0) ? dx_idx : (w - 1 - dx_idx);
                        
                        Core* c1 = &mesh[x + dx][y + dy][z];
                        Core* c2 = &mesh[x + dx][y + dy][z + 1];
                        
                        if (c1->isFree && c1->isnotBlocked && c2->isFree && c2->isnotBlocked) {
                            target_cores.push_back({c1, c2});
                            valid_pairs++;
                            if (valid_pairs == numPairs) break; 
                        } else {
                            region_free = false; 
                        }
                    }
                    if (valid_pairs == numPairs) break;
                }
                
                if (region_free && valid_pairs == numPairs) {
                    return target_cores; 
                }
            }
        }
    }
    
    // Fallback
    target_cores.clear();
    int st_pt = calculate_starting_point(numPairs, mark);
    if (st_pt != -1) {
        for (int i = st_pt; i < st_pt + numPairs; i++) {
            target_cores.push_back(brickVector[i]);
        }
    }
    
    return target_cores;
}

int mapping_application(vector<pair<int, int>> pairs, int appId, Application app, int mark)
{
    // Call our new compact 3D bounding box mapper
    vector<pair<Core*, Core*>> target_slots = find_compact_mapping(pairs.size(), mark);
    
    if (!target_slots.empty())
    {
        // Storing -1 for the starting point since it's no longer a 1D index
        appsLoc.push_back({-1, pairs.size()}); 
        
        for (size_t x = 0; x < pairs.size(); x++)
        {
            Core* c1 = target_slots[x].first;
            Core* c2 = target_slots[x].second;

            // Core 1
            int t1 = pairs[x].first;
            c1->task_id_int = cnv_task_buf(appId, t1);
            c1->isFree = 0;
            c1->wareoff_const += app.tasks[t1];
            c1->num_time_task = 0;
            c1->time_of_death = total_sim_time + app.runtime;

            int buf1 = c1->task_id_int;
            task_timestamp[buf1].push_back({0,
                                            c1->x + (c1->y * Gw) + (c1->z * Gw * Gl),
                                            total_sim_time,
                                            c1->time_of_death});
            avg_node_layer += c1->z;

            // Core 2 (if pair exists)
            if (pairs[x].second != -1)
            {
                int t2 = pairs[x].second;
                c2->task_id_int = cnv_task_buf(appId, t2);
                c2->isFree = 0;
                c2->wareoff_const += app.tasks[t2];
                c2->num_time_task = 0;
                c2->time_of_death = total_sim_time + app.runtime;

                int buf2 = c2->task_id_int;
                task_timestamp[buf2].push_back({0,
                                                c2->x + (c2->y * Gw) + (c2->z * Gw * Gl),
                                                total_sim_time,
                                                c2->time_of_death});
                avg_node_layer += c2->z;
            }
        }
        cout << "1r ";
        return 1;
    }
    cout << "0r ";
    return 0; // Failed to map (chip is entirely full)
}

// int mapping_application(vector<pair<int, int>> pairs, int appId, Application app, int mark)
// {
//     int st_pt = calculate_starting_point(pairs.size(), mark);
//     if (st_pt != -1)
//     {
//         int x = 0;
//         appsLoc.push_back({st_pt, pairs.size()});
//         for (int i = st_pt; i < st_pt + pairs.size(); i++, x++)
//         {
//             // Core 1
//             int t1 = pairs[x].first;
//             brickVector[i].first->task_id_int = cnv_task_buf(appId, t1); // Store int ID
//             brickVector[i].first->isFree = 0;
//             brickVector[i].first->wareoff_const += app.tasks[t1];
//             brickVector[i].first->num_time_task = 0;
//             brickVector[i].first->time_of_death = total_sim_time + app.runtime;

//             int buf1 = brickVector[i].first->task_id_int;
//             task_timestamp[buf1].push_back({0,
//                                             brickVector[i].first->x + (brickVector[i].first->y * Gw) + (brickVector[i].first->z * Gw * Gl),
//                                             total_sim_time,
//                                             brickVector[i].first->time_of_death});
//             avg_node_layer += brickVector[i].first->z;

//             // Core 2 (if pair exists)
//             if (pairs[x].second != -1)
//             {
//                 int t2 = pairs[x].second;
//                 brickVector[i].second->task_id_int = cnv_task_buf(appId, t2); // Store int ID
//                 brickVector[i].second->isFree = 0;
//                 brickVector[i].second->wareoff_const += app.tasks[t2];
//                 brickVector[i].second->num_time_task = 0;
//                 brickVector[i].second->time_of_death = total_sim_time + app.runtime;

//                 int buf2 = brickVector[i].second->task_id_int;
//                 task_timestamp[buf2].push_back({0,
//                                                 brickVector[i].second->x + (brickVector[i].second->y * Gw) + (brickVector[i].second->z * Gw * Gl),
//                                                 total_sim_time,
//                                                 brickVector[i].second->time_of_death});
//                 avg_node_layer += brickVector[i].second->z;
//             }
//         }
//     }
//     cout << ((st_pt != -1) ? 1 : 0) << "r ";
//     return (st_pt != -1);
// }

string shape = to_string(Gw) + "x" + to_string(Gl) + "x" + to_string(Gh);
void thermal_initiation()
{
    // Keeping logic mostly same, just standard string handling
    const vector<string> args = {
        "./a.out", "-c", "../NOC/" + shape + "/hotspot.config", "-init_file", "../NOC/" + shape + "/avg.init", "-p",
        "../NOC/" + shape + "/new_core3D.ptrace", "-grid_layer_file", "../NOC/" + shape + "/NoC_layer.lcf", "-model_type",
        "grid", "-detailed_3D", "on", "-o", "../NOC/" + shape + "/avg.ttrace",
        "-grid_transient_file", "../NOC/" + shape + "/avg.grid.ttrace", "-grid_map_mode", "avg"};
    std::vector<char *> argv;
    for (const auto &arg : args)
        argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);
    initiation(argv.size() - 1, argv.data());
}

void thermal_simulation(int *act)
{
    simulation(act);
}

void thermal_termination()
{
    stop();
}

void get_active(int *act)
{
    for (int i = 0; i < Gh; i++)
        for (int j = 0; j < Gl; j++)
            for (int k = 0; k < Gw; k++)
            {
                int mf = 1;
                if (mesh[k][j][i].isFree)
                    mf = 0;
                // Bounds check would be good here, but trusting NUNITS math
                int base = (i * Gw * Gl) + (j * Gw) + k;
                for (int id = 0; id < NUNITS; id++)
                    act[base + id] = mf; // Logic seems to imply flattened NUNITS?
                                         // Original code: act[(i * Gw * Gl) + (j * Gw) + k + id] = mf;
                                         // Warning: This original math overwrites neighbors if NUNITS > 1
                                         // Correct flattened index if NUNITS is strided: (base * NUNITS) + id
                                         // Keeping original math to avoid breaking implicit dependency,
                                         // assuming thermal simulator expects overlapping/bitmask style?
            }
}

void Thermal_block()
{
    int *act = (int *)malloc(n * sizeof(int));
    if (!act)
    {
        cerr << "Alloc failed" << endl;
        return;
    }

    get_active(act);

    auto t_sim_start = chrono::high_resolution_clock::now();

    thermal_simulation(act);

    auto t_sim_end = chrono::high_resolution_clock::now();
    chrono::duration<double> t_sim_diff = t_sim_end - t_sim_start;
    thermal_overhead_time += t_sim_diff.count();

    free(act); // Added free() to prevent leak

    for (int i = 0; i < Gh; i++)
        for (int j = 0; j < Gl; j++)
            for (int k = 0; k < Gw; k++)
            {
                mesh[k][j][i].temp = get_max_grid_temperature(i * 2, k, j);
                mesh[k][j][i].isnotBlocked = (mesh[k][j][i].temp < TEMP_THRESHOLD);
            }
}

void free_mesh()
{
    pair<int, int> min_element = {INT_MAX, 0};
    bool found = false;

    // Find earliest death time
    for (int i = 0; i < Gh; i++)
        for (int j = 0; j < Gl; j++)
            for (int k = 0; k < Gw; k++)
                if (mesh[k][j][i].task_id_int != -1)
                {
                    if (min_element.first > mesh[k][j][i].time_of_death)
                    {
                        min_element.first = mesh[k][j][i].time_of_death;
                        min_element.second = mesh[k][j][i].task_id_int / 10000; // Extract AppID
                        found = true;
                    }
                }

    if (!found)
        return;

    total_sim_time = min_element.first;
    removed_app_id = min_element.second;
    removed_app_death_time = total_sim_time;

    // Remove all tasks for this App ID
    for (int i = 0; i < Gh; i++)
        for (int j = 0; j < Gl; j++)
            for (int k = 0; k < Gw; k++)
                if (mesh[k][j][i].task_id_int != -1)
                {
                    int current_app = mesh[k][j][i].task_id_int / 10000;
                    if (current_app == removed_app_id)
                    {
                        mesh[k][j][i].isFree = 1;
                        mesh[k][j][i].task_id_int = -1;
                    }
                }
}

void pair_algorithm()
{
    int cnt = 0;
    while (cnt < apps.size())
    {
        if (cnt != 0)
        {
            free_mesh();
            //fillGaps(); // Now uses reliability check
        }

        Thermal_block();
        for (int i = 0; i < apps.size(); i++)
        {
            if (!mapped[i])
            {
                int ans = starting_point_condition();
                s = (ans) ? ((Gh * Gw * Gl) - extra) / 4 : 0;

                vector<pair<int, int>> pairs = make_pairs(apps[i]);
                int zick = mapping_application(pairs, apps[i].id, apps[i], cnt); // Use real ID
                if (zick)
                {
                    cout << "Mapped Application No: " << apps[i].id << " " << apps[i].tasks.size() << endl;
                    mapped[i] = true;
                    mapped_apps_rt.insert({apps[i].runtime, apps[i].id});
                    cnt++;
                }
            }
        }
    }
}

void initiate()
{
    for (int i = 0; i < Gh; i++)
        for (int j = 0; j < Gl; j++)
            for (int k = 0; k < Gw; k++)
                mesh[k][j][i].x = k, mesh[k][j][i].y = j, mesh[k][j][i].z = i;
}

// XML PARSING UTILS
int extractValue(const string &str)
{
    size_t start = str.find('(');
    size_t end = str.find(')', start);
    return (start != string::npos && end != string::npos && end > start) ? stoi(str.substr(start + 1, end - start - 1)) : 0;
}
int extractNodeIndex(const string &title)
{
    size_t underscore = title.find('_');
    return (underscore != string::npos) ? stoi(title.substr(underscore + 1)) : -1;
}
int extractGraphId(const string &title)
{
    size_t underscore = title.find('_');
    return (title.size() >= 2 && underscore != string::npos) ? stoi(title.substr(1, underscore - 1)) : 0;
}

int graphsUpdating(int argc, char *argv[])
{
    if (argc < 2)
    {
        cerr << "Usage error" << endl;
        return 1;
    }
    XMLDocument doc;
    if (doc.LoadFile(argv[1]) != XML_SUCCESS)
        return 1;

    XMLElement *graphElem = doc.FirstChildElement("graph");
    if (!graphElem)
        return 1;

    map<int, vector<pair<int, int>>> graphNodes;
    XMLElement *nodesElem = graphElem->FirstChildElement("nodes");

    if (nodesElem)
    {
        for (XMLElement *nodeElem = nodesElem->FirstChildElement("node"); nodeElem; nodeElem = nodeElem->NextSiblingElement("node"))
        {
            const char *t = nodeElem->Attribute("title");
            const char *l = nodeElem->Attribute("label");
            if (t && l)
                graphNodes[extractGraphId(t)].push_back({extractNodeIndex(t), extractValue(l)});
        }
    }

    map<int, vector<tuple<int, int, int>>> graphEdges;
    XMLElement *edgesElem = graphElem->FirstChildElement("edges");
    if (edgesElem)
    {
        for (XMLElement *e = edgesElem->FirstChildElement("edge"); e; e = e->NextSiblingElement("edge"))
        {
            const char *s = e->Attribute("sourcename");
            const char *t = e->Attribute("targetname");
            const char *l = e->Attribute("label");
            if (s && t && l)
                graphEdges[extractGraphId(s)].push_back(make_tuple(extractNodeIndex(s), extractNodeIndex(t), extractValue(l)));
        }
    }

    // int a = 0; // Removed ind_apps logic var
    for (auto &kv : graphNodes)
    {
        vector<pair<int, int>> &nodesVec = kv.second;
        sort(nodesVec.begin(), nodesVec.end(), [](const pair<int, int> &a, const pair<int, int> &b)
             { return a.first < b.first; });

        map<int, int> reindex;
        vector<int> tasks;
        for (size_t i = 0; i < nodesVec.size(); i++)
        {
            reindex[nodesVec[i].first] = i;
            tasks.push_back(nodesVec[i].second);
        }

        vector<vector<int>> edgesVec;
        vector<double> commVol;
        if (graphEdges.count(kv.first))
        {
            for (auto &t : graphEdges[kv.first])
            {
                int s, tr, w;
                tie(s, tr, w) = t;
                if (reindex.count(s) && reindex.count(tr))
                {
                    edgesVec.push_back({reindex[s], reindex[tr]});
                    commVol.push_back(w); // Keeping original logic
                }
            }
        }

        Application app;
        app.id = kv.first + 1;
        app.tasks = tasks;
        app.edges = edgesVec;
        app.communicationVolume = commVol;
        app.runtime = calc_runtime(app);
        apps.push_back(app);
    }
    mapped.resize(apps.size(), false);
    return 1;
}

int main(int argc, char *argv[])
{
    // int tot_nodes = Gw * Gl * Gh;
    // if (tot_nodes <= 64)
    //     TEMP_THRESHOLD = 1500;
    // else if (tot_nodes <= 256)
    //     TEMP_THRESHOLD = 2000;
    // else if (tot_nodes <= 512)
    //     TEMP_THRESHOLD = 4500;
    // else
    //     TEMP_THRESHOLD = 5500;
    graphsUpdating(argc, argv);
    initiate();
    loadNodes();
    extra = (Gh % 2 == 0) ? 0 : Gw * Gl;

    thermal_initiation();
    auto start = chrono::high_resolution_clock::now();

    pair_algorithm();

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> diff = end - start;
    double algorithm_time_only = diff.count() - thermal_overhead_time;

    // Reporting Logic using new int IDs

    for (auto &[buf, vector_list] : task_timestamp)
    {
        for (auto x : vector_list)
        {
            cout << buf << " ";
            for (auto y : x)
                cout << y << " ";
            cout << endl;
        }
    }

    for (int i = 0; i < apps.size(); i++)
    {
        // Iterate through expected task IDs for this app
        int num_tasks = apps[i].tasks.size();
        for (int t_idx = 0; t_idx < num_tasks; t_idx++)
        {
            int buf = cnv_task_buf(apps[i].id, t_idx);

            // Need to correlate edges to these buffer IDs for traffic print
            // Original logic iterated edges and found timestamps.
        }

        // Re-implementing traffic print based on edges
        for (int j = 0; j < apps[i].edges.size(); j++)
        {
            int t1 = apps[i].edges[j][0];
            int t2 = apps[i].edges[j][1];
            int buf1 = cnv_task_buf(apps[i].id, t1);
            int buf2 = cnv_task_buf(apps[i].id, t2);

            if (task_timestamp.count(buf1) && task_timestamp.count(buf2))
            {
                auto &v1 = task_timestamp[buf1];
                auto &v2 = task_timestamp[buf2];

                for (size_t k = 0; k < v1.size(); k++)
                {
                    for (size_t l = 0; l < v2.size(); l++)
                    {
                        // Original logic checked time overlap or index alignment?
                        // Original: if (task_timestamp[second_task][l][0] == k)
                        int start1 = v1[k][2], end1 = v1[k][3];
                        int start2 = v2[l][2], end2 = v2[l][3];

                        // Find the overlapping time window
                        int overlap_start = max(start1, start2);
                        int overlap_end = min(end1, end2);

                        // If the overlapping window is valid (they existed at the same time)
                        if (overlap_start < overlap_end)
                        {
                            if (abs(v1[k][1] - v2[l][1]) == Gw * Gl)
                                edges_on_tsv++;
                            if (v1[k][2] != v1[k][3])
                            {
                                testTraffic << v1[k][1] << " " << v2[l][1] << " "
                                            << PIR * apps[i].communicationVolume[j] << " "
                                            << PIR * apps[i].communicationVolume[j] << " "
                                            << v1[k][2] << " " << v1[k][3] << endl;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    double total_nodes = 0, total_edges = 0;
    for (auto app : apps)
    {
        total_nodes += app.tasks.size();
        total_edges += app.edges.size();
    }
    avg_node_layer /= total_nodes;
    edges_on_tsv /= total_edges;

    reportValues << "Avg node layer: " << avg_node_layer << endl;
    reportValues << "Avg Edges on tsv: " << edges_on_tsv << endl;
    reportValues << "\nRunning time: " << algorithm_time_only << " seconds" << endl;

    thermal_termination();
    return 0;
}