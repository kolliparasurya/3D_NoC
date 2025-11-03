#include <bits/stdc++.h>
#include <unistd.h>
#include "tinyxml2.h"

using namespace std;
using namespace tinyxml2;

const int Gw = 4;
const int Gl = 4;
const int Gh = 4;

const double A = 1e9;
const double Ea = 0.7;
const double K = 8.617e-5;
const double t = 1e6;

const double sigmastar_const = 1.3;
const int task_multiplyer = 10000;
int glbmark = 1;
int mnt = 0;

ofstream testTraffic("./mapping/test_traffic2.txt");

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

double ERT(int a, double v, int nol, int md)
{
    // double c0 = 5.0, c1 = 1.0, c2 = 3.0, c3 = 8.0, c4 = 2.0;
    double c0 = 1.0, c1 = 1.0, c2 = 1.0, c3 = -1.0, c4 = -1.0;
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

double regionReliab()
{
    double rel = 0;
    int count = 0;
    for (int x = 0; x < Gw; x++)
    {
        for (int y = 0; y < Gl; y++)
        {
            for (int z = 0; z < Gh; z++)
            {
                double T = NoC[x][y][z].temperature;
                double lambda = A * exp(-Ea / (K * T));
                rel += exp(-lambda * t);
                count++;
            }
        }
    }
    return rel / count;
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
                            buf2[k] -= avgcores;
                        pair<vector<Application>, vector<int>> temp;
                        temp.first = buf;
                        temp.second = buf2;
                        WQ.push(temp);
                        for (int k = i; k < i + j && k <= Gh; k++)
                            buf2[k] += avgcores;
                    }
                }
            }
        }
        else
        {
            double sigma = 0;
            for (auto app : Nq.first)
            {
                sigma += ERT(app.tasks.size(), app.avgCommValue, app.NOL, app.MD);
            }
            if (sigma < sigmaStar)
            {
                sigmaStar = sigma;
                BN = Nq;
                marker = 0;
            }
            if (sigma <= leafNode_sigmastar)
            {
                leafNode_sigmastar = sigma;
                LBN = Nq;
            }
            if (!BN.first.empty() || !LBN.first.empty())
                leafnode_marker = 0;
        }
        if (marker)
            BN = LBN;
        if (leafnode_marker)
        {
            double sigma = 0;
            for (auto app : Nq.first)
            {
                sigma += ERT(app.tasks.size(), app.avgCommValue, app.NOL, app.MD);
            }
            if (sigma <= non_leaf_sigmastar)
            {
                non_leaf_sigmastar = sigma;
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

void calculate_task_criticalites(Application app)
{
    vector<vector<pair<int, int>>> adj(app.tasks.size());
    for (auto x : app.edges)
        adj[x[0]].push_back({x[1], x[2]});

    vector<int> comm_values(app.tasks.size());
    int outdegree = 0, commVolume = 0, execTime = 0;
    for (int i = 1; i <= app.tasks.size(); i++)
    {
        outdegree += adj[i].size();
        int buf = 0;
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
        int out_degree_ratio = adj[i].size() / outdegree;
        int comm_volume_ratio = comm_values[i - 1] / commVolume;
        int exec_time_ratio = app.tasks[i - 1] / execTime;
        app.task_criticalities[i] = out_degree_ratio + comm_volume_ratio + exec_time_ratio;
    }
}
void mapping_tasks()
{
}

int main()
{
    vector<Application> tapps = {
        {1, {1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5}, {{1, 2, 4}, {1, 3, 4}, {2, 4, 4}, {4, 5, 4}}, {3, 4, 5, 3}},
        {2, {3, 4, 2, 3, 1, 3, 4, 2, 3, 1}, {{2, 4, 3}, {3, 2, 4}, {5, 2, 3}}, {6, 2, 2}},
        {3, {2, 3, 9, 1}, {{3, 4, 1}, {1, 4, 8}}, {4, 9}},
        {4, {1, 5, 3, 3, 4}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}},
        {5, {1, 5, 3}, {{1, 2, 4}}, {2}},
        {6, {1, 5, 3, 3}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}},
        {7, {1, 2}, {{1, 2, 6}}, {2}},
        {8, {1, 2, 3, 4, 5, 6}, {{1, 3, 4}}, {2}}};
    /*{4, {1, 5, 3, 3, 4}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}}*/
    for (auto app : tapps)
    {
        calculate_task_criticalites(app);
    }
}