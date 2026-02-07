#include <bits/stdc++.h>
#include <unistd.h>
#include "tinyxml2.h"
#include "thermal_simulator.h"

using namespace std;
using namespace tinyxml2;

// --- CONSTANTS & GLOBALS ---
const int Gw = 16;
const int Gl = 8;
const int Gh = 4;
const double A = 1e5;
const double Ea = 0.7;
const double K = 8.617e-5;
const double t_sim = 1e4;

const int NUNITS = 30;
const double sigmastar_const = 1.15;
const int task_multiplyer = 10000;
int glbmark = 1;
int mnt = 0;
int ctc, ret;

// If 'n' is not defined in thermal_simulator.h, we define it here based on grid size * units
#ifndef THERMAL_SIMULATOR_H
int n = Gw * Gl * Gh * NUNITS;
#endif

ofstream testTraffic;
ofstream mapFile;
ofstream crvalues;

// --- STRUCTURES ---
struct Application
{
    int id;
    vector<int> tasks;
    vector<vector<int>> edges;
    vector<double> commVolume;
    vector<double> task_criticalities;
    int NOL = -1;
    int MD = -1;
    double avgCommValue;
    int xmin = INT_MAX, xmax = INT_MIN, ymin = INT_MAX, ymax = INT_MIN;
    int startX, startY, startZ;
    int placed = 0;
    int run_time = 0;

    void computeAvg()
    {
        if (!commVolume.empty())
            avgCommValue = accumulate(commVolume.begin(), commVolume.end(), 0.0) / commVolume.size();
        else
            avgCommValue = 0.0;
    }

    void computeRuntime()
    {
        if (run_time != 0)
            return;
        for (auto x : tasks)
            run_time += x;
    }
};

struct Core
{
    int isFree = -1;
    int x, y, z;
    int timeofdeath = 0;
    int num_time_task = 0;
    string task_no = "";
    double temperature;
};

Core NoC[Gw][Gl][Gh];
int Pc[Gw][Gl][Gh];

int removed_app_death_time = 0;
int total_sim_time = 0;
std::vector<int> freecores((int)Gh, (int)(Gw * Gl));
using AppIntPair = pair<vector<Application *>, vector<int>>;
vector<Application> tapps;
unordered_map<int, int> task_to_core;
map<int, vector<vector<int>>> task_timestamp;
vector<double> region_reliabilites(Gh);

// --- THERMAL SIMULATION WRAPPERS ---

string shape = to_string(Gw) + "x" + to_string(Gl) + "x" + to_string(Gh);
void thermal_initiation()
{
    static const vector<string> args = {
        "./a.out", "-c", "../NOC/" + shape + "/hotspot.config", "-init_file", "../NOC/" + shape + "/avg.init", "-p",
        "../NOC/" + shape + "/new_core3D.ptrace", "-grid_layer_file", "../NOC/" + shape + "/NoC_layer.lcf", "-model_type",
        "grid", "-detailed_3D", "on", "-o", "../NOC/" + shape + "/avg.ttrace",
        "-grid_transient_file", "../NOC/" + shape + "/avg.grid.ttrace", "-grid_map_mode", "avg"};
    std::vector<char *> argv;
    for (const auto &arg : args)
    {
        // We use const_cast because argv expects `char*`, not `const char*`.
        // This is safe if program_logic doesn't modify the arguments.
        argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr); // Null-terminate the argv array
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
    {
        for (int j = 0; j < Gl; j++)
        {
            for (int k = 0; k < Gw; k++)
            {
                int mf = 1;
                if (NoC[k][j][i].isFree == -1)
                    mf = 0;
                for (int id = 0; id < NUNITS; id++)
                    act[(i * Gw * Gl) + (j * Gw) + k + id] = mf;
            }
        }
    }
}

void temperature_update()
{
    int *act;
    act = (int *)malloc(n * sizeof(int));
    if (act == NULL)
    {
        std::cerr << "unable to allocate memory for 'act' array\n";
    }
    get_active(act);
    thermal_simulation(act);

    free(act);

    for (int i = 0; i < Gh; i++)
    {
        for (int j = 0; j < Gl; j++)
        {
            for (int k = 0; k < Gw; k++)
            {
                NoC[k][j][i].temperature = get_max_grid_temperature(i * 2, k, j);
            }
        }
    }
}
// --- OPTIMIZATION ALGORITHM HELPERS ---

double ERT(int a, double v, int nol, int md)
{
    double c0 = 1.0, c1 = 2.0, c2 = 2.0, c3 = -1.0, c4 = 2.0;
    return (double)c0 + ((double)c1 * a) + ((double)c2 * v) + ((double)c3 * nol) + ((double)c4 * md);
}

double lowerbound_ert(Application app)
{
    int max_task_per_layer = Gw * Gl;
    int nol = (int)(ceil((double)(app.tasks.size()) / (double)max_task_per_layer));
    if (nol == 0)
        nol = 1;
    if (nol > Gh)
    {
        // cerr << "App too large for system" << endl;
        return -1.0;
    }
    int md = 0;
    return ERT(app.tasks.size(), app.avgCommValue, nol, md);
}

void regionReliab()
{
    if (ret == 0)
        return;
    for (int z = Gh - 1; z >= 0; z--)
    {
        double rel = 0;
        int count = 0;
        for (int x = 0; x < Gw; x++)
        {
            for (int y = 0; y < Gl; y++)
            {
                double T = NoC[x][y][z].temperature;
                rel += 1e3 / T;
                count++;
            }
        }
        region_reliabilites[z] = rel / count;
    }
}

// --- LIGHTWEIGHT BEAM SEARCH IMPLEMENTATION ---

struct SearchNode
{
    int app_idx;
    double accumulated_cost;
    std::vector<std::pair<int, int>> decisions;
    std::vector<int> current_free_cores;
    double estimated_total_cost;
};

const int BEAM_WIDTH = 1;

// Helper to reconstruct result
AppIntPair reconstructResult(const std::vector<Application *> &original_apps, const SearchNode &bestNode)
{
    AppIntPair result;
    result.second = bestNode.current_free_cores;

    for (size_t i = 0; i < original_apps.size(); i++)
    {
        Application *newApp = new Application(*original_apps[i]);
        if (i < bestNode.decisions.size())
        {
            newApp->MD = bestNode.decisions[i].first;
            newApp->NOL = bestNode.decisions[i].second;
        }
        result.first.push_back(newApp);
    }
    return result;
}

AppIntPair findCoreRegionShape(std::vector<Application *> &apps)
{
    // Pre-calculate lower bounds
    std::vector<double> lower_bounds(apps.size());
    double initial_lb_sum = 0;

    for (int i = 0; i < apps.size(); i++)
    {
        apps[i]->computeAvg();
        lower_bounds[i] = lowerbound_ert(*apps[i]);
        if (lower_bounds[i] == -1.0)
            continue; // skip invalid
        initial_lb_sum += lower_bounds[i];
    }

    regionReliab();

    std::queue<SearchNode> WQ;
    SearchNode root;
    root.app_idx = 0;
    root.accumulated_cost = 0;
    root.current_free_cores = freecores;
    root.estimated_total_cost = initial_lb_sum;
    WQ.push(root);

    double sigmaStar = initial_lb_sum * sigmastar_const;
    SearchNode bestNode = root;
    bool solutionFound = false;

    while (!WQ.empty())
    {
        // cout << WQ.size() << endl;
        SearchNode current = WQ.front();
        WQ.pop();

        // Pruning Rule 2
        if (current.estimated_total_cost > sigmaStar)
            continue;

        // Leaf Node Check
        if (current.app_idx >= apps.size())
        {
            if (current.accumulated_cost < sigmaStar)
            {
                sigmaStar = current.accumulated_cost;
                bestNode = current;
                solutionFound = true;
            }
            continue;
        }

        // Branching
        int idx = current.app_idx;
        Application *currApp = apps[idx];
        int tasks_count = currApp->tasks.size();

        int nolmin = (tasks_count + (Gw * Gl) - 1) / (Gw * Gl);
        if (nolmin == 0)
            nolmin = 1;
        int nolmax = std::min((int)Gh, tasks_count);

        int mdmin = 0;
        // Optimization: Find first layer with any free space
        for (int k = 0; k < current.current_free_cores.size(); k++)
        {
            if (current.current_free_cores[k] > 0)
            {
                mdmin = k;
                break;
            }
        }
        int mdmax = (int)Gh - nolmin;

        struct Candidate
        {
            int md, nol;
            double cost;
            std::vector<int> updated_cores;
        };
        std::vector<Candidate> candidates;

        for (int i = mdmin; i <= mdmax; i++) // MD loop
        {
            for (int j = nolmin; j <= nolmax; j++) // NOL loop
            {
                if (i + j > (int)Gh)
                    continue;

                // Feasibility Rule 1
                int avgcores = (tasks_count + j - 1) / j;
                bool feasible = true;
                for (int k = i; k < i + j; k++)
                {
                    if (avgcores > current.current_free_cores[k])
                    {
                        feasible = false;
                        break;
                    }
                }
                if (!feasible)
                    continue;

                Candidate cand;
                cand.md = i;
                cand.nol = j;
                cand.updated_cores = current.current_free_cores;

                int temp_tasks = tasks_count;
                for (int k = i; k < i + j; k++)
                {
                    int consume = std::min(avgcores, temp_tasks);
                    cand.updated_cores[k] -= consume;
                    temp_tasks -= consume;
                }

                double this_ert = ERT(tasks_count, currApp->avgCommValue, j, i);
                double future_lb = current.estimated_total_cost - current.accumulated_cost - lower_bounds[idx];
                cand.cost = current.accumulated_cost + this_ert + future_lb;

                if (cand.cost <= sigmaStar)
                    candidates.push_back(cand);
            }
        }

        // Beam Search Sort
        std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b)
                  { return a.cost < b.cost; });

        // Push Top K
        for (size_t k = 0; k < candidates.size() && k < BEAM_WIDTH; k++)
        {
            SearchNode nextNode;
            nextNode.app_idx = current.app_idx + 1;
            nextNode.accumulated_cost = current.accumulated_cost + ERT(tasks_count, currApp->avgCommValue, candidates[k].nol, candidates[k].md);
            nextNode.decisions = current.decisions;
            nextNode.decisions.push_back({candidates[k].md, candidates[k].nol});
            nextNode.current_free_cores = candidates[k].updated_cores;
            nextNode.estimated_total_cost = candidates[k].cost;
            WQ.push(nextNode);
        }
    }

    if (!solutionFound && bestNode.decisions.empty())
        std::cout << "Warning: No feasible mapping found." << std::endl;
    return reconstructResult(apps, bestNode);
}

// --- PLACEMENT AND MAPPING HELPERS ---

pair<vector<Core *>, bool> findCoreRegionLocation(Application &app, int cornerIndex)
{
    vector<Core *> result = {};

    int width = ceil(sqrt(app.tasks.size() / (double)app.NOL));
    int length = ceil(sqrt(app.tasks.size() / (double)app.NOL));

    int startX, startY, startZ, dirX, dirY;
    if (cornerIndex % 4 == 0)
    {
        startX = 0;
        startY = 0;
        startZ = app.MD;
        dirX = 1;
        dirY = 1;
    }
    else if (cornerIndex % 4 == 1)
    {
        startX = Gw - 1;
        startY = 0;
        startZ = app.MD;
        dirX = -1;
        dirY = 1;
    }
    else if (cornerIndex % 4 == 2)
    {
        startX = Gw - 1;
        startY = Gl - 1;
        startZ = app.MD;
        dirX = -1;
        dirY = -1;
    }
    else
    {
        startX = 0;
        startY = Gl - 1;
        startZ = app.MD;
        dirX = 1;
        dirY = -1;
    }

    int available = 0;
    bool inmark = false;

    for (int k = startZ; k <= Gh - app.NOL; k++)
    {
        for (int y = startY; (dirY > 0 ? y < Gl : y >= 0); y += dirY)
        {
            for (int x = startX; (dirX > 0 ? x < Gw : x >= 0); x += dirX)
            {
                available = 0;
                // Check availability
                for (int z = k; z < k + app.NOL; z++)
                {
                    for (int dy = 0; dy < length; ++dy)
                    {
                        for (int dx = 0; dx < width; ++dx)
                        {
                            int nx = x + dx * dirX;
                            int ny = y + dy * dirY;
                            if (nx >= 0 && nx < Gw && ny >= 0 && ny < Gl && NoC[nx][ny][z].isFree == -1)
                            {
                                available++;
                            }
                        }
                    }
                }

                // If enough space found, assign
                if (available >= app.tasks.size())
                {
                    inmark = true;
                    int assigned = 0;
                    for (int z = k; z < k + app.NOL && assigned < app.tasks.size(); z++)
                    {
                        for (int dy = 0; dy < length && assigned < app.tasks.size(); dy++)
                        {
                            for (int dx = 0; dx < width && assigned < app.tasks.size(); dx++)
                            {
                                int nx = x + dx * dirX;
                                int ny = y + dy * dirY;
                                if (nx >= 0 && nx < Gw && ny >= 0 && ny < Gl && NoC[nx][ny][z].isFree == -1)
                                {
                                    NoC[nx][ny][z].isFree = app.id;
                                    NoC[nx][ny][z].task_no = to_string(app.id) + "." + to_string(assigned);
                                    NoC[nx][ny][z].num_time_task = 0;

                                    if (ctc == 0)
                                    {
                                        int jump = total_sim_time;
                                        NoC[nx][ny][z].timeofdeath = total_sim_time + app.run_time;

                                        // FIX: Calculate key using integer math to match the printing loop
                                        // This matches: app_id * 10000 + task_id
                                        int buf = app.id * 10000 + assigned;

                                        task_timestamp[buf].push_back({0, z * (Gw * Gl) + ny * (Gw) + nx, jump, NoC[nx][ny][z].timeofdeath});
                                        if (mapFile.is_open())
                                            mapFile << NoC[nx][ny][z].task_no << " " << "\t" << NoC[nx][ny][z].z << "\t" << NoC[nx][ny][z].y << "\t" << NoC[nx][ny][z].x << "\t" << ((NoC[nx][ny][z].z * Gw * Gl) + (NoC[nx][ny][z].y * Gw) + NoC[nx][ny][z].x) << "\n";
                                    }

                                    app.xmax = max(app.xmax, nx);
                                    app.xmin = min(app.xmin, nx);
                                    app.ymax = max(app.ymax, ny);
                                    app.ymin = min(app.ymin, ny);
                                    result.push_back(&NoC[nx][ny][z]);
                                    assigned++;
                                }
                            }
                        }
                    }
                    app.startX = startX;
                    app.startY = startY;
                    app.startZ = k;
                    break;
                }
            }
            if (inmark)
                break;
        }
        if (inmark)
            break;
    }
    return {result, inmark};
}

void calculate_task_criticalites(Application &app)
{
    if (ctc == 0)
        return;
    vector<vector<pair<int, int>>> adj(app.tasks.size() + 1);

    for (int i = 0; i < app.commVolume.size(); i++)
    {
        if (i < app.edges.size())
            adj[app.edges[i][0]].push_back({app.edges[i][1], (int)app.commVolume[i]});
    }

    vector<double> comm_values(app.tasks.size() + 1);
    double outdegree = 0, commVolume = 0, execTime = 0;

    for (int i = 1; i <= app.tasks.size(); i++)
    {
        outdegree += adj[i].size();
        double buf = 0;
        for (auto x : adj[i])
            buf += x.second;
        comm_values[i - 1] = buf;
        commVolume += buf;
        execTime += app.tasks[i - 1];
    }

    app.task_criticalities.clear();
    app.task_criticalities.resize(app.tasks.size());

    for (int i = 1; i <= app.tasks.size(); i++)
    {
        double out_degree_ratio = (outdegree > 0) ? adj[i].size() / outdegree : 0;
        double comm_volume_ratio = (commVolume > 0) ? comm_values[i - 1] / commVolume : 0;
        double exec_time_ratio = (execTime > 0) ? app.tasks[i - 1] / execTime : 0;
        app.task_criticalities[i - 1] = out_degree_ratio + comm_volume_ratio + exec_time_ratio;
    }
}

int cnv_task_buf(string task_no)
{
    int buf = 0;
    try
    {
        size_t decimal_pos = task_no.find('.');
        if (decimal_pos != string::npos)
        {
            int app_id = stoi(task_no.substr(0, decimal_pos));
            int task_id = stoi(task_no.substr(decimal_pos + 1));
            // Assuming formula used elsewhere
            buf = app_id * 10000 + task_id;
        }
    }
    catch (...)
    {
        return 0;
    }
    return buf;
}

void mapping_tasks(Application &app, vector<Core *> &region)
{
    if (ctc == 0)
        return;

    cout << "Application Id: " << app.id << endl;
    crvalues << "core_id \t core_reliabilities" << endl;
    for (auto buf : region)
        crvalues << ((buf->z * Gw * Gl) + (buf->y * Gw) + buf->x) << " \t " << 1e3 / buf->temperature << endl;
    crvalues << endl;

    sort(region.begin(), region.end(), [](Core *a, Core *b)
         {
        double relA = (a->temperature > 0) ? 1e3 / a->temperature : 0;
        double relB = (b->temperature > 0) ? 1e3 / b->temperature : 0;
        return relA > relB; });

    vector<pair<double, int>> tasks_with_criticality;
    for (int i = 0; i < app.task_criticalities.size(); i++)
        tasks_with_criticality.push_back({app.task_criticalities[i], i});

    sort(tasks_with_criticality.begin(), tasks_with_criticality.end(), greater<pair<double, int>>());

    crvalues << "task_id \t task_criticalities" << endl;
    for (auto x : tasks_with_criticality)
        crvalues << x.second << " \t " << x.first << endl;
    crvalues << endl;

    for (int i = 0; i < app.tasks.size() && i < region.size(); i++)
    {
        region[i]->task_no = to_string(app.id) + "." + to_string(tasks_with_criticality[i].second);
        int jump = total_sim_time;
        region[i]->timeofdeath = total_sim_time + app.run_time;

        int buf = cnv_task_buf(region[i]->task_no);
        task_timestamp[buf].push_back({0, region[i]->z * (Gw * Gl) + region[i]->y * (Gw) + region[i]->x, jump, region[i]->timeofdeath});

        if (mapFile.is_open())
            mapFile << region[i]->task_no << " " << "\t" << region[i]->z << "\t" << region[i]->y << "\t" << region[i]->x << "\t" << ((region[i]->z * Gw * Gl) + (region[i]->y * Gw) + region[i]->x) << "\n";
    }
}

int compute_layer_freecores()
{
    int buf = 0;
    for (int i = 0; i < freecores.size(); i++)
        buf += freecores[i];
    return buf;
}

// --- XML PARSING ---

int extractValue(const string &str)
{
    size_t start = str.find('(');
    size_t end = str.find(')', start);
    if (start != string::npos && end != string::npos && end > start)
        return stoi(str.substr(start + 1, end - start - 1));
    return 0;
}

int extractNodeIndex(const string &title)
{
    size_t underscore = title.find('_');
    if (underscore != string::npos && underscore + 1 < title.size())
        return stoi(title.substr(underscore + 1));
    return -1;
}

int extractGraphId(const string &title)
{
    if (title.size() < 2)
        return 0;
    size_t underscore = title.find('_');
    if (underscore != string::npos)
        return stoi(title.substr(1, underscore - 1));
    return 0;
}

int graphsUpdating(int argc, char *argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " input.xml" << endl;
        return 1;
    }

    const char *filename = argv[1];
    XMLDocument doc;
    if (doc.LoadFile(filename) != XML_SUCCESS)
    {
        cerr << "Error loading XML: " << filename << endl;
        return 1;
    }

    XMLElement *graphElem = doc.FirstChildElement("graph");
    if (!graphElem)
        return 1;

    map<int, vector<pair<int, int>>> graphNodes;
    XMLElement *nodesElem = graphElem->FirstChildElement("nodes");
    if (nodesElem)
    {
        for (XMLElement *nodeElem = nodesElem->FirstChildElement("node"); nodeElem; nodeElem = nodeElem->NextSiblingElement("node"))
        {
            const char *title = nodeElem->Attribute("title");
            const char *label = nodeElem->Attribute("label");
            if (title && label)
            {
                int gid = extractGraphId(title);
                int idx = extractNodeIndex(title);
                int value = extractValue(label);
                graphNodes[gid].push_back({idx, value});
            }
        }
    }

    map<int, vector<tuple<int, int, int>>> graphEdges;
    XMLElement *edgesElem = graphElem->FirstChildElement("edges");
    if (edgesElem)
    {
        for (XMLElement *edgeElem = edgesElem->FirstChildElement("edge"); edgeElem; edgeElem = edgeElem->NextSiblingElement("edge"))
        {
            const char *src = edgeElem->Attribute("sourcename");
            const char *tgt = edgeElem->Attribute("targetname");
            const char *label = edgeElem->Attribute("label");
            if (src && tgt && label)
            {
                int gid = extractGraphId(src);
                int s = extractNodeIndex(src);
                int t = extractNodeIndex(tgt);
                int weight = extractValue(label);
                graphEdges[gid].push_back(make_tuple(s, t, weight));
            }
        }
    }

    for (auto &kv : graphNodes)
    {
        int gid = kv.first;
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
        if (graphEdges.find(gid) != graphEdges.end())
        {
            for (auto &t : graphEdges[gid])
            {
                int origS, origT, weight;
                tie(origS, origT, weight) = t;
                if (reindex.find(origS) != reindex.end() && reindex.find(origT) != reindex.end())
                {
                    edgesVec.push_back({reindex[origS], reindex[origT]});
                    commVol.push_back(weight);
                }
            }
        }

        Application app;
        app.id = gid + 1;
        app.tasks = tasks;
        app.edges = edgesVec;
        app.commVolume = commVol;
        tapps.push_back(app);
    }
    return 1;
}

// --- MAIN ---

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cout << "Usage: ./exe input.xml variant_num" << endl;
        return 0;
    }

    int var = stoi(argv[2]);
    if (var == 1)
    {
        ctc = 0;
        ret = 0;
        testTraffic.open("./mapping/test_traffic_basic.txt");
        mapFile.open("./mapping/mapFile_basic.txt");
        cout << "one\n";
    }
    else if (var == 2)
    {
        ctc = 0;
        ret = 1;
        testTraffic.open("./mapping/test_traffic_reliability.txt");
        mapFile.open("./mapping/mapFile_reliability.txt");
    }
    else if (var == 3)
    {
        ctc = 1;
        ret = 0;
        testTraffic.open("./mapping/test_traffic_criticality.txt");
        mapFile.open("./mapping/mapFile_criticality.txt");
    }
    else if (var == 4)
    {
        ctc = 1;
        ret = 1;
        testTraffic.open("./mapping/test_traffic_combined.txt");
        mapFile.open("./mapping/mapFile_combined.txt");
    }
    mapFile << "tId" << " " << "\t" << "Z" << "\t" << "Y" << "\t" << "X" << "\t" << "core_Id" << endl;
    crvalues.open("./mapping/criticality_reliability_values.txt", std::ios::out | std::ios::trunc);

    graphsUpdating(argc, argv);
    for (int i = 0; i < tapps.size(); i++)
        tapps[i].computeRuntime();

    thermal_initiation();

    // Init NoC
    for (int i = 0; i < Gh; i++)
        for (int j = 0; j < Gw; j++)
            for (int k = 0; k < Gl; k++)
            {
                Pc[j][k][i] = 1; // Corrected Indexing? Typically [x][y][z] or [w][l][h]
                NoC[j][k][i].x = j;
                NoC[j][k][i].y = k;
                NoC[j][k][i].z = i;
            }

    for (auto &app : tapps)
    {
        app.computeAvg();
        app.computeRuntime();
    }

    map<int, Application *> application_map;
    for (int i = 0; i < tapps.size(); i++)
        application_map[tapps[i].id] = &tapps[i];

    vector<Application *> active_apps;
    auto start = chrono::high_resolution_clock::now();

    int count = 0, prev_count = count;

    cout << tapps.size() << " total apps" << endl;

    while (count < tapps.size())
    {
        // 1. SELECT APPS TO SCHEDULE
        vector<Application *> apps_to_schedule;
        int avaliablecores = compute_layer_freecores();
        cout << avaliablecores << " cores available" << endl;

        temperature_update();

        int sz = 0;
        for (int i = 0; i < tapps.size(); i++)
        {
            if (!tapps[i].placed && sz + tapps[i].tasks.size() < avaliablecores)
            {
                sz += tapps[i].tasks.size();
                apps_to_schedule.push_back(&tapps[i]);
            }
        }
        cout << apps_to_schedule.size() << " apps selected for optimization" << endl;

        // 2. OPTIMIZATION STEP
        // Pass pointers to ORIGINAL tapps
        AppIntPair regionShape = findCoreRegionShape(apps_to_schedule);

        // 3. COPY DECISIONS BACK TO ORIGINAL OBJECTS AND CLEANUP
        for (size_t i = 0; i < regionShape.first.size(); i++)
        {
            // We copy the found shape decisions back to the original objects in our schedule list
            if (i < apps_to_schedule.size())
            {
                apps_to_schedule[i]->MD = regionShape.first[i]->MD;
                apps_to_schedule[i]->NOL = regionShape.first[i]->NOL;
            }
            // Delete the temporary copy created by reconstructResult
            delete regionShape.first[i];
        }

        // Update the freecores vector with the state from the optimization
        freecores = regionShape.second;

        cout << "Dimensions found:" << endl;
        for (auto *app : apps_to_schedule)
            cout << app->id << " MD:" << app->MD << " NOL:" << app->NOL << endl;

        // 4. PLACEMENT STEP
        for (int i = 0; i < Gh; i++)
        {
            int cornerIndex = 0;
            for (auto *app : apps_to_schedule)
            {
                if (app->MD == i && !app->placed)
                {
                    bool possible = true;
                    pair<vector<Core *>, bool> ans;
                    for (int x = 1; x <= 4; x++)
                    {
                        ans = findCoreRegionLocation(*app, cornerIndex % 4);
                        possible = ans.second;
                        if (!possible)
                            cornerIndex++;
                        else
                            break;
                    }

                    if (!possible)
                    {
                        app->placed = 0;
                    }
                    else
                    {
                        app->placed = 1; // Updates ORIGINAL tapps
                        calculate_task_criticalites(*app);
                        mapping_tasks(*app, ans.first);
                    }
                    cornerIndex++;
                }
            }
        }
        cout << count << "count\n";

        // 5. UPDATE ACTIVE APPS LIST
        for (auto *app : apps_to_schedule)
        {
            if (app->placed)
            {
                // Avoid adding duplicates if logic runs multiple times
                bool found = false;
                for (auto *existing : active_apps)
                    if (existing->id == app->id)
                        found = true;
                if (!found)
                {
                    active_apps.push_back(app);
                    count++;
                }
            }
        }

        // 6. SIMULATION STEP / TIME ADVANCEMENT
        if (count != prev_count)
        {
            prev_count = count;
        }
        else
        {
            int minRT_applicaiton_id = -1, minRT = INT_MAX;
            for (int z = 0; z < Gh; ++z)
                for (int y = 0; y < Gl; ++y)
                    for (int x = 0; x < Gw; ++x)
                        if (NoC[x][y][z].isFree != -1)
                        {
                            if (minRT > NoC[x][y][z].timeofdeath)
                            {
                                minRT_applicaiton_id = NoC[x][y][z].isFree;
                                minRT = tapps[minRT_applicaiton_id - 1].run_time; // Careful with index 0 vs 1
                            }
                        }

            // Advance time
            int step = (minRT_applicaiton_id == -1 ? 0 : minRT);
            // If gridlock (no apps running but tasks remain), break or handle
            if (active_apps.empty() && count < tapps.size() && step == 0 && minRT == INT_MAX)
            {
                cout << "Gridlock detected. No active apps, but tasks remain." << endl;
                break;
            }
            if (step == INT_MAX)
                step = 100; // default step if nothing running

            total_sim_time += step;
            removed_app_death_time = total_sim_time;

            // Free up cores
            for (int z = 0; z < Gh; ++z)
                for (int y = 0; y < Gl; ++y)
                    for (int x = 0; x < Gw; ++x)
                        if (NoC[x][y][z].isFree == minRT_applicaiton_id)
                            NoC[x][y][z].isFree = -1;

            for (size_t i = 0; i < active_apps.size(); i++)
            {
                if (active_apps[i]->id == minRT_applicaiton_id)
                {
                    active_apps.erase(active_apps.begin() + i);
                    break;
                }
            }
        }

        // 7. RECALCULATE FREE CORES FOR NEXT ITERATION
        for (int z = 0; z < Gh; ++z)
        {
            int going_to_be_free = 0;
            for (int y = 0; y < Gl; ++y)
                for (int x = 0; x < Gw; ++x)
                    if (NoC[x][y][z].isFree == -1)
                        going_to_be_free++;
            freecores[z] = going_to_be_free;
        }

        cout << "After NoC is located" << endl;
        for (int z = 0; z < Gh; ++z)
        {
            cout << "Layer " << z << ":" << endl;
            for (int y = 0; y < Gl; ++y)
            {
                for (int x = 0; x < Gw; ++x)
                {
                    cout << NoC[x][y][z].isFree << " ";
                }
                cout << endl;
            }
            cout << endl;
        }
    }

    // --- REPORTING ---
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
    for (int i = 0; i < tapps.size(); i++)
    {
        int buf = (tapps[i].id) * 10000;
        int noof_changes = task_timestamp[buf].size();
        for (int j = 0; j < tapps[i].edges.size(); j++)
        {
            int first_task = buf + tapps[i].edges[j][0];
            int second_task = buf + tapps[i].edges[j][1];
            for (int k = 0; k < noof_changes; k++)
            {
                for (int l = 0; l < noof_changes; l++)
                {
                    if (task_timestamp[second_task][l][0] == k)
                    {
                        testTraffic << task_timestamp[first_task][k][1] << " " << task_timestamp[second_task][l][1] << " " << 0.01 * tapps[i].commVolume[j] << " " << 0.01 * tapps[i].commVolume[j] << " " << task_timestamp[first_task][k][2] << " " << task_timestamp[first_task][k][3] << endl;
                        break;
                    }
                }
            }
        }
    }

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> diff = end - start;
    // if (mapFile.is_open())
    //     mapFile << "Running time: " << diff.count() << " seconds" << endl;

    thermal_termination();
    return 0;
}