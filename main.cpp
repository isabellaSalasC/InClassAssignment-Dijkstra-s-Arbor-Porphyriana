// Arbor Porphyriana modeled with a Van Emde Boas Tree (VEB)
// + Dijkstra (unit weights) with timing + ASCII & Graphviz diagrams (ASCII-only).
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
// 3) Measures and prints build time and Dijkstra time (shortest path between terms).
// 4) Prints a compact textual view of the VEB clusters with their labels.
// 5) Renders the taxonomy as:
//    - ASCII tree in the console (ASCII characters only for portability).
//    - Graphviz DOT file (porphyry.dot) for a clean diagram.
//

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <functional>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <cmath>
#include <memory>
#include <stdexcept>
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

// ----------------------------- Proposed Tree ----------------------------------
// The proposed tree is Trie Data structure, which is a prefix tree that stores strings bby characters along tree paths

struct TrieNode {
    unordered_map<char, TrieNode*> children;
    bool is_end_of_word;
    int id;

    TrieNode() {
        is_end_of_word = false;
        id = -1;
    }
};

class Trie {
    TrieNode* root;

    
public:

    void print_trie_helper(TrieNode* node, string word) const {
        if (node->is_end_of_word) {
            cout << "ID: " << node->id << ", Word: " << word << "\n";
        }
        for (const auto& child : node->children) {
            print_trie_helper(child.second, word + child.first);
        }
    }

    Trie() { root = new TrieNode(); }

    void insert(const string& word, int id) {
        TrieNode* curr = root;
        for (char c : word) {
            if (curr->children.find(c) == curr->children.end()) {
                curr->children[c] = new TrieNode();
            }
            curr = curr->children[c];
        }
        curr->is_end_of_word = true;
        curr->id = id;
    }
    int search(const string& word) const{
        TrieNode* curr = root;
        for (char c : word) {
            if (curr->children.find(c) == curr->children.end()) {
                return -1;
            }
            curr = curr->children[c];
        }
        if (curr->is_end_of_word) {
            return curr->id;
        }
        return -1;
    }

    void print_trie() const {
        print_trie_helper(root, "");
    }
};


// ----------------------------- Arbor Porphyriana ------------------------------
struct Arbor {
    vector<vector<int>> adj;                    // adjacency list (undirected)
    unordered_map<string,int> id_of;            // label -> id
    vector<string> label_of;                    // id -> label

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
            auto [d,u] = pq.top(); pq.pop();
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

    // BFS for unit weights (added alongside Dijkstra so both can be compared).
    // Same signature and same return format as shortest_path().
    vector<int> shortest_path_bfs(const string& a, const string& b) const {
        auto ita = id_of.find(a), itb = id_of.find(b);
        if (ita == id_of.end() || itb == id_of.end()) return {};
        int s = ita->second, t = itb->second;
        int n = (int)label_of.size();
        vector<char> visited(n, 0);
        vector<int> parent(n, -1);
        queue<int> q;
        visited[s] = 1; q.push(s);
        while(!q.empty()){
            int u = q.front(); q.pop();
            if (u == t) break;
            for(int v: adj[u]){
                if (!visited[v]){
                    visited[v] = 1;
                    parent[v] = u;
                    q.push(v);
                }
            }
        }
        if (!visited[t]) return {};
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

struct ArborTrie {
    vector<vector<int>> adj;                    // adjacency list (undirected)
    vector<string> label_of;                    // id -> label
    Trie trie;                                  // Trie index

    int ensure_node(const string& label) {
        int existing_id = trie.search(label);
        if (existing_id != -1) { return existing_id; }
        int id = (int)label_of.size();
        label_of.push_back(label);

        if ((int)adj.size() <= id) { adj.resize(id + 1); }

        trie.insert(label, id);

        return id;
    }

    void connect_parent_child(const string& parent, const string& child) {
        int p = ensure_node(parent);
        int c = ensure_node(child);
        adj[p].push_back(c);
        adj[c].push_back(p);
    }

    vector<int> shortest_path(const string& a, const string& b) const {
        int s = trie.search(a), t = trie.search(b);
        if (s == -1 || t == -1) return {};
        int n = (int)label_of.size();
        const int INF = 1e9;
        vector<int> dist(n, INF), parent(n, -1);
        using P = pair<int,int>; // (dist, node)
        priority_queue<P, vector<P>, greater<P>> pq;
        dist[s] = 0; pq.push({0, s});
        while(!pq.empty()){
            auto [d,u] = pq.top(); pq.pop();
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

    // BFS for unit weights (added alongside Dijkstra so both can be compared).
    // Same signature and same return format as shortest_path().
    vector<int> shortest_path_bfs(const string& a, const string& b) const {
        int s = trie.search(a), t = trie.search(b);
        if (s == -1 || t == -1) return {};
        int n = (int)label_of.size();
        vector<char> visited(n, 0);
        vector<int> parent(n, -1);
        queue<int> q;
        visited[s] = 1; q.push(s);
        while(!q.empty()){
            int u = q.front(); q.pop();
            if (u == t) break;
            for(int v: adj[u]){
                if (!visited[v]){
                    visited[v] = 1;
                    parent[v] = u;
                    q.push(v);
                }
            }
        }
        if (!visited[t]) return {};
        vector<int> path;
        for (int cur = t; cur != -1; cur = parent[cur]) path.push_back(cur);
        reverse(path.begin(), path.end());
        return path;
    }

    void dump_trie_view() const {
        cout << "\n--- Trie View ---\n";
        trie.print_trie();
    }
};


// ----------------------------- Sample Builders -------------------------------
void build_sample_animals(Arbor& A){
    A.connect_parent_child("substance", "body");
    A.connect_parent_child("substance", "incorporeal");   // optional sibling

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
    // (You could add "immortal_rational_animal" here if you want the classic ladder)

    // Individuals under man
    A.connect_parent_child("man", "Plato");
    A.connect_parent_child("man", "Socrates");
    A.connect_parent_child("man", "Aristotle");

    // A few species under irrational animal (for contrast)
    A.connect_parent_child("irrational_animal", "equine");
    A.connect_parent_child("irrational_animal", "canine");
    A.connect_parent_child("irrational_animal", "bird");
    // Kinds of bird
    //A.connect_parent_child("bird", "non-domestic");
    //A.connect_parent_child("bird", "domestic");
    // An example of bird
    A.connect_parent_child("bird", "chicken");
}

void build_sample_animals(ArborTrie& A) {
    A.connect_parent_child("substance", "body");
    A.connect_parent_child("substance", "incorporeal");

    A.connect_parent_child("body", "living");
    A.connect_parent_child("body", "non_living");

    A.connect_parent_child("living", "animal");
    A.connect_parent_child("living", "plant");

    A.connect_parent_child("animal", "rational_animal");
    A.connect_parent_child("animal", "irrational_animal");

    A.connect_parent_child("rational_animal", "man");
    A.connect_parent_child("rational_animal", "immortal_rational_animal");

    A.connect_parent_child("man", "Plato");
    A.connect_parent_child("man", "Socrates");
    A.connect_parent_child("man", "Aristotle");

    A.connect_parent_child("irrational_animal", "equine");
    A.connect_parent_child("irrational_animal", "canine");
    A.connect_parent_child("irrational_animal", "bird");

    A.connect_parent_child("bird", "chicken");
}

// Synthetic N-level Porphyrian-style tree with branching factor B.
// ---------------------- Custom corpus: animals + food -------------------------
// Our own corpus, mixing THREE different terminologies over the same terms:
//   1) biological taxonomy  : vertebrate > mammal > carnivore > felid > cat
//   2) functional categories: flying_animal, aquatic_animal
//   3) culinary terminology : food > animal_food > dairy > milk
//
// Because a term can belong to more than one of them, the graph is NOT a tree:
// it contains cycles. Every edge marked [CYCLE] below closes a loop, because the
// term it points at already has another parent somewhere else.
//
// Template so the exact same corpus feeds both index structures (VEB and Trie)
// without keeping two copies that could drift apart.
template <class G>
void build_corpus(G& A){
    // --- 1) Biological taxonomy ---
    A.connect_parent_child("animal", "vertebrate");
    A.connect_parent_child("vertebrate", "mammal");
    A.connect_parent_child("vertebrate", "bird");
    A.connect_parent_child("vertebrate", "fish");

    A.connect_parent_child("mammal", "carnivore");
    A.connect_parent_child("carnivore", "felid");
    A.connect_parent_child("felid", "cat");
    A.connect_parent_child("felid", "lion");
    A.connect_parent_child("felid", "tiger");
    A.connect_parent_child("carnivore", "canid");
    A.connect_parent_child("canid", "dog");
    A.connect_parent_child("canid", "wolf");
    A.connect_parent_child("canid", "fox");

    A.connect_parent_child("mammal", "primate");
    A.connect_parent_child("primate", "hominid");
    A.connect_parent_child("hominid", "human");
    A.connect_parent_child("hominid", "gorilla");
    A.connect_parent_child("hominid", "chimpanzee");

    A.connect_parent_child("mammal", "cetacean");
    A.connect_parent_child("cetacean", "whale");
    A.connect_parent_child("cetacean", "dolphin");

    A.connect_parent_child("mammal", "chiroptera");
    A.connect_parent_child("chiroptera", "bat");

    A.connect_parent_child("mammal", "rodent");
    A.connect_parent_child("rodent", "mouse");
    A.connect_parent_child("rodent", "rat");
    A.connect_parent_child("rodent", "squirrel");

    A.connect_parent_child("bird", "penguin");
    A.connect_parent_child("bird", "eagle");
    A.connect_parent_child("bird", "chicken");

    A.connect_parent_child("fish", "salmon");
    A.connect_parent_child("fish", "tuna");

    // --- 2) Functional categories (cut across the taxonomy) ---
    A.connect_parent_child("animal", "flying_animal");
    A.connect_parent_child("flying_animal", "bat");        // [CYCLE 1] also under chiroptera
    A.connect_parent_child("flying_animal", "eagle");      // [CYCLE 2] also under bird

    A.connect_parent_child("animal", "aquatic_animal");
    A.connect_parent_child("aquatic_animal", "whale");     // [CYCLE 3] also under cetacean
    A.connect_parent_child("aquatic_animal", "dolphin");   // [CYCLE 4] also under cetacean
    A.connect_parent_child("aquatic_animal", "penguin");   // [CYCLE 5] also under bird
    A.connect_parent_child("aquatic_animal", "salmon");    // [CYCLE 6] also under fish
    A.connect_parent_child("aquatic_animal", "tuna");      // [CYCLE 7] also under fish

    // --- 3) Culinary terminology ---
    A.connect_parent_child("food", "plant_food");
    A.connect_parent_child("plant_food", "fruit");
    A.connect_parent_child("fruit", "apple");
    A.connect_parent_child("fruit", "banana");
    A.connect_parent_child("fruit", "tomato");
    A.connect_parent_child("plant_food", "vegetable");
    A.connect_parent_child("vegetable", "carrot");
    A.connect_parent_child("vegetable", "lettuce");
    A.connect_parent_child("vegetable", "tomato");         // [CYCLE 8] fruit botanically, vegetable in cooking
    A.connect_parent_child("plant_food", "grain");
    A.connect_parent_child("grain", "rice");
    A.connect_parent_child("grain", "wheat");

    A.connect_parent_child("food", "animal_food");
    A.connect_parent_child("animal_food", "meat");
    A.connect_parent_child("meat", "beef");
    A.connect_parent_child("meat", "pork");
    A.connect_parent_child("meat", "poultry");
    A.connect_parent_child("poultry", "chicken");          // [CYCLE 9] also a bird - links both domains

    A.connect_parent_child("animal_food", "dairy");
    A.connect_parent_child("dairy", "cheese");
    A.connect_parent_child("dairy", "milk");
    A.connect_parent_child("mammal", "milk");              // [CYCLE 10] mammals are what produces it

    A.connect_parent_child("animal_food", "seafood");
    A.connect_parent_child("seafood", "salmon");           // [CYCLE 11] also a fish
    A.connect_parent_child("seafood", "tuna");             // [CYCLE 12] also a fish
}

// Number of independent cycles in an undirected graph (its cyclomatic number):
//   cycles = edges - nodes + connected_components
// A tree scores 0. Anything above 0 means there is more than one route between
// some pair of terms, which is exactly what makes BFS vs Dijkstra interesting.
template <class G>
int count_independent_cycles(const G& A){
    int n = (int)A.label_of.size();
    long long deg_sum = 0;
    for (int i = 0; i < n; ++i) deg_sum += (long long)A.adj[i].size();
    int edges = (int)(deg_sum / 2);   // each edge is stored twice (undirected)

    // count connected components with a simple flood fill
    vector<char> seen(n, 0);
    int components = 0;
    for (int i = 0; i < n; ++i){
        if (seen[i]) continue;
        ++components;
        queue<int> q; q.push(i); seen[i] = 1;
        while(!q.empty()){
            int u = q.front(); q.pop();
            for (int v : A.adj[u]) if (!seen[v]) { seen[v] = 1; q.push(v); }
        }
    }
    return edges - n + components;
}

// Lists the edges that actually close a cycle, using union-find: walk every
// edge once, and if both endpoints are already in the same component then this
// edge creates a second route between them - i.e. it closes a cycle.
// The number of such edges always equals count_independent_cycles().
template <class G>
void report_cycle_edges(const G& A){
    int n = (int)A.label_of.size();
    vector<int> par(n);
    for (int i = 0; i < n; ++i) par[i] = i;
    function<int(int)> find = [&](int x){ while (par[x] != x) { par[x] = par[par[x]]; x = par[x]; } return x; };

    int found = 0;
    cout << "Edges that close a cycle (a term reachable by two different routes):\n";
    for (int u = 0; u < n; ++u){
        for (int v : A.adj[u]){
            if (u >= v) continue;                 // visit each undirected edge once
            int ru = find(u), rv = find(v);
            if (ru == rv) {
                ++found;
                cout << "  " << found << ") " << A.label_of[u] << " -- " << A.label_of[v] << "\n";
            } else {
                par[ru] = rv;
            }
        }
    }
    if (!found) cout << "  (none - the graph is a tree)\n";
}

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
void print_ascii_tree_from_root(const ArborTrie& A, const string& root_lbl){

    int root = A.trie.search(root_lbl);

    if (root == -1) {
        cerr << "[diagram] root label not found: " << root_lbl << "\n"; 
        return;
    }


    function<void(int,int,string,bool)> dfs = [&](int u, int parent, string prefix, bool last){
        cout << prefix;

        if (parent != -1) {
            cout << "+- ";
        }

        cout << A.label_of[u] << "\n";

        vector<int> children = A.adj[u];
        if (parent != -1) children.erase(remove(children.begin(), children.end(), parent), children.end());
        for (size_t i=0;i<children.size();++i){
            bool is_last = (i+1==children.size());
            string next_prefix = prefix;
            if (parent != -1) {
                next_prefix += (last ? "   " : "|  ");
            }
            dfs(children[i], u, next_prefix, is_last);
        }
    };

    dfs(root, -1, "", true);
}

// Graphviz DOT emitter (undirected). Writes to porphyry.dot
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

    // Universe size: set comfortably larger than total nodes you plan to add.
    Arbor arborV;
    ArborTrie arborT;

    // --- Measure build time for the sample animal taxonomy for VEB---
    auto veb_build_s = high_resolution_clock::now();
    build_sample_animals(arborV);
    auto veb_build_e = high_resolution_clock::now();
    auto build_us = duration_cast<microseconds>(veb_build_e - veb_build_s).count();

    auto trie_build_s = high_resolution_clock::now();
    build_sample_animals(arborT);
    auto trie_build_e = high_resolution_clock::now();
    auto trie_build_us = duration_cast<microseconds>(trie_build_e - trie_build_s).count();

    cout << "VEB Build time (sample animals): " << build_us << " us\n";
    cout << "Trie build time: " << trie_build_us << " us\n";

    // --- VEB view ---
    arborV.dump_veb_view();

    // --- Trie view ---
    arborT.dump_trie_view();

    // --- ASCII tree diagram (rooted at "substance") ---
    cout << "\nASCII Diagram (root=substance) with Trie\n";
    print_ascii_tree_from_root(arborT, "substance");

    // --- Graphviz DOT output ---
    emit_graphviz(arborV, "porphyry.dot");

    // --- Measure Dijkstra time for a sample query ---
    auto t_d0 = high_resolution_clock::now();
    auto path = arborV.shortest_path("Plato", "chicken");
    auto t_d1 = high_resolution_clock::now();
    auto dijk_us = duration_cast<microseconds>(t_d1 - t_d0).count();

    if (path.empty()) {
        cout << "\nNo path found between Plato and a featherless chicken\n";
    } else {
        cout << "\nShortest path (Plato -> chicken) [VEB]:\n  " << join_labels(path, arborV.label_of) << "\n";
        int edges = (int)path.size() - 1;
        int nodes_between = max(0, (int)path.size() - 2);
        cout << "Edges (hops): " << edges << "\n";
        cout << "Nodes between terms (excluding endpoints): " << nodes_between << "\n";
        cout << "Dijkstra time: " << dijk_us << " us\n";
    }

    auto trie_path_s = high_resolution_clock::now();
    auto trie_path = arborT.shortest_path("Plato", "chicken");
    auto trie_path_e = high_resolution_clock::now();
    auto trie_dijk_us = duration_cast<microseconds>(trie_path_e - trie_path_s).count();

    if (trie_path.empty()) {
        cout << "\nNo path found between Plato and a featherless chicken (Trie)\n";
    } else {
        cout << "\nShortest path (Plato -> chicken) [Trie]:\n  " << join_labels(trie_path, arborT.label_of) << "\n";
        int edges = (int)trie_path.size() - 1;
        int nodes_between = max(0, (int)trie_path.size() - 2);
        cout << "Edges (hops): " << edges << "\n";
        cout << "Nodes between terms (excluding endpoints): " << nodes_between << "\n";
        cout << "Trie Dijkstra time: " << trie_dijk_us << " us\n";
    }

    // --- Measure BFS time for the same query (VEB-indexed) ---
    auto veb_bfs_s = high_resolution_clock::now();
    auto veb_bfs_path = arborV.shortest_path_bfs("Plato", "chicken");
    auto veb_bfs_e = high_resolution_clock::now();
    auto veb_bfs_us = duration_cast<microseconds>(veb_bfs_e - veb_bfs_s).count();

    if (veb_bfs_path.empty()) {
        cout << "\nNo path found between Plato and a featherless chicken (VEB, BFS)\n";
    } else {
        cout << "\nShortest path (Plato -> chicken) [VEB, BFS]:\n  " << join_labels(veb_bfs_path, arborV.label_of) << "\n";
        int edges = (int)veb_bfs_path.size() - 1;
        int nodes_between = max(0, (int)veb_bfs_path.size() - 2);
        cout << "Edges (hops): " << edges << "\n";
        cout << "Nodes between terms (excluding endpoints): " << nodes_between << "\n";
        cout << "VEB BFS time: " << veb_bfs_us << " us\n";
    }

    // --- Measure BFS time for the same query (Trie-indexed) ---
    auto trie_bfs_s = high_resolution_clock::now();
    auto trie_bfs_path = arborT.shortest_path_bfs("Plato", "chicken");
    auto trie_bfs_e = high_resolution_clock::now();
    auto trie_bfs_us = duration_cast<microseconds>(trie_bfs_e - trie_bfs_s).count();

    if (trie_bfs_path.empty()) {
        cout << "\nNo path found between Plato and a featherless chicken (Trie, BFS)\n";
    } else {
        cout << "\nShortest path (Plato -> chicken) [Trie, BFS]:\n  " << join_labels(trie_bfs_path, arborT.label_of) << "\n";
        int edges = (int)trie_bfs_path.size() - 1;
        int nodes_between = max(0, (int)trie_bfs_path.size() - 2);
        cout << "Edges (hops): " << edges << "\n";
        cout << "Nodes between terms (excluding endpoints): " << nodes_between << "\n";
        cout << "Trie BFS time: " << trie_bfs_us << " us\n";
    }

    // ================= OUR OWN CORPUS (animals + food, with cycles) =================
    cout << "\n\n========== CUSTOM CORPUS: animals + food ==========\n";
    Arbor corpusV(/*U=*/512);
    ArborTrie corpusT;

    auto cv_s = high_resolution_clock::now();
    build_corpus(corpusV);
    auto cv_e = high_resolution_clock::now();
    auto cv_build_us = duration_cast<microseconds>(cv_e - cv_s).count();

    auto ct_s = high_resolution_clock::now();
    build_corpus(corpusT);
    auto ct_e = high_resolution_clock::now();
    auto ct_build_us = duration_cast<microseconds>(ct_e - ct_s).count();

    cout << "Terms: " << corpusV.label_of.size() << "\n";
    cout << "Build time: VEB " << cv_build_us << " us | Trie " << ct_build_us << " us\n";

    int cycles = count_independent_cycles(corpusV);
    cout << "Independent cycles: " << cycles
         << (cycles >= 3 ? "  (requirement of >= 3 met)" : "  (NOT ENOUGH)") << "\n\n";
    report_cycle_edges(corpusV);

    // Queries chosen so the answer depends on the cycles: each of these terms is
    // reachable through two different categories, so a shortcut exists.
    struct Q { const char* a; const char* b; const char* why; };
    Q queries[] = {
        {"bat",    "eagle",  "both are flying_animal - shortcut across the taxonomy"},
        {"cat",    "milk",   "milk is both dairy and a mammal product"},
        {"tomato", "beef",   "tomato is both fruit and vegetable"},
        {"whale",  "tuna",   "both are aquatic_animal"},
        {"dog",    "rice",   "crosses from the animal domain into the food domain"},
    };

    cout << "\n--- Dijkstra vs BFS on the corpus ---\n";
    long long sum_d = 0, sum_b = 0;
    for (auto& q : queries){
        auto d0 = high_resolution_clock::now();
        auto pd = corpusV.shortest_path(q.a, q.b);
        auto d1 = high_resolution_clock::now();
        auto pb_t0 = high_resolution_clock::now();
        auto pb = corpusV.shortest_path_bfs(q.a, q.b);
        auto pb_t1 = high_resolution_clock::now();

        auto dus = duration_cast<nanoseconds>(d1 - d0).count();
        auto bus = duration_cast<nanoseconds>(pb_t1 - pb_t0).count();
        sum_d += dus; sum_b += bus;

        cout << "\n" << q.a << " -> " << q.b << "   (" << q.why << ")\n";
        cout << "  Dijkstra: " << join_labels(pd, corpusV.label_of) << "\n";
        cout << "  BFS     : " << join_labels(pb, corpusV.label_of) << "\n";
        cout << "  hops: " << (int)pb.size()-1
             << " | same result: " << ((pd.size()==pb.size()) ? "yes" : "NO")
             << " | Dijkstra " << dus << " ns vs BFS " << bus << " ns\n";
    }
    cout << "\nTotal over " << (int)(sizeof(queries)/sizeof(queries[0])) << " queries: "
         << "Dijkstra " << sum_d << " ns | BFS " << sum_b << " ns\n";

    // --- Dijkstra vs BFS summary ---
    cout << "\n--- Dijkstra vs BFS (Plato -> chicken) ---\n";
    cout << "VEB  : Dijkstra " << dijk_us      << " us | BFS " << veb_bfs_us  << " us\n";
    cout << "Trie : Dijkstra " << trie_dijk_us << " us | BFS " << trie_bfs_us << " us\n";
    cout << "All four paths identical: "
         << ((path == veb_bfs_path && trie_path == trie_bfs_path && path == trie_path) ? "yes" : "no")
         << "\n";

    return 0;
}