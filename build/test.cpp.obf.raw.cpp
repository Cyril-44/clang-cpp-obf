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
const int mvTjuRY = 9005;

struct JLo1rj6jaQff {
    int GK2SS9yNbacv, J7_uSIw4, HkLSsfyjgg;
};

int ItdY69[mvTjuRY];
bitset<mvTjuRY> GaFGKrVCz8M[mvTjuRY];
bitset<mvTjuRY> n97xL9x[mvTjuRY];
int u4Sa0jaChWf7;
map<int, int> ottf9NVWZ3DI;

// DSU Find
int wS3Ohtb_4c(int j_1G74) {
    if (j_1G74 == ItdY69[j_1G74]) return j_1G74;
    return ItdY69[j_1G74] = wS3Ohtb_4c(ItdY69[j_1G74]);
}

// Get compressed ID
int Qu0LOd9oyiO(int FdRELob4Qj) {
    if (ottf9NVWZ3DI.find(FdRELob4Qj) == ottf9NVWZ3DI.end()) {
        ottf9NVWZ3DI[FdRELob4Qj] = u4Sa0jaChWf7++;
    }
    return ottf9NVWZ3DI[FdRELob4Qj];
}

// Merge set b into set a
void tov5LLHY(int xOO7OW, int NYczkzY) {
    xOO7OW = wS3Ohtb_4c(xOO7OW);
    NYczkzY = wS3Ohtb_4c(NYczkzY);
    if (xOO7OW != NYczkzY) {
        // Merge Row Constraints (What constraints apply to this group)
        GaFGKrVCz8M[xOO7OW] |= GaFGKrVCz8M[NYczkzY];
        n97xL9x[xOO7OW] |= n97xL9x[NYczkzY];
        
        // Merge Column Constraints (Update references in other groups)
        // Since 'b' is now 'a', any group containing 'b' now contains 'a'.
        // This effectively renames bit 'b' to 'a' in all bitsets.
        for (int oAbjCGu95aY = 0; oAbjCGu95aY < u4Sa0jaChWf7; ++oAbjCGu95aY) {
            // Only iterate active roots to save time, though strictly i < id_counter covers all.
            // Using parent[i]==i check might be an optimization, but loop is fast enough for N=3000 sum.
            if (ItdY69[oAbjCGu95aY] == oAbjCGu95aY) {
                if (GaFGKrVCz8M[oAbjCGu95aY].test(NYczkzY)) {
                    GaFGKrVCz8M[oAbjCGu95aY].reset(NYczkzY);
                    GaFGKrVCz8M[oAbjCGu95aY].set(xOO7OW);
                }
                if (n97xL9x[oAbjCGu95aY].test(NYczkzY)) {
                    n97xL9x[oAbjCGu95aY].reset(NYczkzY);
                    n97xL9x[oAbjCGu95aY].set(xOO7OW);
                }
            }
        }
        
        ItdY69[NYczkzY] = xOO7OW;
    }
}

void OamaSCgJO() {
    int SndMo5;
    if (!(cin >> SndMo5)) return;

    // Reset for new test case
    ottf9NVWZ3DI.clear();
    u4Sa0jaChWf7 = 0;
    
    struct uXCKPMSE1fD {
        int iGabMuiFk; // 0: rec, 1: not-rec, 2: inter
        int r9ZAR7TpQC, qcEvyg, f9bOqozrE;
    };
    vector<uXCKPMSE1fD> qMuRm2pMj4U;
    qMuRm2pMj4U.reserve(SndMo5);

    for (int r6MD9LEli = 0; r6MD9LEli < SndMo5; ++r6MD9LEli) {
        string Z6yNyL38zDsB;
        cin >> Z6yNyL38zDsB;
        if (Z6yNyL38zDsB == "recommend") {
            int yNA23lmQ7, ZbuENH33qs; cin >> yNA23lmQ7 >> ZbuENH33qs;
            qMuRm2pMj4U.push_back({0, yNA23lmQ7, ZbuENH33qs, 0});
        } else if (Z6yNyL38zDsB == "not-recommend") {
            int xMtWPcb5vd, vngsoj; cin >> xMtWPcb5vd >> vngsoj;
            qMuRm2pMj4U.push_back({1, xMtWPcb5vd, vngsoj, 0});
        } else {
            int tu470PSjHIsq, eCiT3js6r, uy9FAWA; cin >> tu470PSjHIsq >> eCiT3js6r >> uy9FAWA;
            qMuRm2pMj4U.push_back({2, tu470PSjHIsq, eCiT3js6r, uy9FAWA});
        }
    }

    // Assign internal IDs
    vector<JLo1rj6jaQff> TYPSsW2W;
    for (auto& qbBLhH6VIS1F : qMuRm2pMj4U) {
        qbBLhH6VIS1F.r9ZAR7TpQC = Qu0LOd9oyiO(qbBLhH6VIS1F.r9ZAR7TpQC);
        qbBLhH6VIS1F.qcEvyg = Qu0LOd9oyiO(qbBLhH6VIS1F.qcEvyg);
        if (qbBLhH6VIS1F.iGabMuiFk == 2) {
            qbBLhH6VIS1F.f9bOqozrE = Qu0LOd9oyiO(qbBLhH6VIS1F.f9bOqozrE);
            TYPSsW2W.push_back({qbBLhH6VIS1F.r9ZAR7TpQC, qbBLhH6VIS1F.qcEvyg, qbBLhH6VIS1F.f9bOqozrE});
        }
    }

    // Initialize Structures
    for (int bjUx7KNezJm = 0; bjUx7KNezJm < u4Sa0jaChWf7; ++bjUx7KNezJm) {
        ItdY69[bjUx7KNezJm] = bjUx7KNezJm;
        GaFGKrVCz8M[bjUx7KNezJm].reset();
        n97xL9x[bjUx7KNezJm].reset();
    }

    // Apply Initial Constraints
    // Note: We use find_set(x) immediately to handle any pre-existing logic if we were dynamic,
    // but here initially everyone is their own root.
    for (auto& Q_DrwDMZJ7ZM : qMuRm2pMj4U) {
        int Bl7RFjt = Q_DrwDMZJ7ZM.r9ZAR7TpQC; 
        int l_UFcd7SD = Q_DrwDMZJ7ZM.qcEvyg;
        if (Q_DrwDMZJ7ZM.iGabMuiFk == 0) { // recommend u v -> u in S_v
            GaFGKrVCz8M[l_UFcd7SD].set(Bl7RFjt);
        } else if (Q_DrwDMZJ7ZM.iGabMuiFk == 1) { // not-recommend u v -> u not in S_v
            n97xL9x[l_UFcd7SD].set(Bl7RFjt);
        }
    }

    bool nI21ICsu = true;
    while (nI21ICsu) {
        nI21ICsu = false;

        // 1. Detect aliases via duplicate intersection definitions
        // If S_u = S_v \cap S_w and S_x = S_v \cap S_w, then u == x.
        map<pair<int, int>, int> PwtOMYO2fmM3;
        
        for (auto& rL9KVltN : TYPSsW2W) {
            int Gpfpd6h1 = wS3Ohtb_4c(rL9KVltN.GK2SS9yNbacv);
            int MPp0Xvka = wS3Ohtb_4c(rL9KVltN.J7_uSIw4);
            int LYP6sjy0MC = wS3Ohtb_4c(rL9KVltN.HkLSsfyjgg);

            // Normalize order for key
            if (MPp0Xvka > LYP6sjy0MC) swap(MPp0Xvka, LYP6sjy0MC);

            // Special Case: S_u = S_v \cap S_v => S_u = S_v => u == v
            if (MPp0Xvka == LYP6sjy0MC) {
                if (Gpfpd6h1 != MPp0Xvka) {
                    tov5LLHY(Gpfpd6h1, MPp0Xvka);
                    nI21ICsu = true;
                }
            } else {
                if (PwtOMYO2fmM3.count({MPp0Xvka, LYP6sjy0MC})) {
                    int VYk2wK = PwtOMYO2fmM3[{MPp0Xvka, LYP6sjy0MC}];
                    if (VYk2wK != Gpfpd6h1) {
                        tov5LLHY(VYk2wK, Gpfpd6h1);
                        nI21ICsu = true;
                    }
                } else {
                    PwtOMYO2fmM3[{MPp0Xvka, LYP6sjy0MC}] = Gpfpd6h1;
                }
            }
        }
        
        // 2. Propagate Intersection Logic
        // Re-iterate because unions might have changed roots
        for (auto& ocXB71wI : TYPSsW2W) {
            int DelToxnTT2H = wS3Ohtb_4c(ocXB71wI.GK2SS9yNbacv);
            int PK36CwB = wS3Ohtb_4c(ocXB71wI.J7_uSIw4);
            int U5L5gC = wS3Ohtb_4c(ocXB71wI.HkLSsfyjgg);

            // S_u = S_v \cap S_w
            
            // Logic: x in Sv AND x in Sw => x in Su
            bitset<mvTjuRY> xgOEb8MIhs = GaFGKrVCz8M[PK36CwB] & GaFGKrVCz8M[U5L5gC];
            if ((GaFGKrVCz8M[DelToxnTT2H] | xgOEb8MIhs) != GaFGKrVCz8M[DelToxnTT2H]) {
                GaFGKrVCz8M[DelToxnTT2H] |= xgOEb8MIhs;
                nI21ICsu = true;
            }

            // Logic: x in Su => x in Sv AND x in Sw
            if ((GaFGKrVCz8M[PK36CwB] | GaFGKrVCz8M[DelToxnTT2H]) != GaFGKrVCz8M[PK36CwB]) {
                GaFGKrVCz8M[PK36CwB] |= GaFGKrVCz8M[DelToxnTT2H];
                nI21ICsu = true;
            }
            if ((GaFGKrVCz8M[U5L5gC] | GaFGKrVCz8M[DelToxnTT2H]) != GaFGKrVCz8M[U5L5gC]) {
                GaFGKrVCz8M[U5L5gC] |= GaFGKrVCz8M[DelToxnTT2H];
                nI21ICsu = true;
            }

            // Logic: x not in Sv => x not in Su (since Su subset Sv)
            if ((n97xL9x[DelToxnTT2H] | n97xL9x[PK36CwB]) != n97xL9x[DelToxnTT2H]) {
                n97xL9x[DelToxnTT2H] |= n97xL9x[PK36CwB];
                nI21ICsu = true;
            }
            if ((n97xL9x[DelToxnTT2H] | n97xL9x[U5L5gC]) != n97xL9x[DelToxnTT2H]) {
                n97xL9x[DelToxnTT2H] |= n97xL9x[U5L5gC];
                nI21ICsu = true;
            }

            // Logic: x not in Su AND x in Sv => x not in Sw
            bitset<mvTjuRY> faszjP = n97xL9x[DelToxnTT2H] & GaFGKrVCz8M[PK36CwB];
            if ((n97xL9x[U5L5gC] | faszjP) != n97xL9x[U5L5gC]) {
                n97xL9x[U5L5gC] |= faszjP;
                nI21ICsu = true;
            }
            
            // Logic: x not in Su AND x in Sw => x not in Sv
            bitset<mvTjuRY> qXrvgwC2 = n97xL9x[DelToxnTT2H] & GaFGKrVCz8M[U5L5gC];
            if ((n97xL9x[PK36CwB] | qXrvgwC2) != n97xL9x[PK36CwB]) {
                n97xL9x[PK36CwB] |= qXrvgwC2;
                nI21ICsu = true;
            }
        }
    }

    // Final Consistency Checks
    int M72zavlWo1m = 0;
    for (int P4h3KHtJmR3 = 0; P4h3KHtJmR3 < u4Sa0jaChWf7; ++P4h3KHtJmR3) {
        if (ItdY69[P4h3KHtJmR3] == P4h3KHtJmR3) {
            M72zavlWo1m++;
            // 1. Contradiction check
            if ((GaFGKrVCz8M[P4h3KHtJmR3] & n97xL9x[P4h3KHtJmR3]).any()) {
                cout << "no" << endl;
                return;
            }
            // 2. Self-loop check (x recommends x)
            if (GaFGKrVCz8M[P4h3KHtJmR3].test(P4h3KHtJmR3)) {
                cout << "no" << endl;
                return;
            }
        }
    }

    // 3. Cycle Detection (Topological Sort)
    // Build graph where edge U -> V exists if group V contains member U
    // Due to union_sets logic, bits in bitsets correspond to current roots.
    vector<vector<int>> wLzny6xv_V(u4Sa0jaChWf7);
    vector<int> RP_2fT9_(u4Sa0jaChWf7, 0);

    for (int c_UUHtlJ8 = 0; c_UUHtlJ8 < u4Sa0jaChWf7; ++c_UUHtlJ8) {
        if (ItdY69[c_UUHtlJ8] != c_UUHtlJ8) continue;
        
        // For every active root u, if u is in must_have[v], add u -> v
        // We can iterate all k set in must_have[v]. 
        // Since we migrated columns, only active roots should be set.
        for (int yiN1FSuVU_ = GaFGKrVCz8M[c_UUHtlJ8]._Find_first(); yiN1FSuVU_ < mvTjuRY; yiN1FSuVU_ = GaFGKrVCz8M[c_UUHtlJ8]._Find_next(yiN1FSuVU_)) {
             // u is a recommender of v. So u happened before v. Edge u -> v.
             // Double check u is a root (it should be if logic is correct).
             // If u is not a root, find_set(u) is the root.
             int Rye9fDq = wS3Ohtb_4c(yiN1FSuVU_);
             if (Rye9fDq != yiN1FSuVU_) {
                 // This theoretically shouldn't happen with full migration, 
                 // but safe to handle.
             }
             wLzny6xv_V[Rye9fDq].push_back(c_UUHtlJ8);
             RP_2fT9_[c_UUHtlJ8]++;
        }
    }

    // Kahn's Algorithm
    vector<int> j_Ds45Odtk;
    for (int aOFVu6oA = 0; aOFVu6oA < u4Sa0jaChWf7; ++aOFVu6oA) {
        if (ItdY69[aOFVu6oA] == aOFVu6oA && RP_2fT9_[aOFVu6oA] == 0) {
            j_Ds45Odtk.push_back(aOFVu6oA);
        }
    }

    int Vnbmv_TfDyFA = 0;
    while (!j_Ds45Odtk.empty()) {
        int b3YObQy = j_Ds45Odtk.back(); j_Ds45Odtk.pop_back();
        Vnbmv_TfDyFA++;
        for (int FGzHa2Z : wLzny6xv_V[b3YObQy]) {
            RP_2fT9_[FGzHa2Z]--;
            if (RP_2fT9_[FGzHa2Z] == 0) j_Ds45Odtk.push_back(FGzHa2Z);
        }
    }

    if (Vnbmv_TfDyFA < M72zavlWo1m) {
        cout << "no" << endl;
    } else {
        cout << "yes" << endl;
    }
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);
    int MQ4Rfozw;
    if (cin >> MQ4Rfozw) {
        while (MQ4Rfozw--) {
            OamaSCgJO();
        }
    }
    return 0;
}