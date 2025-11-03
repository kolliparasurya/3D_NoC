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

using namespace std;
using namespace tinyxml2;

#define SATURATION_THRESHOLD 1
const int Gw = 8;
const int Gl = 8;
const int Gh = 2;
int f = 0, s = 0;
int glbmark = 1;

float ambient_temp = 25.0; // Ambient temperature in °C
float time_interval = 1.0; // Time interval for temperature

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
    string task_no = "";
    int isnotBlocked = 1;
    int wareoff_const = 0;
    int x = -1, y = -1, z = -1;

    float temp;               // Current temperature of the core
    float thermal_capacity;   // Heat capacity (J/°C)
    float thermal_resistance; // Thermal resistance (°C/W)
    float power_dynamic;

    int time_of_death;
    vector<pair<int, int>> time_stamps;
    void updateOcc(int freeness, string num, int blockedNess)
    {
        isFree = freeness;
        task_no = num;
        isnotBlocked = blockedNess;
    }
};
Core example;
Core mesh[Gw][Gl][Gh];
int strx = 0;
int stry = 0;
int strz = 0;
vector<vector<int>> emptySingleCores;
vector<pair<Core *, Core *>> brickVector;
vector<pair<int, int>> appsLoc;
int extra = 0;
vector<Application> apps;
vector<Application> ind_apps;
ofstream mapFile("./mapping/mapFile.txt");
ofstream testTraffic("./mapping/test_traffic.txt");

void pairsAdding(Core &meshCore1, Core &meshCore2)
{
    Core *node1, *node2;
    node1 = &meshCore1;
    node2 = &meshCore2;
    brickVector.push_back({node1, node2});
}
void singleNodePairAdding(Core &meshCore1, Core &meshCore2)
{
    Core *node1, *node2;
    node1 = &meshCore1;
    node2 = &meshCore2;
    brickVector.push_back({node1, node2});
}

double runtime(Application app)
{
    double time = 0;
    for (int j = 0; j < app.communicationVolume.size(); j++)
    {
        time += app.communicationVolume[j];
    }
    return time;
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
// void fillGaps()
// {
//     int num_blocked = 0, num_free = 0;
//     f = 0, s = 0;
//     while (f < brickVector.size() || s < brickVector.size())
//     {
//         int firstBlocked = 0, secondBlocked = 0, firstFree = 0, secondFree = 0;
//         if (f < (Gw * Gl * Gh) - extra)
//         {
//             if (!brickVector[f].first->isnotBlocked || !brickVector[f].second->isnotBlocked)
//                 firstBlocked = 1;
//             if (!brickVector[s].first->isnotBlocked || !brickVector[s].second->isnotBlocked)
//                 secondBlocked = 1;
//             if (brickVector[f].first->isFree && brickVector[f].second->isFree)
//                 firstFree = 1;
//             if (brickVector[s].first->isFree && brickVector[s].second->isFree)
//                 secondFree = 1;
//         }
//         else
//         {
//             if (!brickVector[f].first->isnotBlocked)
//                 firstBlocked = 1;
//             if (!brickVector[s].first->isnotBlocked)
//                 secondBlocked = 1;
//             if (brickVector[f].first->isFree)
//                 firstFree = 1;
//             if (brickVector[s].first->isFree)
//                 secondFree = 1;
//         }

//         if ((!firstFree && !secondFree) || (firstBlocked && secondBlocked) || (firstFree && secondBlocked))
//         {
//             f++, s++;
//         }
//         else if (!firstFree && secondFree && !secondBlocked)
//         {
//             if (f < (Gw * Gl * Gh) - extra)
//             {
//                 brickVector[s].first->task_no = brickVector[f].first->task_no;
//                 brickVector[s].second->task_no = brickVector[f].second->task_no;
//                 brickVector[s].first->isFree = brickVector[f].first->isFree;
//                 brickVector[s].second->isFree = brickVector[f].second->isFree;

//                 brickVector[f].first->task_no = "";
//                 brickVector[f].second->task_no = "";
//                 brickVector[f].first->isFree = 1;
//                 brickVector[f].second->isFree = 1;
//             }
//             else
//             {
//                 brickVector[s].first->task_no = brickVector[f].first->task_no;
//                 brickVector[s].first->isFree = brickVector[f].first->isFree;
//                 brickVector[f].first->task_no = "";
//                 brickVector[f].first->isFree = 1;
//             }
//             f++, s++;
//         }
//         else if ((firstFree && secondFree) || (firstBlocked && secondFree))
//         {
//             f++;
//         }
//         else if (!firstFree && secondBlocked)
//         {
//             s++;
//         }
//     }
// }
//======================================================================================================================z
bool is_usable(int i)
{
    if (i < (Gw * Gl * Gh) - extra)
    {
        return brickVector[i].first->isnotBlocked && brickVector[i].second->isnotBlocked;
    }
    else
    {
        return brickVector[i].first->isnotBlocked;
    }
}

bool is_occupied(int i)
{
    if (i < (Gw * Gl * Gh) - extra)
    {
        return !brickVector[i].first->isFree || !brickVector[i].second->isFree;
    }
    else
    {
        return !brickVector[i].first->isFree;
    }
}

void move_tasks(int j, int i)
{
    if (i < (Gw * Gl * Gh) - extra && j < (Gw * Gl * Gh) - extra)
    {
        brickVector[i].first->task_no = brickVector[j].first->task_no;
        brickVector[i].second->task_no = brickVector[j].second->task_no;
        brickVector[i].first->isFree = brickVector[j].first->isFree;
        brickVector[i].second->isFree = brickVector[j].second->isFree;
    }
    else if (i >= (Gw * Gl * Gh) - extra && j >= (Gw * Gl * Gh) - extra)
    {
        brickVector[i].first->task_no = brickVector[j].first->task_no;
        brickVector[i].first->isFree = brickVector[j].first->isFree;
    }
}

void set_free(int i)
{
    if (i < (Gw * Gl * Gh) - extra)
    {
        brickVector[i].first->task_no = "";
        brickVector[i].second->task_no = "";
        brickVector[i].first->isFree = 1;
        brickVector[i].second->isFree = 1;
    }
    else
    {
        brickVector[i].first->task_no = "";
        brickVector[i].first->isFree = 1;
    }
}

void fillGaps()
{
    int N = brickVector.size();

    std::deque<int> Q;
    for (int i = 0; i < N; ++i)
    {
        if (is_usable(i) && is_occupied(i))
        {
            Q.push_back(i);
        }
    }

    for (int i = 0; i < N; ++i)
    {
        if (is_usable(i))
        {
            if (!Q.empty())
            {
                int j = Q.front();
                if (j != i)
                {
                    move_tasks(j, i);
                    set_free(j);
                }
                Q.pop_front();
            }
            else
            {
                if (is_occupied(i))
                {
                    set_free(i);
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
    set<pair<int, int>, greater<pair<int, int>>> orderedEdges;
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
    {
        if (tasks[i] == 0)
        {
            remTasks.push_back(i);
        }
    }
    set<pair<int, int>, greater<pair<int, int>>> orderedTasks;
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
    return pairs;
}
int calculate_starting_point(int numPairs)
{
    int buf = s, cnt = 0, ans = 0, st_pt = s;
    while (buf < Gw * Gl * Gh)
    {

        if (cnt == numPairs)
        {
            ans = 1;
            break;
        }
        if (!brickVector[buf].first->isnotBlocked || !brickVector[buf].second->isnotBlocked || !brickVector[buf].first->isFree || !brickVector[buf].second->isFree)
        {
            cnt = 0;
        }
        else if (brickVector[buf].first->isnotBlocked && brickVector[buf].second->isnotBlocked && brickVector[buf].first->isFree && brickVector[buf].second->isFree)
        {
            cnt++;
            if (cnt == 1)
                st_pt = buf;
        }
        buf++;
    }
    if (ans == 0)
    {
        st_pt = -1;
    }

    return st_pt;
}

void mapping_application(vector<pair<int, int>> pairs, int appId, Application app)
{
    int st_pt = calculate_starting_point(pairs.size());
    if (st_pt != -1)
    {
        int x = 0;
        appsLoc.push_back({st_pt, pairs.size()});
        for (int i = st_pt; i < st_pt + pairs.size(); i++, x++)
        {
            brickVector[i].first->task_no = to_string(appId) + "." + to_string(pairs[x].first);
            brickVector[i].first->isFree = 0;
            brickVector[i].first->wareoff_const += app.tasks[pairs[x].first];
            if (pairs[x].second != -1)
            {
                brickVector[i].second->task_no = to_string(appId) + "." + to_string(pairs[x].second);
                brickVector[i].second->isFree = 0;
                brickVector[i].second->wareoff_const += app.tasks[pairs[x].second];
            }
        }
    }
}
void update_temperature(Core *core)
{
    // Calculate temperature change using the thermal model
    float delta_T = (core->power_dynamic * time_interval - (core->temp - ambient_temp) / core->thermal_resistance) /
                    core->thermal_capacity;

    // Update the core's temperature
    core->temp += delta_T;
    update_temperature(core); // Update temperature for each core

    // Check for thermal violations (hotspot detection)
    if (core->temp > 85.0)
    { // Threshold temperature in °C
        printf("Warning: Core  exceeds safe temperature!\n");

        // Apply mitigation strategies
        core->power_dynamic *= 0.8; // Reduce power by 20%
        core->isnotBlocked = 0;
    }
}

// // Function to mi    grate tasks from overheating cores
// void migrate_tasks(Core *overheating_core)
// {
//     printf("Migrating tasks from overheated core...\n");

//     // Logic to find cooler cores and redistribute tasks
//     // Example: Select a core with temp < 70°C for task migration
// }

void Thermal_block()
{
    for (int i = 0; i < Gh; i++)
    {
        for (int j = 0; j < Gl; j++)
        {
            for (int k = 0; k < Gw; k++)
            {
                update_temperature(&mesh[k][j][i]);
            }
        }
    }
}
void pair_algorithm(vector<Application> apps)
{
    for (int i = 0; i < apps.size(); i++)
    {
        // Thermal_block();
        int ans = starting_point_condition();
        if (ans)
        {
            s = ((Gh * Gw * Gl) - extra) / 4;
        }
        else
        {
            fillGaps();
        }
        vector<pair<int, int>> pairs = make_pairs(apps[i]);
        mapping_application(pairs, i + 1, apps[i]);
    }
}

void initiate()
{
    for (int i = 0; i < Gh; i++)
    {
        for (int j = 0; j < Gl; j++)
        {
            for (int k = 0; k < Gw; k++)
            {
                mesh[k][j][i].x = k, mesh[k][j][i].y = j, mesh[k][j][i].z = i;
            }
        }
    }
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
        app.communicationVolume = commVol;

        Application ind_app;
        ind_app.id = gid + 1;
        ind_app.tasks = ind_tasks;
        ind_app.edges = ind_edgesVec;
        ind_app.communicationVolume = commVol;

        apps.push_back(app);
        ind_apps.push_back(ind_app);
    }

    // For demonstration, print out the applications in the requested format.
    // (You can remove the printing if you need to use the apps vector directly.)
    for (auto &app : apps)
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
        for (size_t i = 0; i < app.communicationVolume.size(); i++)
        {
            cout << app.communicationVolume[i];
            if (i != app.communicationVolume.size() - 1)
                cout << ", ";
        }
        cout << "}\n";
        cout << "}\n";
        cout << "\n";
    }
    return 1;
}
void saveMapping()
{
    if (!mapFile)
    {
        cerr << "Error: Unable to open output file!" << endl;
        return;
    }
    set<pair<int, int>> task_str;
    vector<int> tasks;
    mapFile << "taskno\tX\tY\tZ\n"; // Write heading

    for (int i = 0; i < Gh; i++)
    {
        cout << "layer " << i + 1 << endl;
        for (int j = 0; j < Gl; j++)
        {
            for (int k = 0; k < Gw; k++)
            {
                if (mesh[k][j][i].task_no == "")
                    cout << " #   ";
                else if (mesh[k][j][i].task_no != "")
                { // Only print non-empty task numbers
                    cout << mesh[k][j][i].task_no << "  ";
                    mapFile << mesh[k][j][i].task_no << " " << "\t" << k << "\t" << j << "\t" << i << "\n";
                    int buf = 0;
                    stringstream ss(mesh[k][j][i].task_no);
                    string buff_t;
                    int divd = 10000;
                    while (getline(ss, buff_t, '.'))
                        buf += stoi(buff_t) * divd, divd /= 10000;
                    task_str.insert({buf, k + (j * Gw) + (i * Gw * Gl)});
                }
            }
            cout << endl;
        }
        cout << endl;
    }
    for (auto x : task_str)
    {
        tasks.push_back(x.second);
    }
    cout << endl;
    vector<pair<int, int>> taskMap;
    for (auto x : ind_apps)
    {
        pair<int, int> buf;
        for (int i = 0; i < x.edges.size(); i++)
        {
            buf.first = tasks[x.edges[i][0]];
            buf.second = tasks[x.edges[i][1]];
            testTraffic << buf.first << "\t" << buf.second << "\n";
        }
        taskMap.push_back(buf);
    }

    mapFile.close();
}

int main(int argc, char *argv[])
{
    // vector<Application>
    //     apps = {
    //         {1, {20, 20, 20, 20, 20, 20, 20, 20, 20}, {{0, 2, 25}, {0, 3, 22}, {1, 7, 13}, {3, 4, 12}, {3, 5, 9}, {3, 6, 7}, {4, 8, 6}, {7, 8, 12}}, {5, 6, 8, 12, 13, 14, 15, 17}},
    //         {2, {1, 2, 3, 4, 5}, {{0, 1, 4}, {0, 2, 4}, {1, 3, 4}, {3, 4, 4}}, {2, 3, 5, 3}},
    //         {3, {3, 4, 2, 4}, {{1, 0, 3}, {2, 1, 4}, {2, 3, 3}}, {0, 3, 2}},
    //         {4, {2, 3}, {{0, 1, 1}}, {4}},
    //         {5, {1, 5, 3}, {{1, 0, 4}, {2, 1, 2}}, {2, 4}},
    //         {6, {1}, {}, {}}};
    // for (int i = 0; i < brickVector.size(); i++)
    // {
    //     cout << brickVector[i].first->x << " " << brickVector[i].first->y << " " << brickVector[i].first->z << "   " << brickVector[i].second->x << " " << brickVector[i].second->y << " " << brickVector[i].second->z << endl;
    // }
    // brickVector[0].first->updateOcc(0, "a", 1), brickVector[0].second->updateOcc(0, "b", 1);
    // brickVector[1].first->updateOcc(0, "c", 1), brickVector[1].second->updateOcc(0, "d", 1);
    // brickVector[2].first->updateOcc(0, "e", 1), brickVector[2].second->updateOcc(0, "f", 1);
    // brickVector[3].first->updateOcc(1, "", 0), brickVector[3].second->updateOcc(1, "", 1);
    // brickVector[4].first->updateOcc(1, "", 1), brickVector[4].second->updateOcc(1, "", 1);
    // brickVector[5].first->updateOcc(1, "", 0), brickVector[5].second->updateOcc(1, "", 1);
    // brickVector[6].first->updateOcc(1, "", 1), brickVector[6].second->updateOcc(1, "", 1);
    // brickVector[7].first->updateOcc(1, "", 1), brickVector[7].second->updateOcc(1, "", 1);
    // brickVector[8].first->updateOcc(0, "g", 1), brickVector[8].second->updateOcc(0, "h", 1);
    // brickVector[9].first->updateOcc(1, "", 1), brickVector[9].second->updateOcc(1, "", 1);
    // brickVector[9].first->updateOcc(0, "k", 1);
    graphsUpdating(argc, argv);
    initiate();
    loadNodes();
    if (Gh % 2 == 0)
        extra = 0;
    else
        extra = Gw * Gl;
    pair_algorithm(apps);
    saveMapping();
}
