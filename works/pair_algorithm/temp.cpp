#include <bits/stdc++.h>
using namespace std;

#define SATURATION_THRESHOLD 10
const int Gw = 10;
const int Gl = 10;
const int Gh = 10;
int glbmark = 1;

struct Application
{
    int id;
    vector<int> tasks;
    vector<vector<int>> edges;
    vector<double> communicationVolume;
};

struct Core
{
    int isFree = 1;
    string task_no = NULL;
    int isnotBlocked = 0;
    int wareoff_const = 0;
    int x, y, z;
};

Core mesh[Gw][Gl][Gh];
int strx = 0;
int stry = 0;
int strz = 0;
vector<vector<int>> emptySingleCores;

void fill_gaps()
{
    int curGapSize = 0;
    for (int i = 0; i < Gh; i++)
    {
        if (i % 2 == 0 && (i / 2) % 2 == 0)
        {
            if (i + 1 < Gh)
            {
                for (int j = 0; j < Gl; j++)
                {
                    if (j & 1 == 0)
                    {
                        for (int k = 0; k < Gw; k++)
                        {
                            if (!mesh[k][j][i].isnotBlocked)
                            {
                                curGapSize++;
                            }
                            else
                            {
                                int layerCount = Gw * Gl;
                                int columnCount = Gw;
                                int height_dif = curGapSize / layerCount;
                                int rem_layer = curGapSize % layerCount;
                                int column_dif = rem_layer / columnCount;
                                int row_dif = rem_layer % columnCount;
                            }
                        }
                    }
                    else
                    {
                        for (int k = Gw - 1; k >= 0; k--)
                        {
                            if (!mesh[k][j][i].isnotBlocked)
                            {
                                curGapSize++;
                            }
                        }
                    }
                }
            }
            else
            {

                for (int j = 0; j < Gl; j++)
                {
                    if (j & 1 == 0)
                    {
                        for (int k = 0; k < Gw; k++)
                        {
                            if (!mesh[k][j][i].isnotBlocked)
                            {
                                curGapSize++;
                            }
                        }
                    }
                    else
                    {
                        for (int k = Gw - 1; k >= 0; k--)
                        {
                            if (!mesh[k][j][i].isnotBlocked)
                            {
                                curGapSize++;
                            }
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
                    if (j & 1 == 0)
                    {
                        for (int k = Gw - 1; k >= 0; k--)
                        {
                            if (!mesh[k][j][i].isnotBlocked)
                            {
                                curGapSize++;
                            }
                        }
                    }
                    else
                    {
                        for (int k = 0; k < Gw; k++)
                        {
                            if (!mesh[k][j][i].isnotBlocked)
                            {
                                curGapSize++;
                            }
                        }
                    }
                }
            }
            else
            {
                for (int j = Gl - 1; j >= 0; j--)
                {
                    if (j & 1 == 0)
                    {
                        for (int k = Gw - 1; k >= 0; k--)
                        {
                            if (!mesh[k][j][i].isnotBlocked)
                            {
                                curGapSize++;
                            }
                        }
                    }
                    else
                    {
                        for (int k = 0; k < Gw; k++)
                        {
                            if (!mesh[k][j][i].isnotBlocked)
                            {
                                curGapSize++;
                            }
                        }
                    }
                }
            }
        }
    }
}
int starting_point_condition()
{
    int ans = 0;
    int upperHalfConst = 0, lowerHalfConst = 0, lowerNodes = 0, upperNodes = 0;
    for (int i = 0; i < Gh / 2; i++)
    {
        for (int j = 0; j < Gl; j++)
        {
            for (int k = 0; k < Gw; k++)
            {
                lowerHalfConst += mesh[k][j][i].wareoff_const;
                lowerNodes++;
            }
        }
    }
    for (int i = Gh / 2; i < Gh; i++)
    {
        for (int j = 0; j < Gl; j++)
        {
            for (int k = 0; k < Gw; k++)
            {
                upperHalfConst += mesh[k][j][i].wareoff_const;
                upperNodes++;
            }
        }
    }
    upperHalfConst /= upperNodes;
    lowerHalfConst /= lowerNodes;
    if (lowerHalfConst - upperHalfConst >= SATURATION_THRESHOLD)
    {
        ans = 1;
    }
    return ans;
}
int layer_empty(int layer, int m, int n, int num_tasks)
{
    int mark = 1, cnt = 0, ans = 0;
    if (layer % 2 == 0 && (layer / 2) % 2 == 0)
    {
        for (int j = m; j < Gl; j++)
        {
            if (j & 1 == 0)
            {
                for (int k = n; k < Gw; k++)
                {
                    if (cnt >= num_tasks)
                    {
                        ans = 1;
                        break;
                    }
                    if (!mesh[k][j][layer].isFree)
                    {
                        mark = 0;
                        break;
                    }
                    if (layer + 1 < Gh)
                    {
                        if (mesh[k][j][layer].isnotBlocked && mesh[k][j][layer + 1].isnotBlocked)
                            cnt += 2;
                        else if (mesh[k][j][layer].isnotBlocked || mesh[k][j][layer + 1].isnotBlocked)
                            cnt += 1;
                    }
                    else
                    {
                        if (mesh[k][j][layer].isnotBlocked)
                            cnt++;
                    }
                }
            }
            else
            {
                for (int k = n; k >= 0; k--)
                {
                    if (cnt >= num_tasks)
                    {
                        ans = 1;
                        break;
                    }
                    if (!mesh[k][j][layer].isFree)
                    {
                        mark = 0;
                        break;
                    }
                    if (layer + 1 < Gh)
                    {
                        if (mesh[k][j][layer].isnotBlocked && mesh[k][j][layer + 1].isnotBlocked)
                            cnt += 2;
                        else if (mesh[k][j][layer].isnotBlocked || mesh[k][j][layer + 1].isnotBlocked)
                            cnt += 1;
                    }
                    else
                    {
                        if (mesh[k][j][layer].isnotBlocked)
                            cnt++;
                    }
                }
            }
            if (ans || !mark)
                break;
        }
    }
    else
    {
        for (int j = m; j < Gl; j++)
        {
            if (j & 1 == 0)
            {
                for (int k = n; k >= 0; k--)
                {
                    if (cnt >= num_tasks)
                    {
                        ans = 1;
                        break;
                    }
                    if (!mesh[k][j][layer].isFree)
                    {
                        mark = 0;
                        break;
                    }
                    if (layer + 1 < Gh)
                    {
                        if (mesh[k][j][layer].isnotBlocked && mesh[k][j][layer + 1].isnotBlocked)
                            cnt += 2;
                        else if (mesh[k][j][layer].isnotBlocked || mesh[k][j][layer + 1].isnotBlocked)
                            cnt += 1;
                    }
                    else
                    {
                        if (mesh[k][j][layer].isnotBlocked)
                            cnt++;
                    }
                }
            }
            else
            {
                for (int k = n; k < Gw; k++)
                {
                    if (cnt >= num_tasks)
                    {
                        ans = 1;
                        break;
                    }
                    if (!mesh[k][j][layer].isFree)
                    {
                        mark = 0;
                        break;
                    }
                    if (layer + 1 < Gh)
                    {
                        if (mesh[k][j][layer].isnotBlocked && mesh[k][j][layer + 1].isnotBlocked)
                            cnt += 2;
                        else if (mesh[k][j][layer].isnotBlocked || mesh[k][j][layer + 1].isnotBlocked)
                            cnt += 1;
                    }
                    else
                    {
                        if (mesh[k][j][layer].isnotBlocked)
                            cnt++;
                    }
                }
            }
            if (ans || !mark)
                break;
        }
    }
    return !mark || ans;
}

void start_point_execution(int i, int j, int k, int num_tasks)
{
    if (mesh[k][j][i].isFree && mesh[k][j][i].isnotBlocked)
    {
        if (layer_empty(i, j, k, num_tasks))
        {
            strx = k;
            stry = j;
            strz = i;
        }
    }
}
void calculate_starting_point(int num_tasks)
{
    for (int i = strz; i < Gh; i++)
    {
        if (i % 2 == 0 && (i / 2) % 2 == 0)
        {

            for (int j = stry; j < Gl; j++)
            {
                if (j & 1 == 0)
                {
                    for (int k = strx; k < Gw; k++)
                    {
                        start_point_execution(i, j, k, num_tasks);
                    }
                }
                else
                {
                    for (int k = strx; k >= 0; k--)
                    {
                        start_point_execution(i, j, k, num_tasks);
                    }
                }
            }
        }
        else if (i % 2 == 0 && (i / 2) % 2 == 1)
        {

            for (int j = stry; j >= Gl; j--)
            {
                if (j & 1 == 0)
                {
                    for (int k = 0; k < Gw; k++)
                    {
                        start_point_execution(i, j, k, num_tasks);
                    }
                }
                else
                {
                    for (int k = strx; k >= 0; k--)
                    {
                        start_point_execution(i, j, k, num_tasks);
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
    set<pair<int, int>> orderedEdges;
    for (int i = 0; i < app.edges.size(); i++)
        orderedEdges.insert({app.communicationVolume[i], i});
    for (auto &x : orderedEdges)
    {
        if (tasks[app.edges[x.second][0]] == -1 && tasks[app.edges[x.second][1]] == -1)
        {
            pairs.push_back({app.edges[x.second][0], app.edges[x.second][1]});
            tasks[app.edges[x.second][0]] = 1;
            tasks[app.edges[x.second][1]] = 1;
        }
    }
    vector<int> remTasks;
    for (int i = 0; i < tasks.size(); i++)
    {
        if (tasks[i] == 0)
        {
            remTasks.push_back(i);
        }
    }
    set<pair<int, int>> orderedTasks;
    for (int i = 0; i < remTasks.size(); i++)
    {
        orderedTasks.insert({app.tasks[remTasks[i]], remTasks[i]});
    }
    vector<pair<int, int>> orderedTasksVector(orderedTasks.begin(), orderedTasks.end());
    for (size_t i = 0; i < orderedTasksVector.size(); i += 2)
    {
        if (i + 2 <= orderedTasksVector.size())
        {
            pairs.push_back({orderedTasksVector[i].second, orderedTasksVector[i + 1].second});
        }
        else
        {
            pairs.push_back({orderedTasksVector[i].second, -1});
        }
    }
}

void brick_allocation(int k, int j, int i, int appId, int &x, vector<pair<int, int>> &pairs, vector<vector<int>> &emptySingleCores, vector<int> tasks)
{
    if (mesh[k][j][i].isnotBlocked && mesh[k][j][i + 1].isnotBlocked)
    {

        mesh[k][j][i].task_no = (max(tasks[pairs[x].first], tasks[pairs[x].second]) == tasks[pairs[x].first]) ? (to_string(appId) + "." + to_string(pairs[x].first)) : (to_string(appId) + "." + to_string(pairs[x].second));
        mesh[k][j][i].isFree = 0;
        if (pairs[x].second != -1)
        {
            mesh[k][j][i + 1].task_no = (min(tasks[pairs[x].first], tasks[pairs[x].second]) == tasks[pairs[x].second]) ? (to_string(appId) + "." + to_string(pairs[x].second)) : (to_string(appId) + "." + to_string(pairs[x].first));
            mesh[k][j][i + 1].isFree = 0;
        }
    }
    else if (!mesh[k][j][i].isnotBlocked)
        emptySingleCores.push_back({k, j, i});
    else if (!mesh[k][j][i + 1].isnotBlocked)
        emptySingleCores.push_back({k, j, i + 1});
    x++;
}

void normal_allocation(int k, int j, int i, vector<pair<int, int>> pairs, int &x, int &innInd, int appId)
{
    if (pairs[x].second != -1)
    {
        mesh[k][j][i].task_no = (innInd == 0) ? (to_string(appId) + "." + to_string(pairs[x].first)) : (to_string(appId) + "." + to_string(pairs[x].second));
        mesh[k][j][i].isFree = 0;
    }
    if (innInd == 1)
        x++;
    innInd = (innInd + 1) % 2;
}
void mapping_application(Application app, vector<pair<int, int>> pairs, vector<int> tasks, int appId)
{
    int x = 0, innInd = 0;
    for (int i = strz; i < Gh; i++)
    {
        if (i % 2 == 0 && (i / 2) % 2 == 0)
        {
            if (i + 1 < Gh)
            {
                for (int j = stry; j < Gl; j++)
                {
                    if (j & 1 == 0)
                    {
                        for (int k = strx; k < Gw; k++)
                        {
                            brick_allocation(k, j, i, x, appId, pairs, emptySingleCores, tasks);
                        }
                    }
                    else
                    {
                        for (int k = strx; k >= 0; k--)
                        {
                            brick_allocation(k, j, i, x, appId, pairs, emptySingleCores, tasks);
                        }
                    }
                }
            }
            else
            {
                for (int j = stry; j < Gl; j++)
                {
                    if (j & 1 == 0)
                    {
                        for (int k = strx; k < Gw; k++)
                        {
                            normal_allocation(k, j, i, pairs, x, innInd, appId);
                        }
                    }
                    else
                    {
                        for (int k = strx; k >= 0; k--)
                        {
                            normal_allocation(k, j, i, pairs, x, innInd, appId);
                        }
                    }
                }
            }
        }
        else if (i % 2 == 0 && (i / 2) % 2 == 1)
        {
            if (i + 1 < Gh)
            {
                for (int j = Gl - 1; j >= Gl; j--)
                {
                    if (j & 1 == 0)
                    {
                        for (int k = strx; k >= 0; k--)
                        {
                            brick_allocation(k, j, i, x, appId, pairs, emptySingleCores, tasks);
                        }
                    }
                    else
                    {
                        for (int k = strx; k < Gw; k++)
                        {
                            brick_allocation(k, j, i, x, appId, pairs, emptySingleCores, tasks);
                        }
                    }
                }
            }
            else
            {
                for (int j = Gl - 1; j >= 0; j--)
                {
                    if (j & 1 == 0)
                    {
                        for (int k = strx; k >= 0; k--)
                        {
                            normal_allocation(k, j, i, pairs, x, innInd, appId);
                        }
                    }
                    else
                    {
                        for (int k = strx; k < Gw; k++)
                        {
                            normal_allocation(k, j, i, pairs, x, innInd, appId);
                        }
                    }
                }
            }
        }
    }
}

void pair_algorithm(vector<Application> apps)
{
    for (int i = 0; i < apps.size(); i++)
    {
        fill_gaps();
        if (starting_point_condition())
        {
            strz = Gh / 2;
            strx = 0;
            stry = 0;
        }
        else
        {
            strz = 0;
            stry = 0;
            strx = 0;
        }
        calculate_starting_point(apps[i].tasks.size());
        vector<pair<int, int>> pairs = make_pairs(apps[i]);
        mapping_application(apps[i], pairs, apps[i].tasks, i);
    }
}
int main()
{
    vector<Application> apps = {
        {1, {1, 2, 3, 4, 5}, {{0, 1, 4}, {0, 2, 4}, {1, 3, 4}, {3, 4, 4}}, {2, 3, 5, 3}},
        {2, {3, 4, 2, 4}, {{2, 1, 3}, {3, 2, 4}, {3, 4, 3}}, {1, 4, 2}},
        {3, {2, 3}, {{3, 4, 1}, {1, 4, 8}}, {4, 9, 2}},
        {4, {1, 5, 3}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}},
        {5, {1}, {{1, 2, 4}}, {2}},
        {6, {1, 5, 3}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}},
        {7, {1, 2}, {{1, 2, 6}, {2}}},
        {8, {1, 2, 3, 4, 5, 6}, {{1, 2, 4}}, {2}}};
}