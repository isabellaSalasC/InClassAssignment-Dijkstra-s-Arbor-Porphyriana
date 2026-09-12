// Test suite for the BFS shortest-path implementation.
//
// Build & run:
//   g++ -std=c++17 -O2 -o test_bfs test_bfs.cpp && ./test_bfs
//
// Exit code is 0 if every test passes, 1 otherwise (so CI / scripts can use it).
//
// How it works: it includes main.cpp directly and renames that file's main()
// out of the way, so the tests run against the real implementation instead of
// a copy that could drift out of sync.

#define main program_main
#include "main.cpp"
#undef main

// ------------------------------ Test helpers ---------------------------------
static int failures = 0;

static void check(bool cond, const string& what){
    if (!cond) { cout << "  FAIL: " << what << "\n"; ++failures; }
}

// A path is only valid if it starts at s, ends at t, and every consecutive pair
// is a real edge. Without this check, a function returning a made-up {s, t}
// would still pass a length-only comparison.
template <class G>
static bool path_is_valid(const G& g, const vector<int>& p, int s, int t){
    if (p.empty()) return false;
    if (p.front() != s || p.back() != t) return false;
    for (size_t i = 0; i + 1 < p.size(); ++i){
        const vector<int>& nb = g.adj[p[i]];
        if (find(nb.begin(), nb.end(), p[i+1]) == nb.end()) return false;
    }
    return true;
}

int main(){
    Arbor V; ArborTrie T;
    build_sample_animals(V);
    build_sample_animals(T);
    int n = (int)V.label_of.size();
    cout << "Nodes: " << n << "\n\n";

    // TEST 1: exhaustive cross-check. Dijkstra is the trusted reference, so if
    // BFS agrees with it on every pair and returns real paths, BFS is correct.
    cout << "TEST 1: BFS vs Dijkstra on all " << n*n << " pairs\n";
    int pairs = 0;
    for (int i = 0; i < n; ++i){
        for (int j = 0; j < n; ++j){
            const string& a = V.label_of[i];
            const string& b = V.label_of[j];
            auto dV = V.shortest_path(a,b), bV = V.shortest_path_bfs(a,b);
            auto dT = T.shortest_path(a,b), bT = T.shortest_path_bfs(a,b);
            check(dV.size()==bV.size(), "VEB length mismatch "+a+"->"+b);
            check(dT.size()==bT.size(), "Trie length mismatch "+a+"->"+b);
            check(bV.size()==bT.size(), "VEB/Trie BFS disagree "+a+"->"+b);
            check(path_is_valid(V,bV,i,j), "VEB BFS returned an invalid path "+a+"->"+b);
            check(path_is_valid(T,bT,i,j), "Trie BFS returned an invalid path "+a+"->"+b);
            ++pairs;
        }
    }
    cout << "  compared " << pairs << " pairs\n\n";

    // TEST 2: distances worked out by hand from the taxonomy. Needed because if
    // Dijkstra and BFS shared the same bug, TEST 1 alone would not catch it.
    cout << "TEST 2: hand-checked distances\n";
    struct Case { const char* a; const char* b; int hops; };
    Case cases[] = {
        {"Plato","Socrates",2},     // siblings, both children of man
        {"Plato","man",1},          // direct parent
        {"substance","chicken",6},  // substance>body>living>animal>irrational_animal>bird>chicken
        {"Plato","Plato",0},        // a node to itself
        {"man","animal",2},         // man>rational_animal>animal
    };
    for (auto& c : cases){
        auto p = V.shortest_path_bfs(c.a, c.b);
        int hops = p.empty() ? -1 : (int)p.size()-1;
        cout << "  " << c.a << " -> " << c.b << " : " << hops << " (expected " << c.hops << ")\n";
        check(hops == c.hops, string("wrong distance ")+c.a+"->"+c.b);
    }
    cout << "\n";

    // TEST 3: bad input must return empty instead of crashing or lying.
    cout << "TEST 3: edge cases\n";
    check(V.shortest_path_bfs("Plato","Plato").size()==1, "self-path should be just the node");
    check(V.shortest_path_bfs("nope","Plato").empty(), "unknown source must return empty");
    check(V.shortest_path_bfs("Plato","nope").empty(), "unknown target must return empty");
    check(T.shortest_path_bfs("nope","nope").empty(), "both unknown must return empty");
    cout << "  done\n\n";

    // TEST 4: with two separate components, BFS must report no path rather
    // than inventing one.
    cout << "TEST 4: disconnected components\n";
    Arbor D;
    D.connect_parent_child("island_a","island_b");   // component 1
    D.connect_parent_child("island_c","island_d");   // component 2
    check(D.shortest_path_bfs("island_a","island_b").size()==2, "connected pair should have a path");
    check(D.shortest_path_bfs("island_a","island_c").empty(), "disconnected pair must return empty");
    check(D.shortest_path("island_a","island_c").empty(), "Dijkstra agrees: no path");
    cout << "  done\n\n";

    // TEST 5: a graph with a CYCLE, where two routes of different length exist.
    // The sample taxonomy is a tree, and in a tree there is exactly one simple
    // path between any two nodes - so tests 1-4 cannot tell a correct BFS apart
    // from a depth-first search. This graph can: it has a short route and a long
    // one, and only a genuine breadth-first search returns the short one.
    //
    //   start -- x -- target        (2 hops, the shortest)
    //   start -- y -- z -- target   (3 hops, the detour)
    cout << "TEST 5: shortest route when several routes exist\n";
    Arbor C;
    C.connect_parent_child("start","x");
    C.connect_parent_child("start","y");
    C.connect_parent_child("x","target");
    C.connect_parent_child("y","z");
    C.connect_parent_child("z","target");

    auto cb = C.shortest_path_bfs("start","target");
    auto cd = C.shortest_path("start","target");
    int bfs_hops = cb.empty() ? -1 : (int)cb.size()-1;
    int dij_hops = cd.empty() ? -1 : (int)cd.size()-1;
    cout << "  BFS      : " << join_labels(cb, C.label_of) << "  (" << bfs_hops << " hops)\n";
    cout << "  Dijkstra : " << join_labels(cd, C.label_of) << "  (" << dij_hops << " hops)\n";
    check(bfs_hops == 2, "BFS must take the 2-hop route, not the 3-hop detour");
    check(dij_hops == 2, "Dijkstra must take the 2-hop route too");
    check(path_is_valid(C, cb, C.id_of.at("start"), C.id_of.at("target")), "BFS path must be a real walk");
    cout << "\n";

    // TEST 6: the custom corpus (animals + food). Unlike the sample taxonomy this
    // one has real cycles, so several routes exist between many pairs and BFS has
    // to pick the shortest rather than just the only one.
    cout << "TEST 6: custom corpus with cycles\n";
    Arbor KV(512); ArborTrie KT;
    build_corpus(KV);
    build_corpus(KT);
    int kn = (int)KV.label_of.size();
    int kcycles = count_independent_cycles(KV);
    cout << "  terms: " << kn << ", independent cycles: " << kcycles << "\n";
    check(kcycles >= 3, "corpus must contain at least 3 cycles");
    check(kn == (int)KT.label_of.size(), "VEB and Trie corpora must have the same size");

    int kpairs = 0, unreachable = 0;
    for (int i = 0; i < kn; ++i){
        for (int j = 0; j < kn; ++j){
            const string& a = KV.label_of[i];
            const string& b = KV.label_of[j];
            auto dV = KV.shortest_path(a,b), bV = KV.shortest_path_bfs(a,b);
            auto dT = KT.shortest_path(a,b), bT = KT.shortest_path_bfs(a,b);
            check(dV.size()==bV.size(), "corpus VEB length mismatch "+a+"->"+b);
            check(dT.size()==bT.size(), "corpus Trie length mismatch "+a+"->"+b);
            check(bV.size()==bT.size(), "corpus VEB/Trie BFS disagree "+a+"->"+b);
            check(path_is_valid(KV,bV,i,j), "corpus BFS returned an invalid path "+a+"->"+b);
            if (bV.empty()) ++unreachable;
            ++kpairs;
        }
    }
    cout << "  compared " << kpairs << " pairs (" << unreachable << " with no path)\n";

    // Shortcuts created by the cycles: each of these is short ONLY because the
    // term belongs to two categories at once. A traversal that ignored the
    // shortcut would report a longer path.
    struct KC { const char* a; const char* b; int hops; };
    KC kcases[] = {
        {"bat","eagle",2},        // via flying_animal, not via mammal>...>bird
        {"whale","tuna",2},       // via aquatic_animal, not via mammal>...>fish
        {"whale","dolphin",2},    // via cetacean or aquatic_animal, both 2
        {"tomato","carrot",2},    // via vegetable
        {"cat","milk",4},         // cat>felid>carnivore>mammal>milk
    };
    for (auto& c : kcases){
        auto pv = KV.shortest_path_bfs(c.a, c.b);
        int hops = pv.empty() ? -1 : (int)pv.size()-1;
        cout << "  " << c.a << " -> " << c.b << " : " << hops << " (expected " << c.hops << ")\n";
        check(hops == c.hops, string("corpus wrong distance ")+c.a+"->"+c.b);
    }
    cout << "\n";

    cout << (failures ? "*** " + to_string(failures) + " FAILURES ***" : "ALL TESTS PASSED") << "\n";
    return failures ? 1 : 0;
}
