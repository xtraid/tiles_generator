# FORMALIZZAZIONE CORRETTA: GENERAZIONE PROCEDURALE INCREMENTALE
## Clausole di Horn e Sottoproblemi Locali

---

## PREMESSA: DUE PROBLEMI DIVERSI

### Problema 1: WORLD-VERIFICATION (Decisione Globale)
```
Input: Mondo completo (β, η, π) per TUTTA la griglia
Output: È valido? (SAT/UNSAT)

Questo è NP-completo (dimostrazione precedente)
```

### Problema 2: WORLD-GENERATION (Costruzione Incrementale)
```
Input: Tile corrente t, contesto vicini già generati
Output: Assegnamento valido (bioma_t, elevazione_t, props_t)

Questo è il problema REALE che dobbiamo formalizzare!
```

**CHIAVE:** Generazione != Verifica

Nella generazione:
- Processiamo tile per tile (ordine topologico/BFS)
- Quando generiamo tile t, i vicini sono GIÀ FISSATI
- Formule diventano MOLTO più semplici (Horn clauses)
- Molte soluzioni possibili → varietà procedurale

---

## PARTE 1: GENERAZIONE BIOMA CON HORN CLAUSES

### 1.1 Contesto Generazione Tile

```
Quando generiamo tile t = (q, r):

Input fisso (contesto):
    neighbors_t = {t₁, t₂, ..., tₖ}  (vicini già generati)
    ∀tᵢ ∈ neighbors_t: β[tᵢ] è FISSATO (già scelto)

Output da decidere:
    β[t] ∈ B  (quale bioma scegliere per t?)
```

### 1.2 Formula Horn per Bioma

**Variabili proposizionali (SOLO per tile corrente):**
```
∀b ∈ B:
    choose_b = "scegli bioma b per tile t"

|variabili| = |B| = 6  (NON |V|·|B|!)
```

**Clausole di Horn:**

**H1. At-least-one bioma:**
```
choose_forest ∨ choose_desert ∨ choose_mountain ∨ choose_water ∨ choose_city ∨ choose_swamp
```
Questa è una clausola Horn (tutti letterali positivi).

**H2. At-most-one bioma (pairwise):**
```
∀bᵢ, bⱼ ∈ B con i ≠ j:
    choose_bᵢ → ¬choose_bⱼ

In Horn: ¬choose_bᵢ ∨ ¬choose_bⱼ
```
Numero clausole: (6 choose 2) = 15 clausole

**H3. Compatibilità con vicini (VINCOLO CHIAVE):**
```
Per ogni vicino tᵢ già generato con β[tᵢ] = bᵢ:
    ∀b ∈ B:
        Se M[bᵢ, b] = 0 (transizione proibita):
            choose_b → false
            
        In Horn: ¬choose_b

Esempio:
    Se t ha vicino con bioma water
    E M[water, city] = 0 (incompatibili)
    
    Clausola: ¬choose_city
```

Numero clausole: O(|neighbors| · |B|) = O(6 · 6) = O(1) clausole costanti

**Formula completa Φ_bioma:**
```
Φ_bioma = H1 ∧ H2 ∧ H3

Variabili: 6
Clausole: 1 + 15 + O(36) ≈ 52 clausole TOTALI (non per tile, TOTALI!)
```

### 1.3 Risoluzione Horn Clauses

**Teorema (Dowling-Gallier 1984):**
```
Le clausole di Horn sono risolvibili in tempo O(n + m) dove:
    n = numero variabili
    m = numero clausole

Per Φ_bioma:
    n = 6
    m ≈ 52
    
    Tempo risoluzione: O(6 + 52) = O(1) costante!
```

**Algoritmo Unit Propagation:**
```
unit_propagation(Φ_bioma):
    1. Inizia con contesto:
        Se neighbor ha bioma water e M[water, city] = 0:
            Fixa choose_city = false
    
    2. Propaga:
        Ogni clausola con tutti letterali false tranne 1
        → quel letterale deve essere true
    
    3. Ripeti finché possibile
    
    4. Se rimangono scelte libere:
        Scegli random tra opzioni valide
        (questo crea VARIETÀ!)

Complessità: O(m) = O(52) = O(1)
```

### 1.4 Esempio Concreto

```
Tile t ha 3 vicini già generati:
    neighbor₁: β = forest
    neighbor₂: β = forest  
    neighbor₃: β = water

Matrice transizioni:
    M[forest, desert] = 1  ✓
    M[forest, mountain] = 1  ✓
    M[forest, water] = 1  ✓
    M[water, city] = 0  ✗
    M[water, desert] = 0  ✗

Formula:
    H1: (choose_forest ∨ choose_desert ∨ ...)
    H2: at-most-one (15 clausole)
    H3: 
        ¬choose_city  (incompatibile con water)
        ¬choose_desert  (incompatibile con water)

Unit propagation:
    choose_city = false
    choose_desert = false
    
Biomi validi: {forest, mountain, water, swamp}

Scelta finale: RANDOM tra questi 4 → VARIETÀ procedurale
```

---

## PARTE 2: GENERAZIONE PROPS CON FORMULA LOCALE

### 2.1 Contesto Generazione Props per Sezione

```
Quando generiamo props per sezione s di tile t:

Input fisso (contesto):
    β[t] = bioma tile corrente (già scelto in Parte 1)
    
    Per wedge s = wedge_i:
        adjacent_sections = {
            wedge_opposite in neighbor tile (se esiste),
            wedge_{i-1} in same tile,
            wedge_{i+1} in same tile,
            center in same tile
        }
        
        ∀s' ∈ adjacent_sections:
            π[s'] è FISSATO (già scelto) o ∅

Output da decidere:
    π[s] ∈ P ∪ {∅}  (quale prop scegliere, se any?)
```

### 2.2 Variabili Formula Props

**Variabili proposizionali (SOLO per sezione corrente):**
```
∀p ∈ P:
    choose_p = "scegli prop p per sezione s"

choose_empty = "lascia sezione vuota"

|variabili| = |P| + 1 = 168 + 1 = 169
```

### 2.3 Clausole Props

**P1. At-most-one prop:**
```
∀pᵢ, pⱼ ∈ P ∪ {∅} con i ≠ j:
    ¬choose_pᵢ ∨ ¬choose_pⱼ

Clausole: (169 choose 2) = 14,196 clausole
```

**P2. Section type compatibility:**
```
∀p ∈ P:
    Se section_type(p) ≠ section_type(s):
        ¬choose_p

Esempio: se s = center, tutti props wedge sono esclusi
    ¬choose_tree_wedge_0
    ¬choose_rock_wedge_1
    ...

Clausole: ~84 (metà props incompatibili)
```

**P3. PURITY constraint:**
```
∀p ∈ P con category(p) = PURE:
    Se β[t] ∉ biome_compat(p):
        ¬choose_p

Esempio: tile ha bioma desert, prop tree_forest_PURE
    forest ∉ {desert}
    → ¬choose_tree_forest_PURE

Clausole: ~42 (metà props PURE, metà incompatibili con bioma)
```

**P4. TRANSITION constraint:**
```
∀p ∈ P con category(p) = TRANSITION:
    s' = adjacent_wedge(s)  (wedge opposto in tile vicino)
    
    Se s' non esiste (bordo mappa):
        ¬choose_p  (transition props solo su bordi interni)
    
    Altrimenti:
        b_neighbor = β[tile_of(s')]
        
        Se T(p, β[t], b_neighbor) = 0:
            ¬choose_p

Esempio: tile corrente = forest, neighbor = water
         prop = tree_to_sand_TRANSITION
         T(tree_to_sand, forest, water) = 0 (valido solo forest→desert)
         → ¬choose_tree_to_sand_TRANSITION

Clausole: ~40 (la maggior parte transition props esclusi per coppia biomi)
```

**P5. Neighbor coherence:**
```
Per ogni sezione adiacente s' con π[s'] = p' (già fissato):
    ∀p ∈ P:
        Se C[p', p] = 0 (incompatibili):
            ¬choose_p

Esempio: wedge adiacente ha tree_tall
         C[tree_tall, rock_giant] = 0
         → ¬choose_rock_giant

Clausole: O(|adjacent_sections| · |P|) = O(4 · 168) ≈ 672 clausole
```

**Formula completa Φ_prop:**
```
Φ_prop = P1 ∧ P2 ∧ P3 ∧ P4 ∧ P5

Variabili: 169
Clausole: 14,196 + 84 + 42 + 40 + 672 ≈ 15,034 clausole
```

### 2.4 Risoluzione Formula Props

**IMPORTANTE:** Queste NON sono tutte Horn clauses (P1 ha negazioni multiple).

Ma sono **molto più semplici** del problema globale perché:

1. **Molte variabili eliminate:** Solo 169 invece di 7·|V|·|P| = 470,400
2. **Unit propagation efficace:** P2-P5 sono tutte clausole unitarie negative
3. **Molte soluzioni:** Dopo propagazione, rimangono 5-20 props validi

**Algoritmo:**
```
solve_prop_selection(s, context):
    // Unit propagation (P2-P5)
    valid_props = P  // Inizia con tutti
    
    For each prop p:
        // Elimina incompatibili
        If section_type(p) != section_type(s):
            valid_props.remove(p)
        
        If category(p) == PURE and β[t] not in biome_compat(p):
            valid_props.remove(p)
        
        If category(p) == TRANSITION:
            If not valid_transition(p, β[t], β[neighbor]):
                valid_props.remove(p)
        
        For each adjacent s' with prop p':
            If C[p', p] == 0:
                valid_props.remove(p)
    
    // P1: at-most-one (scelta finale)
    If len(valid_props) == 0:
        return ∅  // Lascia vuoto
    Else:
        return random_choice(valid_props ∪ {∅})  // Varietà!

Complessità: O(|P| · |adjacent|) = O(168 · 4) = O(1) costante!
```

---

## PARTE 3: GENERAZIONE ELEVAZIONE

### 3.1 Opzione A: Procedurale (Smoothing)

```
generate_elevation(t, neighbors):
    // Smoothing: media elevazioni vicini + noise
    
    neighbor_elevations = [η[tᵢ] for tᵢ in neighbors if tᵢ exists]
    
    If len(neighbor_elevations) == 0:
        // Primo tile (seed)
        base_elevation = random_choice(Z_valid[β[t]])
    Else:
        avg = mean(neighbor_elevations)
        noise = random_uniform(-0.5, 0.5)
        base_elevation = round(avg + noise)
    
    // Clamp a Z_valid per bioma
    valid_range = Z_valid[β[t]]
    η[t] = clamp(base_elevation, min(valid_range), max(valid_range))
    
    // Verifica smoothness con ogni vicino
    For each neighbor tᵢ:
        If |η[t] - η[tᵢ]| > Δz:
            // Aggiusta per rispettare smoothness
            If η[tᵢ] > η[t]:
                η[t] = max(η[tᵢ] - Δz, min(valid_range))
            Else:
                η[t] = min(η[tᵢ] + Δz, max(valid_range))

Complessità: O(|neighbors|) = O(6) = O(1)
```

### 3.2 Opzione B: Horn Clauses (se vogliamo solver)

```
Variabili:
    ∀z ∈ Z: choose_z = "scegli elevazione z"
    
    |variabili| = |Z| = 6

Clausole:
    H1: choose_0 ∨ choose_1 ∨ ... ∨ choose_5  (at-least-one)
    
    H2: ∀i≠j: ¬choose_i ∨ ¬choose_j  (at-most-one)
    
    H3: ∀z ∉ Z_valid[β[t]]: ¬choose_z  (bioma-specific)
    
    H4: Per ogni neighbor tᵢ con η[tᵢ] = zᵢ:
            ∀z ∈ Z con |z - zᵢ| > Δz:
                ¬choose_z  (smoothness)

Formula Φ_elevation:
    Variabili: 6
    Clausole: 1 + 15 + ~3 + ~18 = 37 clausole
    
Unit propagation: O(37) = O(1)
```

**Raccomandazione:** Opzione A (procedurale) è più efficiente e naturale per elevazioni.

---

## PARTE 4: PIPELINE GENERAZIONE COMPLETA

### 4.1 Algoritmo Generale

```
generate_world(W, H):
    tiles = {}
    
    // Ordine generazione: BFS da seed point
    queue = [(W/2, H/2)]  // Centro mappa
    visited = set()
    
    While queue not empty:
        t = queue.pop()
        If t in visited: continue
        visited.add(t)
        
        // === STEP 1: BIOMA ===
        neighbors = get_generated_neighbors(t, tiles)
        
        Φ_bioma = build_biome_formula(neighbors)
        β[t] = solve_horn(Φ_bioma)  // O(1)
        
        // === STEP 2: ELEVAZIONE ===
        η[t] = generate_elevation_smooth(t, neighbors)  // O(1)
        
        // === STEP 3: PROPS (per ogni sezione) ===
        For each section s in sections_of(t):  // 7 sezioni
            context = {
                'biome': β[t],
                'elevation': η[t],
                'adjacent_props': [π[s'] for s' in adjacent(s)]
            }
            
            Φ_prop = build_prop_formula(s, context)
            π[s] = solve_prop(Φ_prop)  // O(1)
        
        tiles[t] = (β[t], η[t], {s: π[s] for s in sections_of(t)})
        
        // Aggiungi vicini non visitati a queue
        For neighbor in adj(t):
            If neighbor not in visited:
                queue.append(neighbor)
    
    Return tiles

Complessità totale:
    Per tile: O(1) + O(1) + 7·O(1) = O(1)
    Per griglia: |V| · O(1) = O(|V|) = LINEARE! ✓
```

### 4.2 Perché È Polinomiale (anzi, Lineare!)

```
Ogni sottoproblema locale:
    - Φ_bioma: 6 variabili, 52 clausole → O(1)
    - Φ_elevation: procedurale → O(1)
    - Φ_prop: 169 variabili, ~15K clausole → O(1) (unit propagation)

Numero tiles: |V| = W × H

Tempo totale: |V| · O(1) = O(W · H) = LINEARE ✓
```

---

## PARTE 5: VARIETÀ PROCEDURALE

### 5.1 Dove Nasce la Varietà?

```
In OGNI sottoproblema, dopo unit propagation, rimangono MULTIPLE soluzioni valide.

Bioma:
    Dopo H3, rimangono 2-4 biomi compatibili
    → Scelta random tra questi
    → Mondi diversi!

Props:
    Dopo P2-P5, rimangono 5-20 props validi per sezione
    → Scelta random
    → Mondi visivamente diversi!

Elevazione:
    Noise nel smoothing
    → Terreni diversi!
```

### 5.2 Controllo Deterministico (Seed)

```
generate_world(W, H, seed=None):
    If seed is not None:
        random.seed(seed)
    
    // Resto algoritmo uguale
    
Stesso seed → stesso mondo (riproducibilità)
Seed diverso → mondo diverso (varietà)
```

---

## PARTE 6: COMPLESSITÀ NP-COMPLETEZZA vs GENERAZIONE

### 6.1 Riconciliazione

```
┌────────────────────────────────────────────────────┐
│ PROBLEMA DI DECISIONE (Verifica)                   │
│ ───────────────────────────────────                │
│ Input: Mondo COMPLETO (β, η, π) per TUTTA griglia │
│ Output: È valido?                                  │
│                                                     │
│ Complessità: NP-completo                           │
│ (dimostrazione precedente)                         │
└────────────────────────────────────────────────────┘
                            VS
┌────────────────────────────────────────────────────┐
│ PROBLEMA DI GENERAZIONE (Costruzione)             │
│ ──────────────────────────────────                 │
│ Input: Parametri (W, H, seed)                      │
│ Output: Mondo valido                               │
│                                                     │
│ Algoritmo: BFS + Horn clauses locali               │
│ Complessità: O(|V|) = LINEARE                      │
└────────────────────────────────────────────────────┘
```

**Non è contraddizione!**

- Verificare soluzione nota: NP-completo
- Costruire UNA soluzione: Lineare (algoritmo greedy locale)

Analogia: Sudoku
- Verificare soluzione Sudoku: P (banale, O(n²))
- Generare Sudoku valido: P (costruzione incrementale)
- Risolvere Sudoku dato (trovare soluzione): NP-completo

### 6.2 Caratterizzazione Formale

**Teorema:**
```
WORLD-GEN come problema di COSTRUZIONE GREEDY è in P.
WORLD-GEN come problema di SATISFIABILITY GLOBALE è NP-completo.
```

**Dimostrazione (sketch):**
```
Costruzione Greedy:
    - Ogni tile dipende solo da O(1) vicini
    - Formula locale ha dimensione O(1)
    - Horn clauses → O(1) tempo
    - |V| tiles → O(|V|) totale ✓

Satisfiability Globale:
    - Formula congiunta di TUTTI tiles
    - Accoppiamenti globali (connettività, density, ...)
    - Riduzione da 3-SAT possibile
    - NP-completo ✓
```

---

## PARTE 7: FORMULA FINALE CORRETTA

### 7.1 Formula Per Singolo Tile

```
Dato tile t con contesto C_t = (neighbors, loro biomi/elevazioni/props):

Φ_tile(t, C_t) = Φ_bioma(t, C_t) ∧ Φ_elevation(t, C_t) ∧ (∧_{s ∈ sections(t)} Φ_prop(s, C_t))

Dove:
    Φ_bioma: Horn clauses, 6 variabili, 52 clausole
    Φ_elevation: Smooth procedurale O(1) o Horn 6 variabili, 37 clausole
    Φ_prop per sezione: 169 variabili, 15K clausole ciascuna
    
Totale per tile:
    Variabili: 6 + 6 + 7·169 = 1,195 variabili
    Clausole: 52 + 37 + 7·15,034 = 105,327 clausole

Ma CON CONTESTO FISSATO:
    Unit propagation riduce a poche scelte libere
    Risoluzione: O(variabili + clausole) = O(1,195 + 105,327) = O(1) ✓
```

### 7.2 Generazione Sequenziale

```
For each tile t in BFS order:
    Solve Φ_tile(t, C_t)  // O(1)
    Fixa soluzione per t
    Aggiorna contesto per vicini non generati

Tempo: |V| · O(1) = O(|V|) ✓
```

---

## CONCLUSIONE

### Risposta Corretta alla Domanda Originale

**Come siamo arrivati a quei numeri?**
- Prima formalizzazione: SBAGLIATA (problema globale monolitico)
- Formalizzazione corretta: LOCALE (Horn clauses per tile)

**Numeri corretti:**
```
Per singolo tile (con contesto fissato):
    Φ_bioma: 6 var, 52 clausole
    Φ_prop: 7 × (169 var, 15K clausole)
    Totale: ~1,200 var, ~105K clausole
    
    Risoluzione: O(1) con unit propagation Horn

Per griglia completa:
    |V| tiles × O(1) per tile = O(|V|) = LINEARE ✓
```

**Varietà procedurale:**
- Multiple soluzioni dopo propagazione
- Scelta random → mondi diversi
- Seed → riproducibilità

**NP-completezza:**
- Vale per problema di VERIFICA globale
- NON si applica a GENERAZIONE incrementale
- Algoritmo greedy locale è P-time (anzi, lineare!)

La formalizzazione con Horn clauses è MOLTO più corretta e pratica! ✓
