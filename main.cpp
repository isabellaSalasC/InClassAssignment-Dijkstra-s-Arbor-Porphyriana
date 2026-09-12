// Arbor Porphyriana modeled with a Van Emde Boas Tree (VEB)
// + Dijkstra (unit weights) with timing + ASCII & Graphviz diagrams (ASCII-only).
// + Cycle detection & pruning (shortest-path based) for strict single-inheritance
//   categorization.
//
// Build & run (example):
//   g++ -std=c++17 -O2 -o arbor main.cpp && ./arbor
//
// Diagram (Graphviz):
//   dot -Tpng porphyry.dot -o porphyry.png
//
// What this program does:
// 1) Implements a Van Emde Boas (VEB) tree to index all concept IDs.
// 2) Builds a Porphyrian-style taxonomy (sample "animal -> feline/canine -> cat... dog...",
//    plus a generator for an N-level synthetic tree).
// 3) Detects and prunes cycles / redundant classification edges using a
//    shortest-path (BFS) spanning forest, so the taxonomy respects strict
//    single inheritance (each concept has exactly one shortest route to root).
// 4) Measures and prints build time, prune time, and Dijkstra time (shortest
//    path between terms).
// 5) Prints a compact textual view of the VEB clusters with their labels.
// 6) Renders the taxonomy as:
//    - ASCII tree in the console (ASCII characters only for portability).
//    - Graphviz DOT file (porphyry.dot) for a clean diagram.
//

#include <bits/stdc++.h>
using namespace std;
using namespace std::chrono;

// ----------------------------- Van Emde Boas Tree -----------------------------
class Van_Emde_Boas {
public:
    int universe_size;  // U
    int minimum;        // min key or -1 if empty
    int maximum;        // max key or -1 if empty
    Van_Emde_Boas* summary;                 // VEB(sqrt(U))
    vector<Van_Emde_Boas*> clusters;        // sqrt(U) clusters, each VEB(sqrt(U))

    explicit Van_Emde_Boas(int size): universe_size(size), minimum(-1), maximum(-1), summary(nullptr) {
        if (size <= 2) {
            clusters = vector<Van_Emde_Boas*>(0, nullptr);
        } else {
            int no_clusters = (int)ceil(sqrt((double)size));
            summary = new Van_Emde_Boas(no_clusters);
            clusters = vector<Van_Emde_Boas*>(no_clusters, nullptr);
            for (int i = 0; i < no_clusters; i++) {
                clusters[i] = new Van_Emde_Boas((int)ceil(sqrt((double)size)));
            }
        }
    }

    ~Van_Emde_Boas() {
        if (summary) delete summary;
        for (auto* c : clusters) delete c;
    }

    inline int high(int x) const { int d = (int)ceil(sqrt((double)universe_size)); return x / d; }
    inline int low(int x)  const { int m = (int)ceil(sqrt((double)universe_size)); return x % m; }
    inline int generate_index(int x, int y) const { int ru = (int)ceil(sqrt((double)universe_size)); return x * ru + y; }

    inline bool empty() const { return minimum == -1; }

    void empty_insert(int x) { minimum = maximum = x; }

    bool contains(int x) const {
        if (x == minimum || x == maximum) return true;
        if (universe_size <= 2) return false;
        int h = high(x), l = low(x);
        if (h < 0 || h >= (int)clusters.size()) return false;
        if (!clusters[h] || clusters[h]->empty()) return false;
        return clusters[h]->contains(l);
    }

    void insert(int x) {
        if (minimum == -1) { // empty tree
            empty_insert(x);
            return;
        }
        if (x < minimum) std::swap(x, minimum);

        if (universe_size > 2) {
            int h = high(x);
            int l = low(x);
            if (clusters[h]->minimum == -1) {
                summary->insert(h);
                clusters[h]->empty_insert(l);
            } else {
                clusters[h]->insert(l);
            }
        }
        if (x > maximum) maximum = x;
    }

    // Enumerate all keys stored in this VEB (demo only).
    void enumerate(vector<int>& out) const {
        if (minimum == -1) return;
        out.push_back(minimum);
        if (universe_size <= 2) {
            if (maximum != -1 && maximum != minimum) out.push_back(maximum);
            return;
        }
        int ru = (int)ceil(sqrt((double)universe_size));
        for (int h = 0; h < (int)clusters.size(); ++h) {
            if (!clusters[h] || clusters[h]->minimum == -1) continue;
            vector<int> child;
            clusters[h]->enumerate(child);
            for (int l : child) out.push_back(h * ru + l);
        }
    }
};

// ----------------------------- Arbor Porphyriana ------------------------------
struct Arbor {
    vector<vector<int>> adj;                    // adjacency list (undirected)
    unordered_map<string,int> id_of;            // label -> id
    vector<string> label_of;                    // id -> label
    vector<pair<int,int>> edge_log;             // edges in the order they were taught

    unique_ptr<Van_Emde_Boas> veb;              // VEB index
    int U;                                      // capacity / universe size

    explicit Arbor(int universe_size = 128): U(universe_size) {
        veb = std::make_unique<Van_Emde_Boas>(U);
    }

    int ensure_node(const string& label) {
        auto it = id_of.find(label);
        if (it != id_of.end()) return it->second;
        int id = (int)label_of.size();
        if (id >= U) throw runtime_error("Out of VEB universe capacity. Increase U.");
        id_of[label] = id;
        label_of.push_back(label);
        if ((int)adj.size() <= id) adj.resize(id + 1);
        veb->insert(id);
        return id;
    }

    void connect_parent_child(const string& parent, const string& child) {
        int p = ensure_node(parent);
        int c = ensure_node(child);
        adj[p].push_back(c);
        adj[c].push_back(p);
        edge_log.push_back({p, c});
    }

    // Dijkstra for unit weights (BFS equivalent but kept as requested).
    vector<int> shortest_path(const string& a, const string& b) const {
        auto ita = id_of.find(a), itb = id_of.find(b);
        if (ita == id_of.end() || itb == id_of.end()) return {};
        int s = ita->second, t = itb->second;
        int n = (int)label_of.size();
        const int INF = 1e9;
        vector<int> dist(n, INF), parent(n, -1);
        using P = pair<int,int>; // (dist, node)
        priority_queue<P, vector<P>, greater<P>> pq;
        dist[s] = 0; pq.push({0, s});
        while(!pq.empty()){
            P top = pq.top(); pq.pop();
            int d = top.first, u = top.second;
            if (d != dist[u]) continue;
            if (u == t) break;
            for(int v: adj[u]){
                if (dist[v] > d + 1){
                    dist[v] = d + 1;
                    parent[v] = u;
                    pq.push({dist[v], v});
                }
            }
        }
        if (dist[t] == INF) return {};
        vector<int> path;
        for (int cur = t; cur != -1; cur = parent[cur]) path.push_back(cur);
        reverse(path.begin(), path.end());
        return path;
    }

    void dump_veb_view() const {
        cout << "\n--- VEB View (U=" << U << ") ---\n";
        vector<int> keys; veb->enumerate(keys);
        sort(keys.begin(), keys.end()); keys.erase(unique(keys.begin(), keys.end()), keys.end());
        int ru = (int)ceil(sqrt((double)veb->universe_size));
        vector<vector<int>> by_cluster(ru);
        for (int k : keys) {
            int h = veb->high(k);
            if (h>=0 && h<ru) by_cluster[h].push_back(k);
        }
        for (int h = 0; h < ru; ++h) {
            if (by_cluster[h].empty()) continue;
            cout << "cluster[" << h << "] -> IDs: ";
            for (int k : by_cluster[h]) cout << k << ' ';
            cout << "\nlabels: ";
            for (int k : by_cluster[h]) {
                if (k >= 0 && k < (int)label_of.size()) cout << label_of[k] << ", ";
                else cout << "(unused:#" << k << "), ";
            }
            cout << "\n";
        }
        cout << "minID=" << veb->minimum << ", maxID=" << veb->maximum << "\n";
    }
};

struct PruneReport {
    vector<pair<int,int>> pruned_edges;
    vector<vector<int>> new_adj;           
    vector<int> node_dist;                 
};


PruneReport detect_and_prune_cycles_shortest_path(const Arbor& A, const string& preferred_root_label) {
    PruneReport report;
    int n = (int)A.label_of.size();
    vector<int> dist(n, -1), parent(n, -1);

    vector<int> order;
    auto itr = A.id_of.find(preferred_root_label);
    if (itr != A.id_of.end()) order.push_back(itr->second);
    for (int i = 0; i < n; ++i) order.push_back(i);

    for (int start : order) {
        if (dist[start] != -1) continue; 
        queue<int> q;
        dist[start] = 0;
        q.push(start);
        while (!q.empty()) {
            int u = q.front(); q.pop();
            for (int v : A.adj[u]) {
                if (dist[v] == -1) {
                    dist[v] = dist[u] + 1;
                    parent[v] = u;
                    q.push(v);
                }
            }
        }
    }

    set<pair<int,int>> tree_edges;
    for (int v = 0; v < n; ++v) {
        if (parent[v] != -1) {
            int u = parent[v];
            tree_edges.insert({min(u, v), max(u, v)});
        }
    }

    vector<vector<int>> new_adj(n);
    set<pair<int,int>> seen;
    for (int u = 0; u < n; ++u) {
        for (int v : A.adj[u]) {
            int a = min(u, v), b = max(u, v);
            if (seen.count({a, b})) continue;
            seen.insert({a, b});
            if (tree_edges.count({a, b})) {
                new_adj[a].push_back(b);
                new_adj[b].push_back(a);
            } else {
                report.pruned_edges.push_back({a, b});
            }
        }
    }

    report.new_adj = std::move(new_adj);
    report.node_dist = std::move(dist);
    return report;
}

struct DSU {
    vector<int> parent_, rank_;
    explicit DSU(int n): parent_(n), rank_(n, 0) { iota(parent_.begin(), parent_.end(), 0); }
    int find(int x) { while (parent_[x] != x) { parent_[x] = parent_[parent_[x]]; x = parent_[x]; } return x; }
    bool unite(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) return false; // already connected -> this edge would close a cycle
        if (rank_[a] < rank_[b]) swap(a, b);
        parent_[b] = a;
        if (rank_[a] == rank_[b]) rank_[a]++;
        return true;
    }
};

PruneReport detect_and_prune_cycles_insertion_order(const Arbor& A) {
    PruneReport report;
    int n = (int)A.label_of.size();
    DSU dsu(n);
    vector<vector<int>> new_adj(n);
    for (auto& e : A.edge_log) {
        int u = e.first, v = e.second;
        if (dsu.unite(u, v)) {
            new_adj[u].push_back(v);
            new_adj[v].push_back(u);
        } else {
            report.pruned_edges.push_back({min(u, v), max(u, v)});
        }
    }
    report.new_adj = std::move(new_adj);
    return report;
}

void commit_pruned_graph(Arbor& A, const PruneReport& rep) {
    A.adj = rep.new_adj;
}

void print_prune_report(const Arbor& A, const PruneReport& rep, const string& algo_name) {
    cout << "\n--- Cycle Detection & Pruning Report (" << algo_name << ") ---\n";
    if (rep.pruned_edges.empty()) {
        cout << "No redundant/cyclic edges found. Taxonomy is already a strict tree.\n";
        return;
    }
    cout << "Pruned " << rep.pruned_edges.size() << " edge(s):\n";
    for (auto& pr : rep.pruned_edges) {
        int u = pr.first, v = pr.second;
        string lu = (u >= 0 && u < (int)A.label_of.size()) ? A.label_of[u] : "#" + to_string(u);
        string lv = (v >= 0 && v < (int)A.label_of.size()) ? A.label_of[v] : "#" + to_string(v);
        cout << "  removed edge: " << lu << " -- " << lv << "\n";
    }
}

// ----------------------------- Sample Builders -------------------------------
void build_sample_animals(Arbor& A){
    A.connect_parent_child("substance", "body");
    A.connect_parent_child("substance", "incorporeal");

    // Body splits
    A.connect_parent_child("body", "living");
    A.connect_parent_child("body", "non_living");

    // Living splits
    A.connect_parent_child("living", "animal");
    A.connect_parent_child("living", "plant");

    // Animal splits by differentia
    A.connect_parent_child("animal", "rational_animal");
    A.connect_parent_child("animal", "irrational_animal");

    // Species under rational animal
    A.connect_parent_child("rational_animal", "man");
    A.connect_parent_child("rational_animal", "immortal_rational_animal");

    // Individuals under man
    A.connect_parent_child("man", "Plato");
    A.connect_parent_child("man", "Socrates");
    A.connect_parent_child("man", "Aristotle");

    // A few species under irrational animal (for contrast)
    A.connect_parent_child("irrational_animal", "equine");
    A.connect_parent_child("irrational_animal", "canine");
    A.connect_parent_child("irrational_animal", "bird");
    // An example of bird
    A.connect_parent_child("bird", "chicken");
}

// Synthetic N-level Porphyrian-style tree with branching factor B.
void build_synthetic_porhyry(Arbor& A, int levels, int B){
    if (levels <= 0) return;
    vector<string> prev;
    string root = "L1_0";
    A.ensure_node(root);
    prev.push_back(root);
    for (int lvl = 2; lvl <= levels; ++lvl) {
        vector<string> cur;
        for (const string& p : prev) {
            for (int b = 0; b < B; ++b) {
                string name = string("L") + to_string(lvl) + "_" + to_string((int)cur.size());
                A.connect_parent_child(p, name);
                cur.push_back(name);
            }
        }
        prev.swap(cur);
    }
}

// ------------------------------ Diagram Utils --------------------------------
static string join_labels(const vector<int>& ids, const vector<string>& labels, const string& sep = " -> "){
    string out;
    for (size_t i=0;i<ids.size();++i){
        int id = ids[i];
        out += (id>=0 && id<(int)labels.size()) ? labels[id] : (string("#")+to_string(id));
        if (i+1<ids.size()) out += sep;
    }
    return out;
}

// ASCII tree printing from a chosen root label (ASCII-only connectors).
void print_ascii_tree_from_root(const Arbor& A, const string& root_lbl){
    auto it = A.id_of.find(root_lbl);
    if (it == A.id_of.end()) { cerr << "[diagram] root label not found: " << root_lbl << "\n"; return; }
    int root = it->second;

    function<void(int,int,string,bool)> dfs = [&](int u, int parent, string prefix, bool last){
        cout << prefix;
        if (!prefix.empty()) cout << (last ? "+-" : "+-");
        cout << A.label_of[u] << "\n";

        vector<int> children = A.adj[u];
        if (parent != -1) children.erase(remove(children.begin(), children.end(), parent), children.end());
        for (size_t i=0;i<children.size();++i){
            bool is_last = (i+1==children.size());
            string next_prefix = prefix + (prefix.empty()? "" : (last? "  " : "| "));
            dfs(children[i], u, next_prefix, is_last);
        }
    };

    dfs(root, -1, "", true);
}


void emit_graphviz(const Arbor& A, const string& filename){
    ofstream ofs(filename);
    if (!ofs) { cerr << "[graphviz] cannot open: " << filename << "\n"; return; }
    ofs << "graph Porphyry {\n";
    ofs << "  rankdir=TB;\n";
    ofs << "  node [shape=box, style=rounded];\n";
    // Declare nodes
    for (size_t i=0;i<A.label_of.size();++i){
        ofs << "  n" << i << " [label=\"" << A.label_of[i] << "\"];\n";
    }
    // Undirected edges, avoid duplicates by u<v
    for (size_t u=0; u<A.adj.size(); ++u){
        for (int v : A.adj[u]) if ((int)u < v){
            ofs << "  n" << u << " -- n" << v << ";\n";
        }
    }
    ofs << "}\n";
    cerr << "[graphviz] wrote " << filename << " (render with: dot -Tpng " << filename << " -o porphyry.png)\n";
}

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    
    Arbor arbor(/*U=*/256);

    auto t_build0 = high_resolution_clock::now();
    build_sample_animals(arbor);
    auto t_build1 = high_resolution_clock::now();
    auto build_us = duration_cast<microseconds>(t_build1 - t_build0).count();

    cout << "Build time (sample animals): " << build_us << " us\n";

    arbor.connect_parent_child("substance", "chicken");
    arbor.connect_parent_child("man", "canine");

    auto t_prune0 = high_resolution_clock::now();
    PruneReport sp_report = detect_and_prune_cycles_shortest_path(arbor, "substance");
    auto t_prune1 = high_resolution_clock::now();
    auto sp_us = duration_cast<microseconds>(t_prune1 - t_prune0).count();

    auto t_prune2 = high_resolution_clock::now();
    PruneReport io_report = detect_and_prune_cycles_insertion_order(arbor);
    auto t_prune3 = high_resolution_clock::now();
    auto io_us = duration_cast<microseconds>(t_prune3 - t_prune2).count();

    print_prune_report(arbor, sp_report, "global shortest-path / BFS");
    cout << "Prune time: " << sp_us << " us\n";
    cout << "Note: this variant can re-parent nodes beyond the actual cycle if a\n"
            "bad edge creates a genuine shortcut near the root (see 'bird'/'irrational_animal'\n"
            "above -- adding substance->chicken made bird, then irrational_animal, reachable\n"
            "via a shorter route, which pruned the innocent animal--irrational_animal edge).\n";

    print_prune_report(arbor, io_report, "insertion-order / Union-Find");
    cout << "Prune time: " << io_us << " us\n";
    cout << "This variant only drops the two edges we deliberately injected\n"
            "(substance--chicken, man--canine) and leaves every taught edge untouched.\n";


    commit_pruned_graph(arbor, io_report);

    // --- VEB view ---
    arbor.dump_veb_view();

    // --- ASCII tree diagram (rooted at "substance", the actual root label) ---
    cout << "\nASCII Diagram (root=substance)\n";
    print_ascii_tree_from_root(arbor, "substance");

    // --- Graphviz DOT output (now cycle-free) ---
    emit_graphviz(arbor, "porphyry.dot");

    // --- Measure Dijkstra time for a sample query (graph is now a tree) ---
    auto t_d0 = high_resolution_clock::now();
    auto path = arbor.shortest_path("Plato", "chicken");
    auto t_d1 = high_resolution_clock::now();
    auto dijk_us = duration_cast<microseconds>(t_d1 - t_d0).count();

    if (path.empty()) {
        cout << "\nNo path found between Plato and a featherless chicken\n";
    } else {
        cout << "\nShortest path (Plato -> chicken):\n  " << join_labels(path, arbor.label_of) << "\n";
        int edges = (int)path.size() - 1;
        int nodes_between = max(0, (int)path.size() - 2);
        cout << "Edges (hops): " << edges << "\n";
        cout << "Nodes between terms (excluding endpoints): " << nodes_between << "\n";
        cout << "Dijkstra time: " << dijk_us << " us\n";
    }

    return 0;
}
