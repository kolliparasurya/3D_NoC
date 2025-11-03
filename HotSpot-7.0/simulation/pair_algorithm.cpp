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

#include "thermal_simulator.h"

using namespace std;
using namespace tinyxml2;

/*
                                            IMPORVMENT HAVE TO BE DONE
                                    1. Runtime of the application formula have to change
                                    2. While you are doing FillGaps() are you checking the ware_of_constant
                                        (the fillGaps() I am talking about is which the function running before task mapping )
*/

#define SATURATION_THRESHOLD 1000
#define TEMP_THRESHOLD 2000
const int Gw = 4;
const int Gl = 4;
const int Gh = 4;
const int NUNITS = 30; // These units are represent the no of components of everynode which is used in the thermal simulation
int f = 0, s = 0;
int glbmark = 1;

float ambient_temp = 25.0; // Ambient temperature in °C
float time_interval = 1.0; // Time interval for temperature
clock_t start, finish;

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
    string task_no = "";
    int num_time_task = 0;
    int isnotBlocked = 1;
    int wareoff_const = 0;
    int x = -1, y = -1, z = -1;

    float temp;               // Current temperature of the core
    float thermal_capacity;   // Heat capacity (J/°C)
    float thermal_resistance; // Thermal resistance (°C/W)
    float power_dynamic;

    int time_of_death;
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
int removed_app_id;
int removed_app_death_time;
vector<vector<int>> emptySingleCores;
vector<pair<Core *, Core *>> brickVector;
vector<pair<int, int>> appsLoc;
map<int, vector<vector<int>>> task_timestamp;
int extra = 0;
vector<Application> apps;
vector<Application> ind_apps;
vector<bool> mapped;
set<pair<int, int>> mapped_apps_rt;

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

double calc_runtime(Application app)
{
    double time = 0;
    for (int j = 0; j < app.communicationVolume.size(); j++)
    {
        time += app.communicationVolume[j];
    }
    return time;
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
    task_timestamp[buf].push_back({node2->num_time_task, node2->x + (node2->y * Gw) + (node2->z * Gw * Gl), jump, node1->time_of_death});

    node2->time_of_death = node1->time_of_death;
    node1->time_of_death = removed_app_death_time;
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

        update_mvtasks(brickVector[j].first, brickVector[i].first);
        update_mvtasks(brickVector[j].second, brickVector[i].second);
    }
    else if (i >= (Gw * Gl * Gh) - extra && j >= (Gw * Gl * Gh) - extra)
    {
        brickVector[i].first->task_no = brickVector[j].first->task_no;
        brickVector[i].first->isFree = brickVector[j].first->isFree;

        update_mvtasks(brickVector[j].first, brickVector[i].second);
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
int calculate_starting_point(int numPairs, int mark)
{
    int buf = s, cnt = 0, ans = 0, st_pt = s;
    while (buf < (Gw * Gl * Gh) / 2)
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

int mapping_application(vector<pair<int, int>> pairs, int appId, Application app, int mark)
{
    int st_pt = calculate_starting_point(pairs.size(), mark);
    if (st_pt != -1)
    {
        int x = 0;
        appsLoc.push_back({st_pt, pairs.size()});
        for (int i = st_pt; i < st_pt + pairs.size(); i++, x++)
        {
            brickVector[i].first->task_no = to_string(appId) + "." + to_string(pairs[x].first);
            brickVector[i].first->isFree = 0;
            brickVector[i].first->wareoff_const += app.tasks[pairs[x].first];

            brickVector[i].first->num_time_task = 0;
            int jump = brickVector[i].first->time_of_death;
            brickVector[i].first->time_of_death += app.runtime;
            int buf = cnv_task_buf(brickVector[i].first->task_no);
            task_timestamp[buf].push_back({0, brickVector[i].first->x + (brickVector[i].first->y * Gw) + (brickVector[i].first->z * Gw * Gl), jump, brickVector[i].first->time_of_death});
            if (pairs[x].second != -1)
            {
                brickVector[i].second->task_no = to_string(appId) + "." + to_string(pairs[x].second);
                brickVector[i].second->isFree = 0;
                brickVector[i].second->wareoff_const += app.tasks[pairs[x].second];

                brickVector[i].second->num_time_task = 0;
                int jump = brickVector[i].second->time_of_death;
                brickVector[i].second->time_of_death += app.runtime;
                int buf = cnv_task_buf(brickVector[i].second->task_no);
                task_timestamp[buf].push_back({0, brickVector[i].second->x + (brickVector[i].second->y * Gw) + (brickVector[i].second->z * Gw * Gl), jump, brickVector[i].second->time_of_death});
            }
        }
    }
    if (st_pt == -1)
        st_pt = 0;
    else
        st_pt = 1;
    cout << st_pt << "r ";
    return st_pt;
}

// Function to mi    grate tasks from overheating cores
void migrate_tasks(Core *overheating_core)
{
    printf("Migrating tasks from overheated core...\n");

    // Logic to find cooler cores and redistribute tasks
    // Example: Select a core with temp < 70°C for task migration
}
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
                if (mesh[k][j][i].isFree)
                    mf = 0;
                for (int id = 0; id < NUNITS; id++)
                    act[(i * Gw * Gl) + (j * Gw) + k + id] = mf;
            }
        }
    }
}

void Thermal_block()
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
                mesh[k][j][i].temp = get_max_grid_temperature(i * 2, k, j);
                if (mesh[k][j][i].temp >= TEMP_THRESHOLD)
                    mesh[k][j][i].isnotBlocked = 0;
                else
                    mesh[k][j][i].isnotBlocked = 1;
            }
        }
    }
}
void free_mesh()
{
    // pair<int, int> min_element = *mapped_apps_rt.begin();
    // mapped_apps_rt.erase(mapped_apps_rt.begin());
    pair<int, int> min_element = {INT_MAX, 0};
    for (int i = 0; i < Gh; i++)
    {
        for (int j = 0; j < Gl; j++)
        {
            for (int k = 0; k < Gw; k++)
            {
                if (mesh[k][j][i].task_no != "")
                {
                    if (min_element.first > mesh[k][j][i].time_of_death)
                    {
                        min_element.first = mesh[k][j][i].time_of_death;
                        std::string int_part = mesh[k][j][i].task_no.substr(0, mesh[k][j][i].task_no.find('.'));
                        int app_id = stoi(int_part);
                        min_element.second = app_id;
                    }
                }
            }
        }
    }
    for (int i = 0; i < Gh; i++)
    {
        for (int j = 0; j < Gl; j++)
        {
            for (int k = 0; k < Gw; k++)
            {
                if (mesh[k][j][i].task_no != "")
                {
                    std::string int_part = mesh[k][j][i].task_no.substr(0, mesh[k][j][i].task_no.find('.'));
                    int app_id = stoi(int_part);
                    if (min_element.second == app_id)
                    {
                        removed_app_id = app_id;
                        removed_app_death_time = mesh[k][j][i].time_of_death;
                        mesh[k][j][i].isFree = 1;
                        mesh[k][j][i].task_no = "";
                    }
                }
            }
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
            cout << " first " << endl;
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
                        }
                    }
                    cout << endl;
                }
                cout << endl;
            }
            cout << endl;
            free_mesh();
            fillGaps();
            cout << "------------------------------------------------------------------------" << endl;
            cout << " second " << endl;
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
                        }
                    }
                    cout << endl;
                }
                cout << endl;
            }
            cout << endl;
        }
        cout << "=====================================================================" << endl;
        for (int i = 0; i < apps.size(); i++)
        {
            if (!mapped[i])
            {
                int ans = starting_point_condition();
                if (ans)
                {
                    s = ((Gh * Gw * Gl) - extra) / 4;
                }
                else
                {
                    s = 0;
                }
                Thermal_block(); // surya

                vector<pair<int, int>> pairs = make_pairs(apps[i]);
                int zick = mapping_application(pairs, i + 1, apps[i], cnt);
                if (zick)
                {
                    mapped[i] = true;
                    mapped_apps_rt.insert({apps[i].runtime, apps[i].id});
                    cnt++;
                }
            }
        }
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
                    }
                }
                cout << endl;
            }
            cout << endl;
        }
        cout << endl;
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
        app.runtime = calc_runtime(app);

        Application ind_app;
        ind_app.id = gid + 1;
        ind_app.tasks = ind_tasks;
        ind_app.edges = ind_edgesVec;
        ind_app.communicationVolume = commVol;

        apps.push_back(app);
        ind_apps.push_back(ind_app);
    }
    mapped.resize(apps.size(), false);

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
    cout << "runtimes" << endl;
    for (auto x : apps)
        cout << x.runtime << " ";
    cout << endl;

    return 1;
}

int main(int argc, char *argv[])
{
    graphsUpdating(argc, argv);
    initiate();
    loadNodes();
    if (Gh % 2 == 0)
        extra = 0;
    else
        extra = Gw * Gl;
    thermal_initiation();
    pair_algorithm();
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
        int buf = (apps[i].id) * 10000;
        int noof_changes = task_timestamp[buf].size();
        // cout << noof_changes << endl;
        for (int j = 0; j < apps[i].edges.size(); j++)
        {
            int first_task = buf + apps[i].edges[j][0];
            int second_task = buf + apps[i].edges[j][1];
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
    // saveMapping();
    thermal_termination();
}

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

// void saveMapping()
// {
//     if (!mapFile)
//     {
//         cerr << "Error: Unable to open output file!" << endl;
//         return;
//     }
//     set<pair<int, int>> task_str;
//     vector<int> tasks;
//     vector<pair<int, int>> timpestamps;
//     mapFile << "taskno\tX\tY\tZ\n"; // Write heading
//     for (int i = 0; i < Gh; i++)
//     {
//         cout << "layer " << i + 1 << endl;
//         for (int j = 0; j < Gl; j++)
//         {
//             for (int k = 0; k < Gw; k++)
//             {
//                 if (mesh[k][j][i].task_no == "")
//                     cout << " #   ";
//                 else if (mesh[k][j][i].task_no != "")
//                 { // Only print non-empty task numbers
//                     cout << mesh[k][j][i].task_no << "  ";
//                     mapFile << mesh[k][j][i].task_no << " " << "\t" << k << "\t" << j << "\t" << i << "\n";
//                     int buf = cnv_task_buf(mesh[k][j][i].task_no);
//                     task_str.insert({buf, k + (j * Gw) + (i * Gw * Gl)});
//                 }
//             }
//             cout << endl;
//         }
//         cout << endl;
//     }
//     mapFile.close();
// }