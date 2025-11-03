#include <bits/stdc++.h>
#include <unistd.h>
#include "tinyxml2.h"
#include "thermal_simulator.h"

using namespace std;
using namespace tinyxml2;

const int Gw = 4;
const int Gl = 4;
const int Gh = 4;

const double A = 1e9;
const double Ea = 0.7;
const double K = 8.617e-5;
const double t_sim = 1e6;

const int NUNITS = 30; // These units are represent the no of components of everynode which is used in the thermal simulation
const double sigmastar_const = 1.15;
const int task_multiplyer = 10000;
int glbmark = 1;
int mnt = 0;

ofstream testTraffic("./mapping/test_traffic3.txt");

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
        {
            avgCommValue = accumulate(commVolume.begin(), commVolume.end(), 0.0) / commVolume.size();
        }
        else
        {
            avgCommValue = 0.0; // Handle empty vector case
        }
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
/*
Algorithm 1: Finding shape of core region for each
application with reliability of cores as a factor
Input: S: the set of unmapped applications
Output: M Di and N OLi: The MD and NOL values
of each application Ai; BestCuboid: final
chosen cuboid
1 Var: W Q: A working queue, initialized to be empty;
2 N BN : newly branched tree node; BN : best tree node
during search;
3 ˆs(S): minimum overall running time over all tree
nodes searched so far;
4 R(BN ): reliability score of the best node so far;
5 while W Q is not empty do
6 pop the top tree node Nq out of W Q;
7 if Nq is not a leaf node then
8 branch new tree nodes;
9 foreach newly branched tree node N BN do
10 if N BN does not meet the cutting rules
then
11 push N BN into W Q;
12 else
13 runtimeq ← total runtime(Nq );
14 reliabilityq ← aggregate reliability(Nq );
15 if (runtimeq < ˆs(S)) or (runtimeq = ˆs(S)
and reliabilityq > RelScore(BN )) then
16 ˆs(S) ← runtimeq ;
17 R(BN ) ← reliabilityq ;
18 BN ← Nq ;
19 BestCuboid ← BN ;
*/

/*
Algorithm 2: Task Criticality Ranking (with Execution
Time)
    Input: DAG of tasks
    Output: Tasks ranked by criticality score
    1 T otOutDeg ← 0;
    2 T otCommV ol ← 0;
    3 T otExecT ime ← 0;
    4 foreach task Ti in taskList do
    5 ExecT ime ← getExecutionTime(Ti);
    6 outDeg ← getOutDegree(Ti);
    7 CommV ol ← getTotalOutgoingCommVolume(Ti);
    8 T otOutDeg+ = outDeg;
    9 T otCommV ol+ = CommV ol;
    10 T otExecT ime+ = ExecT ime;
    11 store (ExecT ime, outDeg, CommV ol) in Ti
    metadata;
    12 foreach task Ti in taskList do
    13 OutDegRatioi ← outDeg(Ti)
    T otOutDeg ;
    14 ExecRatioi ← ExecT ime(Ti)
    T otExecT ime ;
    15 CommRatioi ← CommV ol(Ti)
    T otCommV ol ;
    16 CriticalityScorei ←
    OutDegRatioi + ExecRatioi + CommRatioi;
    17 Sort taskList in descending order of
    CriticalityScorei;
    18 return ranked taskList;
*/

/*
Algorithm 3: Task to Core Mapping
Input : Set of tasks T = {T1, T2, . . . , Tn} (sorted by
descending criticality score),
Set of cores C = {C1, C2, . . . , Cm} with m ≥ n
(sorted by descending reliability score)
Output : Mapping of each task Tj ∈ T to a unique core
Ci ∈ C
Initialize all Ci ∈ C as unassigned
for j = 1 to n do
Pick the highest ranked unmapped core Ci from C
Assign task Tj to core Ci
Mark Ci as mapped
return mapping of each Tj to Ci
*/

void thermal_initiation()
{
    const vector<string> args = {
        "./a.out", "-c", "./hotspot.config", "-init_file", "avg.init", "-p",
        "new_core3D.ptrace", "-grid_layer_file", "NoC_layer.lcf", "-model_type",
        "grid", "-detailed_3D", "on", "-o", "avg.ttrace",
        "-grid_transient_file", "avg.grid.ttrace", "-grid_map_mode", "avg"};
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

double ERT(int a, double v, int nol, int md)
{
    // double c0 = 5.0, c1 = 1.0, c2 = 3.0, c3 = 8.0, c4 = 2.0;
    double c0 = 1.0, c1 = 2.0, c2 = 2.0, c3 = -1.0, c4 = -1.0;
    double ans = (double)c0 + ((double)c1 * a) + ((double)c2 * v) + ((double)c3 * nol) + ((double)c4 * md);
    return ans;
}

int isnotLeaf(vector<Application> &apps)
{
    int t = 0;
    for (auto &app : apps)
    {
        if (app.NOL == -1 || app.MD == -1)
            t = 1;
    }
    return t;
}

double lowerbound_ert(Application app)
{
    int max_task_per_layer = Gw * Gl;
    int nol = (int)(ceil((double)(app.tasks.size() / max_task_per_layer))) + 1;
    if (nol > Gh)
    {
        std::cerr << "Application with " << app.tasks.size() << " tasks requires "
                  << nol << " layers, which exceeds the available height " << Gh << ". Skipping.\n";
        return -1.0;
    }
    int md = 0;
    double ert = ERT(app.tasks.size(), app.avgCommValue, nol, md);
    return ert;
    // std::cout << "Application (" << app.tasks.size()  << " tasks, avg_comm = "
    //           << app.avgCommValue << "): NOL_min = " << nol
    //           << ", MD_min = " << md << ", ERT = " << ert << "\n";
}

double computeSigmaStar(const std::vector<Application> &apps)
{
    double sigmaStar = 0;
    for (const auto app : apps)
    {
        double ert = lowerbound_ert(app);
        if (ert == -1.0)
            continue;
        sigmaStar += ert;
    }
    return sigmaStar;
}

int cnv_task_buf(string task_no)
{
    int buf = 0;
    stringstream ss(task_no);
    string buff_t;
    int divd = 10000;
    while (getline(ss, buff_t, '.'))
        buf += stoi(buff_t) * divd, divd /= 10000;
    return buf;
}

void regionReliab()
{
    double rel = 0;
    int count = 0;

    for (int z = Gh - 1; z >= 0; z--)
    {
        for (int x = 0; x < Gw; x++)
        {
            for (int y = 0; y < Gl; y++)
            {

                double T = NoC[x][y][z].temperature;
                double lambda = A * exp(-Ea / (K * T));
                rel += exp(-lambda * t_sim);
                count++;
            }
        }
        region_reliabilites[z] = rel / count;
    }
}

AppIntPair findCoreRegionShape(vector<Application *> &apps)
{
    // 1. Create a local vector of Application OBJECTS (copies)
    using AppObjectIntPair = pair<vector<Application>, vector<int>>;

    vector<Application> app_copies;
    for (auto *app_ptr : apps)
    {
        app_copies.push_back(*app_ptr); // Dereference to copy the object
    }

    queue<AppObjectIntPair> WQ;
    AppObjectIntPair NBN, BN, LBN;
    NBN.first = app_copies;
    NBN.second = freecores;
    double BN_reliability = INT_MIN;
    double sigmaStar = computeSigmaStar(NBN.first) * sigmastar_const;
    double leafNode_sigmastar = INT_MAX, non_leaf_sigmastar = INT_MAX;
    int x = 0;
    WQ.push(NBN);
    int leafnode_marker = 1;
    int marker = 1;
    while (WQ.size() != 0)
    {
        AppObjectIntPair Nq = WQ.front();
        WQ.pop();
        int x = 0;
        for (int i = 0; i < Nq.first.size(); i++)
        {
            if (Nq.first[i].NOL == -1)
            {
                x = i;
                break;
            }
        }
        if (isnotLeaf(Nq.first))
        {
            vector<Application> buf = Nq.first;
            vector<int> buf2 = Nq.second;
            int nolmin = ((buf[x].tasks.size()) / (Gw * Gl)) + 1;
            int nolmax = min(Gh, (int)(buf[x].tasks.size()));
            int mdmin = 0;
            for (int i = 0; i < buf2.size(); i++)
            {
                if (buf2[i] != 0)
                {
                    mdmin = i;
                    break;
                }
            }
            int mdmax = (int)Gh - nolmin;
            for (int i = mdmin; i <= mdmax; i++)
            {
                for (int j = nolmin; j <= nolmax; j++)
                {
                    buf[x].MD = i;
                    buf[x].NOL = j;
                    int feasible = 1;
                    if (i + j > (int)Gh)
                    {
                        feasible = 0;
                    }
                    int avgcores = buf[x].tasks.size() / j;
                    for (int k = i; k < i + j && k <= Gh; k++)
                    {
                        if (avgcores > buf2[k])
                        {
                            feasible = 0;
                            break;
                        }
                    }
                    int tempfeasible = feasible;
                    int sigmaMin = 0;
                    for (int k = 0; k < x; k++)
                    {
                        sigmaMin += ERT(buf[k].tasks.size(), buf[k].avgCommValue, buf[k].NOL, buf[k].MD);
                    }
                    sigmaMin += ERT(buf[x].tasks.size(), buf[i].avgCommValue, j, i);
                    for (int k = x + 1; k < Nq.first.size(); k++)
                    {
                        double ert = lowerbound_ert(Nq.first[k]);
                        sigmaMin += ert;
                    }
                    if (sigmaMin >= sigmaStar)
                        feasible = 0;
                    // cout << feasible << " feasible" << endl;
                    // if (x == 0)
                    // {
                    //     cout << buf[x].MD << " " << feasible << " feasible " << endl;
                    // }
                    // if (feasible == 0 && mnt == 0)
                    // {
                    //     for (auto x : buf2)
                    //         cout << x << " ";
                    //     cout << endl;
                    //     cout << buf[x].id << " " << tempfeasible << endl;
                    //     if (buf[x].id)
                    //     {
                    //         cout << sigmaMin << " " << sigmaStar << " sigma" << endl;
                    //     }
                    //     for (auto zick : Nq.first)
                    //     {
                    //         cout << zick.id << " " << zick.MD << " " << zick.NOL << endl;
                    //     }
                    //     cout << endl;
                    // }
                    if (feasible)
                    {
                        for (int k = i; k < i + j && k <= Gh; k++)
                        {
                            int num_tasks = buf[x].tasks.size();
                            buf2[k] -= min(avgcores, num_tasks);
                            num_tasks -= min(avgcores, num_tasks);
                        }
                        pair<vector<Application>, vector<int>> temp;
                        temp.first = buf;
                        temp.second = buf2;
                        WQ.push(temp);
                        for (int k = i; k < i + j && k <= Gh; k++)
                        {
                            int num_tasks = buf[x].tasks.size();
                            buf2[k] += min(avgcores, num_tasks);
                            num_tasks -= min(avgcores, num_tasks);
                        }
                    }
                }
            }
        }
        else
        {
            double sigma = 0, recent_reliability = 0;
            for (auto app : Nq.first)
            {
                sigma += ERT(app.tasks.size(), app.avgCommValue, app.NOL, app.MD);
                recent_reliability += region_reliabilites[app.MD];
            }
            if ((sigma < sigmaStar) || (sigma == sigmaStar && recent_reliability > BN_reliability))
            {
                sigmaStar = sigma;
                BN = Nq;
                BN_reliability = recent_reliability;
                marker = 0;
            }
            if ((sigma < leafNode_sigmastar) || (sigma == leafNode_sigmastar && recent_reliability > BN_reliability))
            {
                leafNode_sigmastar = sigma;
                BN_reliability = recent_reliability;
                LBN = Nq;
            }
            if (!BN.first.empty() || !LBN.first.empty())
                leafnode_marker = 0;
        }
        if (marker)
            BN = LBN;
        if (leafnode_marker)
        {
            double sigma = 0, recent_reliability = 0;
            for (auto app : Nq.first)
            {
                sigma += ERT(app.tasks.size(), app.avgCommValue, app.NOL, app.MD);
                if (app.MD != -1)
                    recent_reliability += region_reliabilites[app.MD];
            }
            if ((sigma < non_leaf_sigmastar) || (sigma == non_leaf_sigmastar && recent_reliability > BN_reliability))
            {
                non_leaf_sigmastar = sigma;
                BN_reliability = recent_reliability;
                BN = Nq;
            }
        }
    }
    AppIntPair final_result;
    final_result.second = BN.second;

    for (const auto &app_obj : BN.first)
        final_result.first.push_back(new Application(app_obj));

    // The caller (main) is now responsible for:
    // 1. Reading the MD/NOL from the pointers in final_result.first
    // 2. 'delete'-ing each pointer in final_result.first

    return final_result;
}

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
    int zmark = startZ;
    bool inmark = false;
    for (int k = startZ; k <= Gh - app.NOL; k++)
    {
        for (int y = startY; (dirY > 0 ? y < Gl : y >= 0); y += dirY)
        {
            for (int x = startX; (dirX > 0 ? x < Gw : x >= 0); x += dirX)
            {
                available = 0;
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
                if (available >= app.tasks.size()) //
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
                                    int jump = total_sim_time;
                                    NoC[nx][ny][z].timeofdeath = total_sim_time + app.run_time;
                                    int buf = cnv_task_buf(NoC[nx][ny][z].task_no);
                                    task_timestamp[buf].push_back({0, z * (Gw * Gl) + ny * (Gw) + nx, jump, NoC[nx][ny][z].timeofdeath});

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
    // return inmark ? true : false;
    return {result, inmark};
}

void calculate_task_criticalites(Application &app)
{
    vector<vector<pair<int, int>>> adj(app.tasks.size() + 1);
    for (auto x : app.edges)
        adj[x[0]].push_back({x[1], x[2]});

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
        double out_degree_ratio = adj[i].size() / outdegree;
        double comm_volume_ratio = comm_values[i - 1] / commVolume;
        double exec_time_ratio = app.tasks[i - 1] / execTime;
        app.task_criticalities[i - 1] = out_degree_ratio + comm_volume_ratio + exec_time_ratio;
    }
}

void mapping_tasks(Application &app, vector<Core *> &region)
{

    sort(region.begin(), region.end(), [](Core *a, Core *b)
         {
        double relA = exp(-A*exp(-Ea/(K*a->temperature))*t_sim);
        double relB = exp(-A*exp(-Ea/(K*b->temperature))*t_sim);
        return relA > relB; });
    vector<pair<int, int>> tasks_with_criticality;
    for (int i = 0; i < app.task_criticalities.size(); i++)
        tasks_with_criticality.push_back({app.task_criticalities[i], i});
    sort(tasks_with_criticality.begin(), tasks_with_criticality.end());
    for (int i = 0; i < app.tasks.size(); i++)
    {
        region[i]->task_no = to_string(app.id) + "." + to_string(tasks_with_criticality[i].second);
    }
}

int compute_layer_freecores()
{
    int buf = 0;
    for (int i = 0; i < freecores.size(); i++)
        buf += freecores[i];
    return buf;
}

//======================================================================================================================================
//======================================================================================================================================
//======================================================================================================================================

int extractValue(const string &str)
{
    size_t start = str.find('(');
    size_t end = str.find(')', start);
    if (start != string::npos && end != string::npos && end > start)
    {
        return stoi(str.substr(start + 1, end - start - 1));
    }
    return 0;
}

// Helper: extract node index from title "tX_Y" (returns Y).
int extractNodeIndex(const string &title)
{
    size_t underscore = title.find('_');
    if (underscore != string::npos && underscore + 1 < title.size())
    {
        return stoi(title.substr(underscore + 1));
    }
    return -1;
}

// Helper: extract graph id from title "tX_Y" (returns X, 0-based).
int extractGraphId(const string &title)
{
    if (title.size() < 2)
        return 0;
    size_t underscore = title.find('_');
    if (underscore != string::npos)
    {
        return stoi(title.substr(1, underscore - 1));
    }
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
        cerr << "Error loading XML file: " << filename << endl;
        return 1;
    }

    // Assume XML has a <graph> element with <nodes> and <edges> children.
    XMLElement *graphElem = doc.FirstChildElement("graph");
    if (!graphElem)
    {
        cerr << "No <graph> element found." << endl;
        return 1;
    }

    // We'll group nodes by graph id.
    // Map: graph id -> vector of pairs (original node index, node value)
    map<int, vector<pair<int, int>>> graphNodes;

    XMLElement *nodesElem = graphElem->FirstChildElement("nodes");
    if (!nodesElem)
    {
        cerr << "No <nodes> element found." << endl;
        return 1;
    }

    for (XMLElement *nodeElem = nodesElem->FirstChildElement("node");
         nodeElem;
         nodeElem = nodeElem->NextSiblingElement("node"))
    {
        const char *titleAttr = nodeElem->Attribute("title");
        const char *labelAttr = nodeElem->Attribute("label");
        if (titleAttr && labelAttr)
        {
            string title(titleAttr);
            string label(labelAttr);
            int gid = extractGraphId(title); // 0-based graph id
            int idx = extractNodeIndex(title);
            int value = extractValue(label);
            graphNodes[gid].push_back({idx, value});
        }
    }

    // Group edges by graph id.
    // Map: graph id -> vector of tuple (source index, target index, weight)
    map<int, vector<tuple<int, int, int>>> graphEdges;

    XMLElement *edgesElem = graphElem->FirstChildElement("edges");
    if (!edgesElem)
    {
        cerr << "No <edges> element found." << endl;
        return 1;
    }

    for (XMLElement *edgeElem = edgesElem->FirstChildElement("edge");
         edgeElem;
         edgeElem = edgeElem->NextSiblingElement("edge"))
    {
        const char *srcAttr = edgeElem->Attribute("sourcename");
        const char *tgtAttr = edgeElem->Attribute("targetname");
        const char *labelAttr = edgeElem->Attribute("label");
        if (srcAttr && tgtAttr && labelAttr)
        {
            string src(srcAttr), tgt(tgtAttr), label(labelAttr);
            int gid = extractGraphId(src); // assume both source and target are from same graph
            int s = extractNodeIndex(src);
            int t = extractNodeIndex(tgt);
            int weight = extractValue(label);
            graphEdges[gid].push_back(make_tuple(s, t, weight));
        }
    }

    // Create a vector of Application structures.
    // For each graph group, re-index nodes and build Application.
    int a = 0;
    for (auto &kv : graphNodes)
    {
        int gid = kv.first; // 0-based
        vector<pair<int, int>> &nodesVec = kv.second;
        // Sort nodes by their original index.
        sort(nodesVec.begin(), nodesVec.end(), [](const pair<int, int> &a, const pair<int, int> &b)
             { return a.first < b.first; });

        // Build a mapping from original index to new index (0 to n-1).
        map<int, int> reindex;
        vector<int> tasks;
        vector<int> ind_tasks;
        vector<vector<int>> ind_edgesVec;
        for (size_t i = 0; i < nodesVec.size(); i++)
        {
            reindex[nodesVec[i].first] = i;
            tasks.push_back(nodesVec[i].second);
            ind_tasks.push_back(a);
            a++;
        }

        // Process edges for this graph.
        vector<vector<int>> edgesVec;
        vector<double> commVol;
        if (graphEdges.find(gid) != graphEdges.end())
        {
            for (auto &t : graphEdges[gid])
            {
                int origS, origT, weight;
                tie(origS, origT, weight) = t;
                // Only add edge if both nodes exist in the reindex map.
                if (reindex.find(origS) != reindex.end() && reindex.find(origT) != reindex.end())
                {
                    int newS = reindex[origS];
                    int newT = reindex[origT];
                    edgesVec.push_back({newS, newT});
                    ind_edgesVec.push_back({ind_tasks[newS], ind_tasks[newT]});
                    // For communicationVolume, here we simply use the weight.
                    commVol.push_back(weight);
                }
            }
        }

        // Create the Application object.
        Application app;
        app.id = gid + 1; // convert to 1-based id
        app.tasks = tasks;
        app.edges = edgesVec;
        app.commVolume = commVol;
        // app.runtime = calc_runtime(app);

        // Application ind_app;
        // ind_app.id = gid + 1;
        // ind_app.tasks = ind_tasks;
        // ind_app.edges = ind_edgesVec;
        // ind_app.communicationVolume = commVol;

        tapps.push_back(app);
    }
    // mapped.resize(apps.size(), false);

    // For demonstration, print out the applications in the requested format.
    // (You can remove the printing if you need to use the apps vector directly.)
    for (auto &app : tapps)
    {
        cout << "{" << app.id << ",\n";
        // Print tasks vector
        cout << "{";
        for (size_t i = 0; i < app.tasks.size(); i++)
        {
            cout << app.tasks[i];
            if (i != app.tasks.size() - 1)
                cout << ", ";
        }
        cout << "},\n";
        // Print edges vector (each edge as {source, target, weight})
        cout << "{";
        for (size_t i = 0; i < app.edges.size(); i++)
        {
            cout << "{";
            for (size_t j = 0; j < app.edges[i].size(); j++)
            {
                cout << app.edges[i][j];
                if (j != app.edges[i].size() - 1)
                    cout << ", ";
            }
            cout << "}";
            if (i != app.edges.size() - 1)
                cout << ", ";
        }
        cout << "},\n";
        // Print communicationVolume vector
        cout << "{";
        for (size_t i = 0; i < app.commVolume.size(); i++)
        {
            cout << app.commVolume[i];
            if (i != app.commVolume.size() - 1)
                cout << ", ";
        }
        cout << "}\n";
        cout << "}\n";
        cout << "\n";
    }
    cout << "runtimes" << endl;
    for (auto x : tapps)
        cout << x.run_time << " ";
    cout << endl;

    return 1;
}

int main(int argc, char *argv[])
{
    // vector<Application>
    // tapps = {
    //     {1, {1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5}, {{1, 2, 4}, {1, 3, 4}, {2, 4, 4}, {4, 5, 4}}, {3, 4, 5, 3}},
    //     {2, {3, 4, 2, 3, 1, 3, 4, 2, 3, 1}, {{2, 4, 3}, {3, 2, 4}, {5, 2, 3}}, {6, 2, 2}},
    //     {3, {2, 3, 9, 1}, {{3, 4, 1}, {1, 4, 8}}, {4, 9}},
    //     {4, {1, 5, 3, 3, 4}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}},
    //     {5, {1, 5, 3}, {{1, 2, 4}}, {2}},
    //     {6, {1, 5, 3, 3}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}},
    //     {7, {1, 2}, {{1, 2, 6}}, {2}},
    //     {8, {1, 2, 3, 4, 5, 6}, {{1, 3, 4}}, {2}}};
    /*{4, {1, 5, 3, 3, 4}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}}*/
    graphsUpdating(argc, argv);
    thermal_initiation(); // thermalrelated
    for (int i = 0; i < Gh; i++)
    {
        for (int j = 0; j < Gw; j++)
        {
            for (int k = 0; k < Gl; k++)
            {
                Pc[i][j][k] = 1;
                NoC[k][j][i].x = k;
                NoC[k][j][i].y = j;
                NoC[k][j][i].z = i;
            }
        }
    }
    for (auto app : tapps)
    {
        app.computeAvg();
        app.computeRuntime();
    }
    map<int, Application *> application_map;
    for (int i = 0; i < tapps.size(); i++)
        application_map[tapps[i].id] = &tapps[i];
    vector<Application *> active_apps;

    int count = 0, prev_count = count;
    while (count < tapps.size())
    {
        vector<Application *> apps;
        int avaliablecores = compute_layer_freecores();

        temperature_update(); // thermalrelated
        int sz = 0;
        for (int i = 0; i < tapps.size(); i++)
        {
            if (!tapps[i].placed && sz + tapps[i].tasks.size() < avaliablecores)
            {
                sz += tapps[i].tasks.size();
                apps.push_back(&tapps[i]);
            }
        }
        cout << apps.size() << " size" << endl;
        for (auto x : apps)
        {
            x->computeRuntime();
        }

        AppIntPair regionShape = findCoreRegionShape(apps);
        apps = regionShape.first;
        freecores = regionShape.second;

        cout << "Dimensions" << endl;
        for (auto *app : apps)
        {
            cout << app->id << " " << app->MD << " " << app->NOL << endl;
        }
        vector<pair<int, int>> dims(apps.size());
        for (int i = 0; i < apps.size(); i++)
        {
            dims[i].first = apps[i]->MD;
            dims[i].second = apps[i]->NOL;
        }
        for (int i = 0; i < Gh; i++)
        {
            int cornerIndex = 0;
            for (auto &app : apps)
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
                    if (possible == false)
                        app->placed = 0;
                    else
                    {
                        app->placed = 1;
                        calculate_task_criticalites(*app);
                        mapping_tasks(*app, ans.first);
                    }
                    cornerIndex++;
                }
            }
        }
        for (int i = 0; i < apps.size(); i++)
        {

            if (apps[i]->placed)
            {
                application_map[apps[i]->id]->placed = 1;
                active_apps.push_back(apps[i]);
                count++;
            }
        }
        if (count != prev_count)
            prev_count = count;
        else
        {
            int minRT_applicaiton_id = -1, minRT = INT_MAX;
            for (int z = 0; z < Gh; ++z)
            {
                for (int y = 0; y < Gl; ++y)
                {
                    for (int x = 0; x < Gw; ++x)
                    {
                        if (NoC[x][y][z].isFree != -1)
                        {
                            if (minRT > NoC[x][y][z].timeofdeath)
                            {
                                minRT_applicaiton_id = NoC[x][y][z].isFree;
                                minRT = NoC[x][y][z].timeofdeath;
                            }
                        }
                    }
                }
            }
            total_sim_time += minRT;
            removed_app_death_time = total_sim_time;
            for (int z = 0; z < Gh; ++z)
            {
                int going_to_be_free = 0;
                for (int y = 0; y < Gl; ++y)
                {
                    for (int x = 0; x < Gw; ++x)
                    {
                        if (NoC[x][y][z].isFree == minRT_applicaiton_id)
                            NoC[x][y][z].isFree = -1;

                        if (NoC[x][y][z].isFree == -1)
                            going_to_be_free++;
                    }
                }
                freecores[z] = going_to_be_free;
            }
            for (int i = 0; i < active_apps.size(); i++)
            {
                if (active_apps[i]->id == minRT_applicaiton_id)
                {
                    active_apps.erase(active_apps.begin() + i);
                    break;
                }
            }
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
        cout << "=================================================================================================" << endl;
        // mnt++;
        // if (mnt == 1)
        //     break;
    }
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
                        testTraffic << task_timestamp[first_task][k][1] << " " << task_timestamp[second_task][l][1] << " " << "0.01" << " " << "0.01" << " " << task_timestamp[first_task][k][2] << " " << task_timestamp[first_task][k][3] << endl;
                        break;
                    }
                }
            }
        }
    }
    thermal_termination(); // thermalrelated
    return 0;
}
