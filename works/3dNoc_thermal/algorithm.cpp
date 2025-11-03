#include <bits/stdc++.h>
using namespace std;

const int Gw = 6;
const int Gl = 6;
const int Gh = 6;
int glbmark = 1;

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
    int placed = 1;
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
};

struct Core
{
    int isFree = -1;
    int x, y, z;
};

Core NoC[Gw][Gl][Gh];
int Pc[Gw][Gl][Gh];
std::vector<int> freecores((int)Gh, (int)(Gw * Gl));
using AppIntPair = pair<vector<Application>, vector<int>>;

double ERT(int a, double v, int nol, int md)
{
    // double c0 = 5.0, c1 = 1.0, c2 = 3.0, c3 = 8.0, c4 = 2.0;
    double c0 = 1.0, c1 = 1.0, c2 = 1.0, c3 = 1.0, c4 = 1.0;
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
    for (const auto &app : apps)
    {
        double ert = lowerbound_ert(app);
        if (ert == -1.0)
            continue;
        sigmaStar += ert;
    }
    return sigmaStar;
}

AppIntPair findCoreRegionShape(vector<Application> &apps)
{
    queue<AppIntPair> WQ;
    AppIntPair NBN, BN, LBN;
    NBN.first = apps;
    NBN.second = freecores;
    double sigmaStar = computeSigmaStar(NBN.first) * 1.3;
    double leafNode_sigmastar = INT_MAX;
    int x = 0;
    WQ.push(NBN);
    int counter = 0;
    int marker = 1;
    while (WQ.size() != 0)
    {
        AppIntPair Nq = WQ.front();
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
                for (int j = nolmin; j <= nolmin; j++)
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
                    int sigmaMin = 0;
                    for (int i = 0; i < x; i++)
                    {
                        sigmaMin += ERT(buf[i].tasks.size(), buf[i].avgCommValue, buf[i].NOL, buf[i].MD);
                    }
                    sigmaMin += ERT(buf[x].tasks.size(), buf[i].avgCommValue, i, j);
                    for (int i = x + 1; i < Nq.first.size(); i++)
                    {
                        double ert = lowerbound_ert(Nq.first[i]);
                        sigmaMin += ert;
                    }
                    if (sigmaMin >= sigmaStar)
                    {
                        feasible = 0;
                    }
                    if (feasible)
                    {
                        for (int k = i; k < i + j && k <= Gh; k++)
                        {
                            buf2[k] -= avgcores;
                        }
                        pair<vector<Application>, vector<int>> temp;
                        temp.first = buf;
                        temp.second = buf2;
                        WQ.push(temp);
                        for (int k = i; k < i + j && k <= Gh; k++)
                        {
                            buf2[k] += avgcores;
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
        }
        if (marker)
        {
            BN = LBN;
        }
    }

    return BN;
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

    for (int y = startY; (dirY > 0 ? y < Gl : y >= 0); y += dirY)
    {
        for (int x = startX; (dirX > 0 ? x < Gw : x >= 0); x += dirX)
        {
            int available = 0;
            int zmark = startZ;
            for (int k = startZ; k <= Gh - app.NOL; k++)
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
                if (available >= app.tasks.size() || glbmark == 1)
                {
                    zmark = k;
                    break;
                }
            }
            if (available < app.tasks.size())
                continue;

            int assigned = 0;
            for (int z = zmark; z < zmark + app.NOL && assigned < app.tasks.size(); z++)
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
            app.startZ = startZ;
            return true;
        }
    }
    return false;
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
    for (int z = startZ; z < startZ + app.NOL; z++)
    {
        for (int y = app.ymin; y <= app.ymax; y++)
        {
            for (int x = app.xmin; x <= app.xmax; x++)
            {
                int newx = x + (dir.first * path.first), newy = y + (dir.second * path.second);
                if (newx != x || newy != y)
                {
                    NoC[x][y][z].isFree = -1;
                    NoC[newx][newy][z].isFree = app.id;
                }
            }
        }
    }
}

bool compare(const Application &a, const Application &b)
{
    return a.tasks.size() > b.tasks.size();
}

void defragmentation(vector<Application> &apps)
{
    double F = 1.0 - (double)calculateCenterFreeCores() / (Gw * Gl * Gh);
    const double FTH = 0.01;

    if (F > FTH)
    {
        sort(apps.begin(), apps.end(), compare);
        for (auto &app : apps)
        {
            if (app.placed != 0)
            {
                int x = 0, y = 0;
                int RE = Gw - (app.xmax + 1);
                int RW = (app.xmin + 1);
                int RN = Gl - (app.ymax + 1);
                int RS = (app.ymin + 1);
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
                pair<pair<int, int>, pair<int, int>> paths = findVirtualMigrationPaths(app, x, y);
                pair<int, int> path;
                if (paths.first.first + paths.first.second > paths.second.first + paths.second.second)
                {
                    path = paths.first;
                }
                else
                {
                    path = paths.second;
                }
                migrateApplication(app, path, {x, y});
            }
        }
    }
}

int main()
{
    vector<Application> apps = {
        {1, {
                1,
                2,
                3,
                4,
                5,
                1,
                2,
                3,
                4,
                5,
                1,
                2,
                3,
                4,
                5,
                1,
                2,
                3,
                4,
                5,
                1,
                2,
                3,
                4,
                5,
                1,
                2,
                3,
                4,
                5,
                1,
                2,
                3,
                4,
                5,
                1,
                2,
                3,
                4,
                5,
            },
         {{1, 2, 4}, {1, 3, 4}, {2, 4, 4}, {4, 5, 4}},
         {3, 4, 5, 3}},
        {2, {
                3,
                4,
                2,
                3,
                1,
                3,
                4,
                2,
                3,
                1,
            },
         {{2, 4, 3}, {3, 2, 4}, {5, 2, 3}},
         {6, 2, 2}},
        {3, {2, 3, 9, 1}, {{3, 4, 1}, {1, 4, 8}}, {4, 9, 2}},
        {4, {1, 5, 3, 3, 4}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}},
        {5, {1, 5, 3}, {{1, 2, 4}}, {2}},
        {6, {1, 5, 3, 3}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}},
        {7, {1, 2}, {{1, 2, 6}, {2}}},
        {8, {1, 2, 3, 4, 5, 6}, {{1, 3, 4}}, {2}}};
    /*{4, {1, 5, 3, 3, 4}, {{1, 3, 4}, {2, 1, 2}}, {2, 4}}*/
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
    for (auto app : apps)
    {
        app.computeAvg();
    }
    AppIntPair regionShape = findCoreRegionShape(apps);
    apps = regionShape.first;

    for (auto app : apps)
    {
        cout << app.id << " " << app.MD << " " << app.NOL << endl;
    }
    int cornerIndex = 0;
    for (auto &app : apps)
    {
        bool possible = true;
        for (int x = 1; x <= 4; x++)
        {
            possible = findCoreRegionLocation(app, cornerIndex % 4);
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
            app.placed = 0;
        cornerIndex++;
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

    defragmentation(apps);

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
    return 0;
}
