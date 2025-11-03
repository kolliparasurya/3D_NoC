#include <bits/stdc++.h>
#include <unistd.h>
#include "tinyxml2.h"

using namespace std;
using namespace tinyxml2;

const int Gw = 4;
const int Gl = 4;
const int Gh = 4;

const double sigmastar_const = 1.15;
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
        // vector<vector<pair<int, int>>> adj(tasks.size());
        // for (auto x : edges)
        //     adj[x[0] - 1].push_back({x[1] - 1, x[2]});
        // cout << " " << id << " hi" << endl;

        // int mx = 0;
        // auto dfs = [&](auto &&dfs, int u, int p, int s) -> void
        // {
        //     s += tasks[u];
        //     mx = max(mx, s);
        //     for (auto v : adj[u])
        //     {
        //         if (v.first == p)
        //             continue;
        //         dfs(dfs, v.first, u, s + v.second);
        //     }
        // };
        // for (auto x : adj)
        // {
        //     if (x.size() == 1)
        //     {
        //         dfs(dfs, x[0].first, -1, 0);
        //     }
        // }
        // run_time = mx;
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
};

Core NoC[Gw][Gl][Gh];
int Pc[Gw][Gl][Gh];

int removed_app_death_time = 0;
int total_sim_time = 0;
std::vector<int> freecores((int)Gh, (int)(Gw * Gl));
using AppIntPair = pair<vector<Application *>, vector<int>>;
vector<Application> tapps;
unordered_map<int, int> task_to_core;
map<int, vector<vector<int>>> task_timestamp; // new
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

void remove_prev_mapped_tod(std::pair<int, std::vector<int>> prefix_to_remove)
{
    const std::vector<int> &prefix_vec = prefix_to_remove.second;

    if (prefix_vec.empty())
    {
        return;
    }
    auto &vec_to_modify = task_timestamp[prefix_to_remove.first];

    for (auto it = vec_to_modify.begin(); it != vec_to_modify.end(); ++it)
    {
        const std::vector<int> &x = *it;

        bool vec_is_long_enough = (x.size() >= prefix_vec.size());
        bool prefix_matches = false;
        if (vec_is_long_enough)
        {
            prefix_matches = std::equal(prefix_vec.begin(), prefix_vec.end(), x.begin());
        }
        if (prefix_matches)
        {
            int jump = 0;
            if (x.size() > 2)
            {
                jump = x[2];
            }
            std::vector<int> new_vec = {prefix_vec[0], prefix_vec[1], jump, removed_app_death_time};
            *it = new_vec;
            break;
        }
    }
}

void update_mvtasks(Core *node1, Core *node2)
{
    int jump = removed_app_death_time;
    int buf = cnv_task_buf(node1->task_no);
    remove_prev_mapped_tod({buf, {node1->num_time_task, node1->x + (node1->y * Gw) + (node1->z * Gw * Gl)}});
    node2->num_time_task = node1->num_time_task + 1;
    task_timestamp[buf].push_back({node2->num_time_task, node2->x + (node2->y * Gw) + (node2->z * Gw * Gl), jump, node1->timeofdeath});

    node2->timeofdeath = node1->timeofdeath;
    node1->timeofdeath = removed_app_death_time;
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

bool findCoreRegionLocation(Application &app, int cornerIndex)
{

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
    return inmark ? true : false;
}

vector<Core> lineFreeCoreCount(Core current, char direction)
{
    vector<Core> freeCores;
    int dx = 0, dy = 0;
    if (direction == 'X')
        dx = 1;
    else if (direction == 'Y')
        dy = 1;
    if (current.isFree == -1)
        freeCores.push_back(current);
    for (int i = 1; i < max(Gw, Gl); ++i)
    {
        int nx = current.x + i * dx;
        int ny = current.y + i * dy;
        if (nx >= 0 && nx < Gw && ny >= 0 && ny < Gl && NoC[nx][ny][current.z].isFree == -1)
            freeCores.push_back(NoC[nx][ny][current.z]);
        else
            break;
    }
    for (int i = 1; i < max(Gw, Gl); ++i)
    {
        int nx = current.x - i * dx;
        int ny = current.y - i * dy;
        if (nx >= 0 && nx < Gw && ny >= 0 && ny < Gl && NoC[nx][ny][current.z].isFree == -1)
            freeCores.push_back(NoC[nx][ny][current.z]);
        else
            break;
    }
    return freeCores;
}

int calculateCenterFreeCores()
{
    int centerFreeCores = 0;
    for (int z = 0; z < Gh; ++z)
    {
        Core centerCore = NoC[Gw / 2][Gl / 2][z];
        vector<Core> xFreeCores = lineFreeCoreCount(centerCore, 'X');
        for (auto &core : xFreeCores)
        {
            vector<Core> yFreeCores = lineFreeCoreCount(core, 'Y');
            centerFreeCores += yFreeCores.size();
        }
    }
    return centerFreeCores;
}

pair<pair<int, int>, pair<int, int>> findVirtualMigrationPaths(Application &app, int dx, int dy)
{
    pair<int, int> pathXY, pathYX;
    int xpos, ypos;
    if (dx == -1)
    {
        xpos = app.xmin;
    }
    else if (dx == 1)
    {
        xpos = app.xmax;
    }
    if (dy == -1)
    {
        ypos = app.ymin;
    }
    else if (dy == 1)
    {
        ypos = app.ymax;
    }
    int mark = 0;
    int tempxXY = 0, tempyXY = 0;
    while (1)
    {
        xpos += dx;
        for (int z = app.startZ; z < app.startZ + app.NOL; z++)
        {
            int inner = 0;
            for (int y = app.ymin; y <= app.ymax; y++)
            {
                if (NoC[xpos][y][z].isFree != -1 || y < 0 || y >= Gl || xpos >= Gw || xpos <= -1)
                {
                    mark = 1;
                    inner = 1;
                    break;
                }
            }
            if (inner == 1)
                break;
        }
        if (mark == 1 || xpos >= Gw || xpos <= -1)
            break;
        tempxXY++;
    }
    mark = 0;
    while (1)
    {
        ypos += dy;
        for (int z = app.startZ; z < app.startZ + app.NOL; z++)
        {
            int inner = 0;
            for (int x = xpos - dx; x <= xpos - dx + (app.xmax - app.xmin); x++)
            {
                if (NoC[x][ypos][z].isFree != -1 || x < 0 || x >= Gw || ypos >= Gl || ypos <= -1)
                {
                    mark = 1;
                    inner = 1;
                    break;
                }
            }
            if (inner == 1)
                break;
        }
        if (mark == 1 || ypos >= Gl || ypos <= -1)
            break;
        tempyXY++;
    }
    pathXY.first = tempxXY;
    pathXY.second = tempyXY;

    xpos, ypos;
    if (dx == -1)
    {
        xpos = app.xmin;
    }
    else if (dx == 1)
    {
        xpos = app.xmax;
    }
    if (dy == -1)
    {
        ypos = app.ymin;
    }
    else if (dy == 1)
    {
        ypos = app.ymax;
    }

    int tempxYX = 0, tempyYX = 0;
    mark = 0;
    while (1)
    {
        ypos += dy;
        for (int z = app.startZ; z < app.startZ + app.NOL; z++)
        {
            int inner = 0;
            for (int x = app.xmin; x <= xpos - dx + (app.xmax - app.xmin); x++)
            {
                if (NoC[x][ypos][z].isFree != -1 || x < 0 || x >= Gw || ypos >= Gl || ypos <= -1)
                {
                    mark = 1;
                    inner = 1;
                    break;
                }
            }
            if (inner == 1)
                break;
        }
        if (mark == 1 || ypos >= Gl || ypos <= -1)
            break;
        tempyYX++;
    }
    mark = 0;
    while (1)
    {
        xpos += dx;
        for (int z = app.startZ; z < app.startZ + app.NOL; z++)
        {
            int inner = 0;
            for (int y = ypos - dy; y <= ypos - dy + (app.ymax - app.ymin); y++)
            {
                if (NoC[xpos][y][z].isFree != -1 || y < 0 || y >= Gl || xpos >= Gw || xpos <= -1)
                {
                    mark = 1;
                    inner = 1;
                    break;
                }
            }
            if (inner == 1)
                break;
        }
        if (mark == 1 || xpos >= Gw || xpos <= -1)
            break;
        tempxYX++;
    }
    pathYX.first = tempxYX;
    pathYX.second = tempyYX;

    return {pathXY, pathYX};
}

void migrateApplication(Application &app, pair<int, int> path, pair<int, int> dir)
{
    int startZ = app.startZ;
    int num_tasks = 0;
    for (int z = startZ; z < startZ + app.NOL; z++)
    {
        for (int y = app.ymax; y >= app.ymin; y--)
        {
            for (int x = app.xmax; x >= app.xmin; x--)
            {
                int newx = x + (dir.first * path.first), newy = y + (dir.second * path.second);
                if ((newx != x || newy != y) && NoC[x][y][z].isFree == app.id) //
                {
                    NoC[x][y][z].isFree = -1;
                    NoC[newx][newy][z].isFree = app.id;
                    update_mvtasks(&NoC[x][y][z], &NoC[newx][newy][z]);
                }
            }
        }
    }
}

bool compare(const Application *a, const Application *b)
{
    return a->tasks.size() > b->tasks.size();
}

void defragmentation(vector<Application *> &apps)
{
    double F = 1.0 - (double)calculateCenterFreeCores() / (Gw * Gl * Gh);
    const double FTH = 0.01;

    if (F > FTH)
    {
        sort(apps.begin(), apps.end(), compare);
        for (auto &app : apps)
        {
            if (app->placed != 0)
            {
                int x = 0, y = 0;
                int RE = Gw - (app->xmax + 1);
                int RW = (app->xmin + 1);
                int RN = Gl - (app->ymax + 1);
                int RS = (app->ymin + 1);
                if (RN >= RS && RE >= RW)
                {
                    x = -1, y = -1;
                }
                else if (RN < RS && RE >= RW)
                {
                    x = -1, y = 1;
                }
                else if (RN >= RS && RE < RW)
                {
                    x = 1, y = -1;
                }
                else if (RN < RS && RE < RW)
                {
                    x = 1, y = 1;
                }
                pair<pair<int, int>, pair<int, int>> paths = findVirtualMigrationPaths(*app, x, y);
                pair<int, int> path;
                if (paths.first.first + paths.first.second > paths.second.first + paths.second.second)
                {
                    path = paths.first;
                }
                else
                {
                    path = paths.second;
                }
                migrateApplication(*app, path, {x, y});
            }
        }
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
    // vector<Application> tapps = {
    //     {1, {1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5}, {{1, 2, 4}, {1, 3, 4}, {2, 4, 4}, {4, 5, 4}}, {3, 4, 5, 3}},
    //     {2, {3, 4, 2, 3, 1, 3, 4, 2, 3, 1}, {{2, 4, 3}, {3, 2, 4}, {5, 2, 3}}, {6, 2, 2}},
    //     {3, {2, 3, 9, 1}, {{3, 4, 1}, {1, 4, 8}}, {4, 9}},
    //     {4, {1, 5, 3, 3, 4}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}},
    //     {5, {1, 5, 3}, {{1, 2, 4}}, {2}},
    //     {6, {1, 5, 3, 3}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}},
    //     {7, {1, 2}, {{1, 2, 6}}, {2}},
    //     {8, {1, 2, 3, 4, 5, 6}, {{1, 3, 4}}, {2}}};
    // /*{4, {1, 5, 3, 3, 4}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}}*/
    graphsUpdating(argc, argv);
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
            int zid = 0;
            for (auto &app : apps)
            {
                if (app->MD == i && !app->placed)
                {
                    bool possible = true;
                    for (int x = 1; x <= 4; x++)
                    {
                        possible = findCoreRegionLocation(*app, cornerIndex % 4);
                        if (!possible)
                        {
                            cornerIndex++;
                        }
                        else
                        {
                            break;
                        }
                    }
                    if (possible == false)
                        app->placed = 0;
                    else
                        app->placed = 1;
                    cornerIndex++;
                }
                zid++;
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
                        {
                            NoC[x][y][z].isFree = -1;
                        }
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
        defragmentation(active_apps);
        // save_task_traffic_file();

        cout << "Final state of the NoC:" << endl;
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
                    // cout << " kello" << endl;
                    // cout << task_timestamp[first_task][k][1] << " " << task_timestamp[second_task][l][1] << " " << "0.01" << " " << "0.01" << " " << task_timestamp[first_task][k][2] << " " << task_timestamp[first_task][k][3] << endl;
                    if (task_timestamp[second_task][l][0] == k)
                    {
                        testTraffic << task_timestamp[first_task][k][1] << " " << task_timestamp[second_task][l][1] << " " << "0.01" << " " << "0.01" << " " << task_timestamp[first_task][k][2] << " " << task_timestamp[first_task][k][3] << endl;
                        break;
                    }
                }
            }
        }
    }
    return 0;
}
