"""
RIDUZIONE POLINOMIALE: 3-SAT <=_p HexMapGeneration

Input: Formula phi con n variabili e m clausole in forma 3-CNF
Output: Configurazione di mappa hex (o certificato di impossibilita')

Mapping:
  - Ogni variabile x_i -> tile hex con coordinate spiral_hex_coords(i)
  - x_i = True  -> bioma in {forest, mountain, swamp}  (BIOME_TRUE)
  - x_i = False -> bioma in {desert, water, city}       (BIOME_FALSE)
  - Ogni clausola C_j -> tile di transizione adiacente ai 3 hex della clausola
  - Transizione valida <=> almeno un letterale della clausola e' soddisfatto

Complessita' riduzione: O(n + m) - polinomiale
Correttezza:
  - phi soddisfacibile => esiste mappa valida (Z3 produce il certificato)
  - Mappa valida => esiste assegnazione che soddisfa phi (leggibile dalla mappa)

NP-hardness: per riduzione da 3-SAT (NP-completo)
NP membership: dato un assignment, verifica in O(n*m) se la mappa e' valida
Quindi: HexMapGeneration e' NP-completo
"""

import z3
import random


# ========== BIOMI PER MAPPING SAT ==========
BIOME_TRUE  = ['forest', 'mountain', 'swamp']   # variabile = True
BIOME_FALSE = ['desert', 'water', 'city']        # variabile = False

# Direzioni hex standard (pointy-top, axial) - coerenti con HexGrid.DIRECTIONS
HEX_DIRECTIONS = [
    (1, 0),   # E
    (0, 1),   # NE
    (-1, 1),  # NW
    (-1, 0),  # W
    (0, -1),  # SW
    (1, -1),  # SE
]


# ========== FORMULA 3-SAT ==========

class Formula3SAT:
    """Rappresenta una formula in forma normale congiuntiva (3-CNF)"""

    def __init__(self, n_vars, clauses):
        """
        Args:
            n_vars: numero di variabili (indicizzate da 0 a n_vars-1)
            clauses: lista di clausole, ognuna e' lista di tuple (var_idx, is_negated)
                     es: [(0, False), (1, True), (3, False)]  <->  x1 v !x2 v x4
        """
        self.n_vars = n_vars
        self.clauses = clauses

    @staticmethod
    def from_string(text):
        """Parsa formato DIMACS CNF.
        
        Formato atteso:
            p cnf <n_vars> <n_clauses>
            <lit> <lit> <lit> 0
            ...
        Gli indici partono da 1 (DIMACS), valori negativi = negazione.
        """
        n_vars = 0
        clauses = []

        for line in text.strip().splitlines():
            line = line.strip()
            if not line or line.startswith('c'):
                continue
            if line.startswith('p cnf'):
                parts = line.split()
                n_vars = int(parts[2])
                continue
            literals = [int(t) for t in line.split() if t != '0']
            if not literals:
                continue
            clause = []
            for lit in literals:
                var_idx = abs(lit) - 1  # converti a 0-indexed
                is_negated = lit < 0
                clause.append((var_idx, is_negated))
            clauses.append(clause)

        return Formula3SAT(n_vars, clauses)

    def to_string(self):
        """Serializza in formato DIMACS CNF."""
        lines = [f"p cnf {self.n_vars} {len(self.clauses)}"]
        for clause in self.clauses:
            lits = []
            for var_idx, is_negated in clause:
                lit = var_idx + 1
                if is_negated:
                    lit = -lit
                lits.append(str(lit))
            lines.append(' '.join(lits) + ' 0')
        return '\n'.join(lines)


# ========== COORDINATE A SPIRALE ==========

def spiral_hex_coords(n):
    """Genera n coordinate (q, r) in spirale esagonale dal centro (0, 0)."""
    coords = [(0, 0)]
    if n <= 1:
        return coords[:n]

    q, r = 0, 0
    radius = 1

    while len(coords) < n:
        # Sposta al primo hex del nuovo anello (SW)
        dq, dr = HEX_DIRECTIONS[4]
        q += dq
        r += dr

        for direction in range(6):
            dq, dr = HEX_DIRECTIONS[direction]
            for _ in range(radius):
                if len(coords) >= n:
                    return coords
                coords.append((q, r))
                q += dq
                r += dr

        radius += 1

    return coords[:n]


# ========== GENERATORE DI MAPPA SAT ==========

class SATMapGenerator:
    """Usa Z3 per risolvere la formula e mappa la soluzione su biomi hex."""

    def __init__(self, formula):
        self.formula = formula
        self._last_model = None
        self._last_assignment = None

    def generate(self, grid_size=5):
        """Risolve la formula e produce il biome_map.

        Returns:
            dict {(q, r, 0): biome_string}  oppure None se UNSAT.
        """
        solver = z3.Solver()

        z3_vars = [z3.Bool(f'x{i}') for i in range(self.formula.n_vars)]

        for clause in self.formula.clauses:
            literals = []
            for var_idx, is_negated in clause:
                v = z3_vars[var_idx]
                literals.append(z3.Not(v) if is_negated else v)
            solver.add(z3.Or(*literals))

        result = solver.check()

        if result == z3.unsat:
            print("[SAT] Formula INSODDISFACIBILE - mappa impossibile")
            return None

        if result == z3.unknown:
            print("[SAT] Z3 non riesce a determinare la soddisfacibilita'")
            return None

        model = solver.model()
        self._last_model = model

        assignment = {}
        for i, v in enumerate(z3_vars):
            val = model[v]
            if val is None:
                assignment[i] = random.choice([True, False])
            else:
                assignment[i] = z3.is_true(val)
        self._last_assignment = assignment

        return self._assignment_to_biome_map(assignment)

    def _assignment_to_biome_map(self, assignment):
        """Converte l'assegnazione booleana in {(q,r,0): biome}."""
        n = self.formula.n_vars
        var_coords = spiral_hex_coords(n)
        biome_map = {}

        # Variabili -> biomi
        for i, (q, r) in enumerate(var_coords):
            val = assignment.get(i, True)
            biome_list = BIOME_TRUE if val else BIOME_FALSE
            biome = biome_list[i % len(biome_list)]
            biome_map[(q, r, 0)] = biome

        # Clausole -> tile di transizione
        var_coord_dict = {i: (q, r) for i, (q, r) in enumerate(var_coords)}

        for j, clause in enumerate(self.formula.clauses):
            positions = []
            for var_idx, _ in clause:
                if var_idx in var_coord_dict:
                    positions.append(var_coord_dict[var_idx])

            if not positions:
                continue

            avg_q = round(sum(p[0] for p in positions) / len(positions))
            avg_r = round(sum(p[1] for p in positions) / len(positions))

            candidate = (avg_q, avg_r, 0)
            if candidate in biome_map:
                for dq, dr in HEX_DIRECTIONS:
                    alt = (avg_q + dq, avg_r + dr, 0)
                    if alt not in biome_map:
                        candidate = alt
                        break

            clause_satisfied = any(
                (assignment.get(vi, True) != negated)
                for vi, negated in clause
            )
            transition_biome = 'forest' if clause_satisfied else 'desert'
            biome_map[candidate] = transition_biome

        return biome_map

    def print_report(self):
        """Stampa il report della formula e del mapping variabile->bioma->hex."""
        f = self.formula
        print("\n" + "="*50)
        print(f"[SAT REPORT]  {f.n_vars} variabili, {len(f.clauses)} clausole")
        print("="*50)

        if self._last_assignment is None:
            print("Nessuna soluzione disponibile (UNSAT o non risolto)")
            return

        var_coords = spiral_hex_coords(f.n_vars)
        print(f"\n{'Variabile':<12} {'Valore':<8} {'Bioma':<12} Coord hex")
        print('-' * 50)
        for i, (q, r) in enumerate(var_coords):
            val = self._last_assignment.get(i, True)
            biome_list = BIOME_TRUE if val else BIOME_FALSE
            biome = biome_list[i % len(biome_list)]
            print(f"  x{i+1:<10} {'True' if val else 'False':<8} {biome:<12} ({q},{r},0)")

        print(f"\nClausole: {len(f.clauses)}")
        for j, clause in enumerate(f.clauses):
            parts = []
            for vi, neg in clause:
                prefix = 'NOT ' if neg else ''
                parts.append(f"{prefix}x{vi+1}")
            satisfied = any(
                (self._last_assignment.get(vi, True) != neg)
                for vi, neg in clause
            )
            status = '[OK]' if satisfied else '[FAIL]'
            print(f"  C{j+1}: {' OR '.join(parts)}  {status}")

        print(f"\nFormula: SODDISFACIBILE")
        print("="*50 + "\n")


# ========== I/O FORMULA ==========

def load_formula_from_file(path):
    """Legge un file .cnf in formato DIMACS e ritorna una Formula3SAT."""
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()
    return Formula3SAT.from_string(text)


def demo_formula():
    """Formula hardcoded di esempio (4 variabili, 6 clausole) per test rapidi."""
    clauses = [
        [(0, False), (1, True),  (2, False)],
        [(0, True),  (1, False), (3, False)],
        [(1, False), (2, True),  (3, False)],
        [(0, True),  (2, True),  (3, True)],
        [(0, False), (2, False), (3, True)],
        [(1, True),  (2, False), (3, False)],
    ]
    return Formula3SAT(n_vars=4, clauses=clauses)
