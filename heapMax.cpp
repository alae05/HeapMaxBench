// Tas max : 3 methodes comparees
//   1) Tas manuel sur tableau brut (new[] / delete[])  -> sans conteneur
//   2) Tas manuel sur std::vector                       -> avec conteneur
//   3) std::priority_queue                              -> conteneur STL pret a l'emploi
// Comparaison : insertion et extraction de la racine (plus grande valeur)
//
// Compilation : g++ -O2 -std=c++17 tas_max_3_methodes.cpp -o tas_max
// Utilisation : ./tas_max [repetitions=5]

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <queue>
#include <random>
#include <vector>

using namespace std;
using Horloge = chrono::steady_clock;

// ============================================================
// Methode 1 : tas max sur tableau dynamique brut (sans conteneur)
// ============================================================
class TasMaxTableau {
    int* tab;
    size_t taille;
    size_t capacite;

    void agrandir() {
        size_t nouvelle = capacite * 2;
        int* nouveau = new int[nouvelle];
        for (size_t i = 0; i < taille; i++) nouveau[i] = tab[i];
        delete[] tab;
        tab = nouveau;
        capacite = nouvelle;
    }

public:
    TasMaxTableau(size_t cap = 16) : tab(new int[cap]), taille(0), capacite(cap) {}
    ~TasMaxTableau() { delete[] tab; }
    TasMaxTableau(const TasMaxTableau&) = delete;
    TasMaxTableau& operator=(const TasMaxTableau&) = delete;

    bool vide() const { return taille == 0; }
    int sommet() const { return tab[0]; }

    void inserer(int valeur) {
        if (taille == capacite) agrandir();
        size_t i = taille++;
        tab[i] = valeur;
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (tab[parent] >= tab[i]) break;
            swap(tab[parent], tab[i]);
            i = parent;
        }
    }

    void extraireMax() {
        tab[0] = tab[--taille];
        size_t i = 0;
        while (true) {
            size_t g = 2 * i + 1, d = 2 * i + 2, max = i;
            if (g < taille && tab[g] > tab[max]) max = g;
            if (d < taille && tab[d] > tab[max]) max = d;
            if (max == i) break;
            swap(tab[i], tab[max]);
            i = max;
        }
    }
};

// ============================================================
// Methode 2 : tas max ecrit a la main, stocke dans un vector
// ============================================================
class TasMaxVector {
    vector<int> tas;

public:
    bool vide() const { return tas.empty(); }
    int sommet() const { return tas.front(); }

    void inserer(int valeur) {
        tas.push_back(valeur);
        size_t i = tas.size() - 1;
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (tas[parent] >= tas[i]) break;
            swap(tas[parent], tas[i]);
            i = parent;
        }
    }

    void extraireMax() {
        tas[0] = tas.back();
        tas.pop_back();
        size_t n = tas.size(), i = 0;
        while (true) {
            size_t g = 2 * i + 1, d = 2 * i + 2, max = i;
            if (g < n && tas[g] > tas[max]) max = g;
            if (d < n && tas[d] > tas[max]) max = d;
            if (max == i) break;
            swap(tas[i], tas[max]);
            i = max;
        }
    }
};

// ============================================================
// Methode 3 : priority_queue (adaptateur d'un tas max)
// ============================================================
class TasMaxPQ {
    priority_queue<int> pq;

public:
    bool vide() const { return pq.empty(); }
    int sommet() const { return pq.top(); }
    void inserer(int valeur) { pq.push(valeur); }
    void extraireMax() { pq.pop(); }
};

// ============================================================
// Outils de mesure
// ============================================================
struct Temps {
    double insertion;
    double extraction;
};

static double ms(Horloge::time_point debut) {
    return chrono::duration<double, milli>(Horloge::now() - debut).count();
}

static double mediane(vector<double> v) {
    sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// Test generique : fonctionne pour les 3 methodes
template <class Tas>
static Temps tester(const vector<int>& donnees, vector<int>& sortie) {
    Tas tas;
    sortie.clear();
    sortie.reserve(donnees.size());

    auto t0 = Horloge::now();
    for (int x : donnees) tas.inserer(x);
    double ins = ms(t0);

    t0 = Horloge::now();
    while (!tas.vide()) {
        sortie.push_back(tas.sommet());
        tas.extraireMax();
    }
    double ext = ms(t0);
    return {ins, ext};
}

int main(int argc, char** argv) {
    int reps = (argc > 1) ? max(1, atoi(argv[1])) : 5;
    const vector<size_t> tailles = {1000, 10000, 100000, 1000000};
    bool toutOk = true;

    cout << "Tas max : 3 methodes | repetitions : " << reps << " (mediane, en ms)\n";

    // Deux tableaux : un pour l'insertion, un pour l'extraction
    const char* titres[2] = {"INSERTION", "EXTRACTION DE LA RACINE"};
    vector<vector<double>> resultats[2];   // [op][taille][methode]
    for (auto& r : resultats) r.assign(tailles.size(), vector<double>(3, 0.0));

    for (size_t t = 0; t < tailles.size(); ++t) {
        size_t n = tailles[t];
        vector<double> ins[3], ext[3];
        vector<int> s1, s2, s3;

        for (int k = 0; k < reps; ++k) {
            mt19937 gen(42 + k);
            uniform_int_distribution<int> dist(1, 1000000000);
            vector<int> donnees(n);
            for (auto& x : donnees) x = dist(gen);

            Temps a = tester<TasMaxTableau>(donnees, s1);
            Temps b = tester<TasMaxVector>(donnees, s2);
            Temps c = tester<TasMaxPQ>(donnees, s3);

            ins[0].push_back(a.insertion); ext[0].push_back(a.extraction);
            ins[1].push_back(b.insertion); ext[1].push_back(b.extraction);
            ins[2].push_back(c.insertion); ext[2].push_back(c.extraction);

            // Verification : memes valeurs, ordre decroissant
            if (s1 != s2 || s2 != s3 || !is_sorted(s1.rbegin(), s1.rend()))
                toutOk = false;
        }
        for (int m = 0; m < 3; ++m) {
            resultats[0][t][m] = mediane(ins[m]);
            resultats[1][t][m] = mediane(ext[m]);
        }
    }

    for (int op = 0; op < 2; ++op) {
        cout << "\n=== " << titres[op] << " ===\n";
        cout << left << setw(10) << "N"
             << right << setw(18) << "Tableau brut"
             << setw(18) << "Vector manuel"
             << setw(18) << "priority_queue" << "\n";
        cout << string(64, '-') << "\n";
        for (size_t t = 0; t < tailles.size(); ++t) {
            cout << left << setw(10) << tailles[t] << right << fixed << setprecision(3);
            for (int m = 0; m < 3; ++m) cout << setw(18) << resultats[op][t][m];
            cout << "\n";
        }
    }

    cout << "\nResultats identiques et tries : " << (toutOk ? "OUI" : "NON") << "\n";
    cout << "Complexite : insertion O(log n), extraction O(log n) pour les 3 methodes.\n";
    return toutOk ? 0 : 1;
}