#include <iostream>
#include <vector>
#include <string>
#include <bitset>
#include <map>
#include <algorithm>

using namespace std;

// Maximum number of statements is 3000. 
// IDs can go up to 3N, but the number of unique entities is limited by N.
// We'll use a safe upper bound for bitsets. 3000 statements -> max ~9000 IDs?
// Actually sum of N over test cases is 3000. 
// A single test case can have N=3000, so we might see IDs up to 9000.
// However, we compress IDs to 0..count-1. The count is at most 9000.
const int MAX_CNT = 9005;

struct Intersection {
    int u, v, w;
};

int parent[MAX_CNT];
bitset<MAX_CNT> must_have[MAX_CNT];
bitset<MAX_CNT> must_not[MAX_CNT];
int id_counter;
map<int, int> val_to_id;

// DSU Find
int find_set(int v) {
    if (v == parent[v]) return v;
    return parent[v] = find_set(parent[v]);
}

// Get compressed ID
int get_id(int val) {
    if (val_to_id.find(val) == val_to_id.end()) {
        val_to_id[val] = id_counter++;
    }
    return val_to_id[val];
}

// Merge set b into set a
void union_sets(int a, int b) {
    a = find_set(a);
    b = find_set(b);
    if (a != b) {
        // Merge Row Constraints (What constraints apply to this group)
        must_have[a] |= must_have[b];
        must_not[a] |= must_not[b];
        
        // Merge Column Constraints (Update references in other groups)
        // Since 'b' is now 'a', any group containing 'b' now contains 'a'.
        // This effectively renames bit 'b' to 'a' in all bitsets.
        for (int i = 0; i < id_counter; ++i) {
            // Only iterate active roots to save time, though strictly i < id_counter covers all.
            // Using parent[i]==i check might be an optimization, but loop is fast enough for N=3000 sum.
            if (parent[i] == i) {
                if (must_have[i].test(b)) {
                    must_have[i].reset(b);
                    must_have[i].set(a);
                }
                if (must_not[i].test(b)) {
                    must_not[i].reset(b);
                    must_not[i].set(a);
                }
            }
        }
        
        parent[b] = a;
    }
}

void solve() {
    int n;
    if (!(cin >> n)) return;

    // Reset for new test case
    val_to_id.clear();
    id_counter = 0;
    
    struct RawStmt {
        int type; // 0: rec, 1: not-rec, 2: inter
        int a, b, c;
    };
    vector<RawStmt> stmts;
    stmts.reserve(n);

    for (int i = 0; i < n; ++i) {
        string type;
        cin >> type;
        if (type == "recommend") {
            int u, v; cin >> u >> v;
            stmts.push_back({0, u, v, 0});
        } else if (type == "not-recommend") {
            int u, v; cin >> u >> v;
            stmts.push_back({1, u, v, 0});
        } else {
            int u, v, w; cin >> u >> v >> w;
            stmts.push_back({2, u, v, w});
        }
    }

    // Assign internal IDs
    vector<Intersection> intersections;
    for (auto& s : stmts) {
        s.a = get_id(s.a);
        s.b = get_id(s.b);
        if (s.type == 2) {
            s.c = get_id(s.c);
            intersections.push_back({s.a, s.b, s.c});
        }
    }

    // Initialize Structures
    for (int i = 0; i < id_counter; ++i) {
        parent[i] = i;
        must_have[i].reset();
        must_not[i].reset();
    }

    // Apply Initial Constraints
    // Note: We use find_set(x) immediately to handle any pre-existing logic if we were dynamic,
    // but here initially everyone is their own root.
    for (auto& s : stmts) {
        int u = s.a; 
        int v = s.b;
        if (s.type == 0) { // recommend u v -> u in S_v
            must_have[v].set(u);
        } else if (s.type == 1) { // not-recommend u v -> u not in S_v
            must_not[v].set(u);
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;

        // 1. Detect aliases via duplicate intersection definitions
        // If S_u = S_v \cap S_w and S_x = S_v \cap S_w, then u == x.
        map<pair<int, int>, int> def_map;
        
        for (auto& inter : intersections) {
            int u = find_set(inter.u);
            int v = find_set(inter.v);
            int w = find_set(inter.w);

            // Normalize order for key
            if (v > w) swap(v, w);

            // Special Case: S_u = S_v \cap S_v => S_u = S_v => u == v
            if (v == w) {
                if (u != v) {
                    union_sets(u, v);
                    changed = true;
                }
            } else {
                if (def_map.count({v, w})) {
                    int existing = def_map[{v, w}];
                    if (existing != u) {
                        union_sets(existing, u);
                        changed = true;
                    }
                } else {
                    def_map[{v, w}] = u;
                }
            }
        }
        
        // 2. Propagate Intersection Logic
        // Re-iterate because unions might have changed roots
        for (auto& inter : intersections) {
            int u = find_set(inter.u);
            int v = find_set(inter.v);
            int w = find_set(inter.w);

            // S_u = S_v \cap S_w
            
            // Logic: x in Sv AND x in Sw => x in Su
            bitset<MAX_CNT> common = must_have[v] & must_have[w];
            if ((must_have[u] | common) != must_have[u]) {
                must_have[u] |= common;
                changed = true;
            }

            // Logic: x in Su => x in Sv AND x in Sw
            if ((must_have[v] | must_have[u]) != must_have[v]) {
                must_have[v] |= must_have[u];
                changed = true;
            }
            if ((must_have[w] | must_have[u]) != must_have[w]) {
                must_have[w] |= must_have[u];
                changed = true;
            }

            // Logic: x not in Sv => x not in Su (since Su subset Sv)
            if ((must_not[u] | must_not[v]) != must_not[u]) {
                must_not[u] |= must_not[v];
                changed = true;
            }
            if ((must_not[u] | must_not[w]) != must_not[u]) {
                must_not[u] |= must_not[w];
                changed = true;
            }

            // Logic: x not in Su AND x in Sv => x not in Sw
            bitset<MAX_CNT> implied_not_w = must_not[u] & must_have[v];
            if ((must_not[w] | implied_not_w) != must_not[w]) {
                must_not[w] |= implied_not_w;
                changed = true;
            }
            
            // Logic: x not in Su AND x in Sw => x not in Sv
            bitset<MAX_CNT> implied_not_v = must_not[u] & must_have[w];
            if ((must_not[v] | implied_not_v) != must_not[v]) {
                must_not[v] |= implied_not_v;
                changed = true;
            }
        }
    }

    // Final Consistency Checks
    int active_roots = 0;
    for (int i = 0; i < id_counter; ++i) {
        if (parent[i] == i) {
            active_roots++;
            // 1. Contradiction check
            if ((must_have[i] & must_not[i]).any()) {
                cout << "no" << endl;
                return;
            }
            // 2. Self-loop check (x recommends x)
            if (must_have[i].test(i)) {
                cout << "no" << endl;
                return;
            }
        }
    }

    // 3. Cycle Detection (Topological Sort)
    // Build graph where edge U -> V exists if group V contains member U
    // Due to union_sets logic, bits in bitsets correspond to current roots.
    vector<vector<int>> adj(id_counter);
    vector<int> in_degree(id_counter, 0);

    for (int v = 0; v < id_counter; ++v) {
        if (parent[v] != v) continue;
        
        // For every active root u, if u is in must_have[v], add u -> v
        // We can iterate all k set in must_have[v]. 
        // Since we migrated columns, only active roots should be set.
        for (int u = must_have[v]._Find_first(); u < MAX_CNT; u = must_have[v]._Find_next(u)) {
             // u is a recommender of v. So u happened before v. Edge u -> v.
             // Double check u is a root (it should be if logic is correct).
             // If u is not a root, find_set(u) is the root.
             int root_u = find_set(u);
             if (root_u != u) {
                 // This theoretically shouldn't happen with full migration, 
                 // but safe to handle.
             }
             adj[root_u].push_back(v);
             in_degree[v]++;
        }
    }

    // Kahn's Algorithm
    vector<int> q;
    for (int i = 0; i < id_counter; ++i) {
        if (parent[i] == i && in_degree[i] == 0) {
            q.push_back(i);
        }
    }

    int processed = 0;
    while (!q.empty()) {
        int u = q.back(); q.pop_back();
        processed++;
        for (int v : adj[u]) {
            in_degree[v]--;
            if (in_degree[v] == 0) q.push_back(v);
        }
    }

    if (processed < active_roots) {
        cout << "no" << endl;
    } else {
        cout << "yes" << endl;
    }
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);
    int t;
    if (cin >> t) {
        while (t--) {
            solve();
        }
    }
    return 0;
}