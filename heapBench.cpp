// benchmark_heap.cpp
// Compare un tas binaire (heap) avec d'autres conteneurs utilises comme file de priorite.
//
// Scenarios mesures (pour chaque taille N) :
//   1) INSERT  : inserer N entiers aleatoires
//   2) POP_ALL : extraire les N elements dans l'ordre (du plus petit au plus grand)
//   3) MIXED   : charge mixte (N/2 inserts initiaux, puis N operations alternant push/pop)
//
// Structures comparees :
//   - std::priority_queue (min-heap)       : tas binaire sur vector
//   - make_heap (heapify en O(n))          : construction en bloc puis pop
//   - tas 4-aire maison                    : meilleure localite cache
//   - vector trie (insertion triee)        : insert O(n), pop O(1) en fin
//   - vector non trie (scan lineaire)      : push O(1), pop O(n)
//   - std::list triee                      : insert O(n), pop O(1)
//   - std::multiset                        : arbre rouge-noir, O(log n)
//   - std::deque trie                      : insert O(n), pop O(1) en tete
//
// Compilation :
//   g++ -O2 -std=c++17 -o benchmark_heap benchmark_heap.cpp
// Utilisation :
//   ./benchmark_heap [repetitions=5]

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;
using Value = std::int64_t;

// Empeche le compilateur d'optimiser les resultats
static volatile Value g_sink = 0;

// ---------------------------------------------------------------
// Tas 4-aire (min-heap) ecrit a la main
// ---------------------------------------------------------------
class DaryHeap4 {
    std::vector<Value> a_;
public:
    void reserve(size_t n) { a_.reserve(n); }
    bool empty() const { return a_.empty(); }
    size_t size() const { return a_.size(); }
    const Value& top() const { return a_.front(); }

    void push(Value v) {
        a_.push_back(v);
        size_t i = a_.size() - 1;
        while (i > 0) {
            size_t p = (i - 1) / 4;
            if (a_[p] <= a_[i]) break;
            std::swap(a_[p], a_[i]);
            i = p;
        }
    }
    void pop() {
        a_.front() = a_.back();
        a_.pop_back();
        size_t n = a_.size(), i = 0;
        while (true) {
            size_t first = 4 * i + 1;
            if (first >= n) break;
            size_t last = std::min(first + 4, n);
            size_t best = first;
            for (size_t c = first + 1; c < last; ++c)
                if (a_[c] < a_[best]) best = c;
            if (a_[i] <= a_[best]) break;
            std::swap(a_[i], a_[best]);
            i = best;
        }
    }
};

// ---------------------------------------------------------------
// Adaptateurs : tous exposent push / pop_min / empty
// ---------------------------------------------------------------
struct StdHeap {
    std::priority_queue<Value, std::vector<Value>, std::greater<Value>> q;
    void reserve(size_t) {}
    void push(Value v) { q.push(v); }
    Value pop_min() { Value v = q.top(); q.pop(); return v; }
    bool empty() const { return q.empty(); }
};

struct Heap4 {
    DaryHeap4 h;
    void reserve(size_t n) { h.reserve(n); }
    void push(Value v) { h.push(v); }
    Value pop_min() { Value v = h.top(); h.pop(); return v; }
    bool empty() const { return h.empty(); }
};

// vector maintenu trie en ordre decroissant : le min est a la fin
struct SortedVector {
    std::vector<Value> v;
    void reserve(size_t n) { v.reserve(n); }
    void push(Value x) {
        auto it = std::lower_bound(v.begin(), v.end(), x, std::greater<Value>());
        v.insert(it, x);
    }
    Value pop_min() { Value x = v.back(); v.pop_back(); return x; }
    bool empty() const { return v.empty(); }
};

// vector non trie : scan lineaire pour trouver le min
struct UnsortedVector {
    std::vector<Value> v;
    void reserve(size_t n) { v.reserve(n); }
    void push(Value x) { v.push_back(x); }
    Value pop_min() {
        auto it = std::min_element(v.begin(), v.end());
        Value x = *it;
        *it = v.back();       // swap-and-pop : O(1) apres le scan
        v.pop_back();
        return x;
    }
    bool empty() const { return v.empty(); }
};

struct SortedList {
    std::list<Value> l;
    void reserve(size_t) {}
    void push(Value x) {
        auto it = std::find_if(l.begin(), l.end(), [x](Value y) { return y >= x; });
        l.insert(it, x);
    }
    Value pop_min() { Value x = l.front(); l.pop_front(); return x; }
    bool empty() const { return l.empty(); }
};

struct MultiSet {
    std::multiset<Value> s;
    void reserve(size_t) {}
    void push(Value x) { s.insert(x); }
    Value pop_min() { Value x = *s.begin(); s.erase(s.begin()); return x; }
    bool empty() const { return s.empty(); }
};

struct SortedDeque {
    std::deque<Value> d;
    void reserve(size_t) {}
    void push(Value x) {
        auto it = std::lower_bound(d.begin(), d.end(), x);
        d.insert(it, x);
    }
    Value pop_min() { Value x = d.front(); d.pop_front(); return x; }
    bool empty() const { return d.empty(); }
};

// ---------------------------------------------------------------
// Utilitaires de mesure
// ---------------------------------------------------------------
static std::vector<Value> make_data(size_t n, unsigned seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<Value> dist(0, 1'000'000'000);
    std::vector<Value> d(n);
    for (auto& x : d) x = dist(rng);
    return d;
}

static double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

static double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// Scenario 1 : insertion de N elements
template <class C>
double bench_insert(const std::vector<Value>& data) {
    C c;
    c.reserve(data.size());
    auto t0 = Clock::now();
    for (Value x : data) c.push(x);
    double t = ms_since(t0);
    g_sink = g_sink + (c.empty() ? 0 : 1);
    return t;
}

// Scenario 2 : insertion puis extraction totale (seule l'extraction est chronometree)
template <class C>
double bench_pop_all(const std::vector<Value>& data) {
    C c;
    c.reserve(data.size());
    for (Value x : data) c.push(x);
    Value acc = 0;
    auto t0 = Clock::now();
    while (!c.empty()) acc ^= c.pop_min();
    double t = ms_since(t0);
    g_sink = g_sink + acc;
    return t;
}

// Scenario 3 : charge mixte (N/2 pre-remplis, puis N operations push/pop alternees)
template <class C>
double bench_mixed(const std::vector<Value>& data) {
    C c;
    c.reserve(data.size());
    size_t half = data.size() / 2;
    for (size_t i = 0; i < half; ++i) c.push(data[i]);
    Value acc = 0;
    auto t0 = Clock::now();
    for (size_t i = half; i < data.size(); ++i) {
        c.push(data[i]);
        acc ^= c.pop_min();
    }
    double t = ms_since(t0);
    g_sink = g_sink + acc;
    return t;
}

// Cas particulier : make_heap (heapify en O(n)) puis pop_heap
static double bench_make_heap_pop_all(const std::vector<Value>& data, double& build_ms) {
    std::vector<Value> v = data;
    auto t0 = Clock::now();
    std::make_heap(v.begin(), v.end(), std::greater<Value>());
    build_ms = ms_since(t0);
    Value acc = 0;
    auto t1 = Clock::now();
    while (!v.empty()) {
        std::pop_heap(v.begin(), v.end(), std::greater<Value>());
        acc ^= v.back();
        v.pop_back();
    }
    double t = ms_since(t1);
    g_sink = g_sink + acc;
    return t;
}

// ---------------------------------------------------------------
// Execution
// ---------------------------------------------------------------
struct Row {
    std::string name;
    size_t max_n;              // taille max supportee (structures O(n^2) limitees)
    std::function<double(const std::vector<Value>&)> insert, pop_all, mixed;
};

template <class C>
Row make_row(const std::string& name, size_t max_n) {
    return {name, max_n, bench_insert<C>, bench_pop_all<C>, bench_mixed<C>};
}

static void print_header(const char* title) {
    std::cout << "\n=== " << title << " (ms, mediane) ===\n";
}

int main(int argc, char** argv) {
    int reps = (argc > 1) ? std::max(1, std::atoi(argv[1])) : 5;
    const std::vector<size_t> sizes = {1'000, 10'000, 100'000, 1'000'000};
    const size_t BIG = static_cast<size_t>(-1);

    std::vector<Row> rows = {
        make_row<StdHeap>("priority_queue (heap)", BIG),
        make_row<Heap4>("tas 4-aire maison", BIG),
        make_row<MultiSet>("multiset (arbre RB)", BIG),
        make_row<SortedVector>("vector trie", 100'000),
        make_row<SortedDeque>("deque trie", 100'000),
        make_row<UnsortedVector>("vector non trie", 100'000),
        make_row<SortedList>("list triee", 20'000),
    };

    std::cout << "Benchmark heap vs autres conteneurs  |  repetitions: " << reps << "\n";
    std::cout << "Les structures O(n^2) sont sautees au-dela d'une taille limite ('-').\n";

    struct Scenario {
        const char* title;
        std::function<double(const Row&, const std::vector<Value>&)> run;
    };
    std::vector<Scenario> scenarios = {
        {"INSERT N elements", [](const Row& r, const std::vector<Value>& d) { return r.insert(d); }},
        {"POP_ALL (extraction des N elements)", [](const Row& r, const std::vector<Value>& d) { return r.pop_all(d); }},
        {"MIXED (N/2 push+pop alternes)", [](const Row& r, const std::vector<Value>& d) { return r.mixed(d); }},
    };

    for (const auto& sc : scenarios) {
        print_header(sc.title);
        std::cout << std::left << std::setw(26) << "structure";
        for (size_t n : sizes) std::cout << std::right << std::setw(12) << n;
        std::cout << "\n" << std::string(26 + 12 * sizes.size(), '-') << "\n";

        for (const auto& r : rows) {
            std::cout << std::left << std::setw(26) << r.name;
            for (size_t n : sizes) {
                if (n > r.max_n) { std::cout << std::right << std::setw(12) << "-"; continue; }
                std::vector<double> times;
                for (int k = 0; k < reps; ++k) {
                    auto data = make_data(n, 42 + k);
                    times.push_back(sc.run(r, data));
                }
                std::cout << std::right << std::setw(12) << std::fixed << std::setprecision(3) << median(times);
            }
            std::cout << "\n";
        }

        // Ligne supplementaire pour POP_ALL : make_heap + pop_heap
        if (std::string(sc.title).rfind("POP_ALL", 0) == 0) {
            std::cout << std::left << std::setw(26) << "make_heap + pop_heap";
            for (size_t n : sizes) {
                std::vector<double> times, builds;
                for (int k = 0; k < reps; ++k) {
                    auto data = make_data(n, 42 + k);
                    double b = 0;
                    times.push_back(bench_make_heap_pop_all(data, b));
                    builds.push_back(b);
                }
                std::cout << std::right << std::setw(12) << std::fixed << std::setprecision(3) << median(times);
            }
            std::cout << "\n";
        }
    }

    // Construction en bloc : heapify O(n) vs N push successifs vs tri complet
    print_header("CONSTRUCTION en bloc de N elements");
    std::cout << std::left << std::setw(26) << "methode";
    for (size_t n : sizes) std::cout << std::right << std::setw(12) << n;
    std::cout << "\n" << std::string(26 + 12 * sizes.size(), '-') << "\n";

    auto build_row = [&](const char* name, std::function<double(const std::vector<Value>&)> f) {
        std::cout << std::left << std::setw(26) << name;
        for (size_t n : sizes) {
            std::vector<double> t;
            for (int k = 0; k < reps; ++k) t.push_back(f(make_data(n, 42 + k)));
            std::cout << std::right << std::setw(12) << std::fixed << std::setprecision(3) << median(t);
        }
        std::cout << "\n";
    };
    build_row("make_heap (O(n))", [](const std::vector<Value>& d) {
        std::vector<Value> v = d;
        auto t0 = Clock::now();
        std::make_heap(v.begin(), v.end());
        double t = ms_since(t0);
        g_sink = g_sink + v.front();
        return t;
    });
    build_row("N x push_heap (O(n log n))", [](const std::vector<Value>& d) {
        std::vector<Value> v;
        v.reserve(d.size());
        auto t0 = Clock::now();
        for (Value x : d) { v.push_back(x); std::push_heap(v.begin(), v.end()); }
        double t = ms_since(t0);
        g_sink = g_sink + v.front();
        return t;
    });
    build_row("sort (O(n log n))", [](const std::vector<Value>& d) {
        std::vector<Value> v = d;
        auto t0 = Clock::now();
        std::sort(v.begin(), v.end());
        double t = ms_since(t0);
        g_sink = g_sink + v.front();
        return t;
    });

    std::cout << "\n(checksum anti-optimisation: " << g_sink << ")\n";
    return 0;
}