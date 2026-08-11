# Guida tecnica: verificatore, solver seriale e trace delle leaf

Implementation status as of 11 August 2026: this guide has been implemented.
The public headers and tests are authoritative where they differ from an
earlier prospective detail below. The document remains the technical contract
and maintenance handoff for the module.

## 1. Scopo del lavoro

Questa guida e un handoff operativo per implementare il prossimo blocco del
progetto senza dover ricostruire le decisioni architetturali dalla cronologia.

Il lavoro comprende:

1. un verificatore indipendente di tiling completi;
2. un solver seriale deterministico basato su domini a bitmask;
3. propagazione locale, MRV e rollback tramite undo trail;
4. metriche opzionali;
5. uno snapshot renderizzabile restituito sia per `SAT` sia per `UNSAT`;
6. un trace binario opzionale di tutte le leaf fallite, scritto tramite
   `mmap` in un file di capacita massima nota e troncato alla dimensione
   effettiva al termine.

Non implementare in questo blocco:

- parser della formula;
- OpenMP o altre forme di parallelismo;
- memoization, skip list, clause learning o backjumping;
- Z3, JSON o rendering;
- un certificato formale e compatto di insoddisfacibilita;
- modifiche al builder Yang-Zhang o al tileset, salvo correzioni dimostrate da
  test indipendenti.

Il solver riceve esclusivamente un `Region`. Le 23 tessere vengono sempre
lette dal `TILESET` canonico in `src/core/tile.c`; non si passa un tileset come
parametro e non si duplicano manualmente le compatibilita.

## 2. Stato e vincoli del repository

API gia implementate e autorevoli:

- `include/wang/tile.h`: `TileId`, `ColorId`, direzioni, colori, 23 tessere;
- `include/wang/region.h`: geometria densa row-major, active mask e bordi;
- `include/wang/yang_zhang.h`: costruzione formula -> regione;
- `wang_tiles_match()`: matching locale orientato di riferimento.

File implementati da questo blocco:

- `include/wang/verify.h`;
- `src/verify/verify_tiling.c`;
- `include/wang/solver.h`;
- `src/solver/solver_serial.c`;
- `src/solver/failed_leaf_trace.c` and its private header.

Test aggiunti:

- `tests/c/test_verify.c`;
- `tests/c/test_solver.c`;
- `tests/c/test_solver_stress.c`;
- `tests/c/test_solver_yang_zhang.c`.

Il `Makefile` usa wildcard per i test C e include gia verifier e solver nella
libreria seriale. Non aggiungere una build system parallela.

Regole progettuali da preservare:

- `TILESET` e l'unica sorgente di verita per lati e colori;
- il verificatore non usa stato o cache del solver;
- il solver non legge `AdjacentSwap` o metadata dei gadget;
- le tessere possono essere riutilizzate senza limite;
- rotazioni e riflessioni non sono consentite;
- nessuno stato mutabile globale;
- ogni output pubblico e costruito transazionalmente.

## 3. Rappresentazione dei domini

`TILE_COUNT == 23`, quindi un `uint32_t` contiene il dominio completo di una
cella:

```c
#define WANG_DOMAIN_ALL \
    ((UINT32_C(1) << TILE_COUNT) - UINT32_C(1))
```

Convenzioni:

- bit `t` impostato: `TILESET[t]` e ancora ammessa;
- dominio zero su cella attiva: conflitto;
- un solo bit: tessera piazzata/forzata;
- piu bit: cella irrisolta;
- dominio zero su cella inattiva: valore normale, non conflitto.

Non introdurre un array `assignment`: per le celle attive l'assegnazione e
derivabile dai domini singleton.

## 4. API del verificatore

Definire in `include/wang/verify.h`:

```c
#ifndef WANG_VERIFY_H
#define WANG_VERIFY_H

#include <stddef.h>

#include "wang/region.h"
#include "wang/tile.h"

#define TILE_NONE ((TileId)UINT8_MAX)

typedef enum {
    WANG_VERIFY_VALID = 0,
    WANG_VERIFY_INVALID_ARGUMENT,
    WANG_VERIFY_INVALID_REGION,
    WANG_VERIFY_INVALID_LENGTH,
    WANG_VERIFY_INCOMPLETE,
    WANG_VERIFY_INVALID_TILE_ID,
    WANG_VERIFY_INACTIVE_ASSIGNED,
    WANG_VERIFY_BOUNDARY_MISMATCH,
    WANG_VERIFY_ADJACENCY_MISMATCH
} WangVerifyStatus;

WangVerifyStatus wang_verify_tiling(
    const Region *region,
    const TileId *tiles,
    size_t tile_count
);

#endif /* WANG_VERIFY_H */
```

Se un altro modulo ha bisogno di `TILE_NONE`, e accettabile spostare la macro
in `tile.h`; deve comunque esistere una sola definizione.

Il tiling e un array denso parallelo a `Region.cells`:

- lunghezza esatta `width * height`;
- cella attiva: `TileId < TILE_COUNT`;
- cella inattiva: obbligatoriamente `TILE_NONE`.

Il verificatore deve:

1. validare puntatori, dimensioni e prodotto `width * height`;
2. rifiutare colori fuori range;
3. rifiutare vincoli di bordo su celle inattive o su lati che toccano una
   cella attiva;
4. verificare ogni vincolo esposto diverso da `COLOR_NONE`;
5. confrontare direttamente gli edge delle tessere adiacenti;
6. visitare solo `E` e `S` per non controllare due volte ogni adiacenza.

Non chiamare funzioni del solver e non usare `compat`. Il verifier puo leggere
direttamente `TILESET[a].edge[d]` e `TILESET[b].edge[opposite(d)]`.

## 5. API pubblica del solver

Definire in `include/wang/solver.h` una API simile alla seguente. I nomi sono
normativi salvo un motivo concreto emerso durante l'implementazione.

```c
#ifndef WANG_SOLVER_H
#define WANG_SOLVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wang/region.h"

typedef enum {
    WANG_SOLVE_ERROR = -1,
    WANG_SOLVE_UNSAT = 0,
    WANG_SOLVE_SAT = 1
} WangSolveStatus;

enum {
    WANG_SOLVE_COLLECT_METRICS = UINT32_C(1) << 0,
    WANG_SOLVE_TRACE_FAILED_LEAVES = UINT32_C(1) << 1
};

typedef struct {
    uint64_t dfs_nodes;
    uint64_t decisions;
    uint64_t backtracks;
    uint64_t failed_leaves;
    uint64_t domain_reductions;
    uint64_t propagated_arcs;
    uint64_t mrv_cells_scanned;
    size_t trail_peak;
    size_t queue_peak;
    size_t max_depth;
} WangSolverMetrics;

typedef struct {
    uint32_t flags;

    /* Richiesti solo con WANG_SOLVE_TRACE_FAILED_LEAVES. */
    const char *failed_leaf_path;
    size_t failed_leaf_capacity;
} WangSolverOptions;

typedef struct {
    /*
     * Snapshot denso row-major, uno uint32_t per RegionCell.
     * SAT: tutti i domini attivi sono singleton.
     * UNSAT: migliore leaf fallita secondo la regola in Sezione 10.
     */
    uint32_t *domains;
    size_t domain_count;

    /* SIZE_MAX per SAT; indice della cella a dominio zero per UNSAT. */
    size_t conflict_cell;

    size_t resolved_count;
    size_t decision_depth;

    size_t traced_leaf_count;
    bool trace_truncated;

    /* Tutti zero se WANG_SOLVE_COLLECT_METRICS non e richiesto. */
    WangSolverMetrics metrics;
} WangSolveResult;

WangSolveStatus wang_solve_serial(
    const Region *region,
    const WangSolverOptions *options,
    WangSolveResult *out_result
);

void wang_solve_result_destroy(WangSolveResult *result);

#endif /* WANG_SOLVER_H */
```

Contratto:

- `options == NULL` equivale a flag zero;
- flag sconosciuti sono un errore;
- `out_result` deve essere azzerato o precedentemente distrutto;
- su `SAT` e `UNSAT`, il chiamante possiede `out_result->domains`;
- su `ERROR`, l'output resta completamente distrutto;
- `wang_solve_result_destroy()` accetta `NULL`, libera lo snapshot e azzera
  ogni campo;
- il solver non crea un file se il flag trace non e presente;
- con il flag trace sono obbligatori path non vuoto e capacita maggiore di
  zero;
- il path di trace e un output esplicito: viene creato o troncato dal solver.

## 6. Strutture private del solver

Tutto quanto segue deve restare in `solver_serial.c`.

```c
typedef struct {
    uint32_t edge_mask[DIR_COUNT][COLOR_COUNT];
    uint32_t compat[DIR_COUNT][TILE_COUNT];
} SolverTables;

typedef struct {
    size_t cell_index;
    uint32_t old_domain;
} TrailEntry;

typedef struct {
    size_t cell_index;
    uint32_t candidates;
    size_t entry_mark;
} SearchFrame;

typedef struct {
    const Region *region;
    SolverTables tables;

    uint32_t *domains;
    uint8_t *neighbor_mask;
    size_t cell_count;
    size_t active_count;
    size_t resolved_count;

    TrailEntry *trail;
    size_t trail_count;
    size_t trail_capacity;

    size_t *queue;
    size_t queue_capacity;

    uint32_t *best_snapshot;
    size_t best_resolved_count;
    size_t best_depth;
    size_t best_conflict_cell;
    bool has_best_snapshot;

    bool collect_metrics;
    WangSolverMetrics metrics;

    /* writer mmap privato, se abilitato */
} SolverState;
```

La separazione concettuale da preservare e:

- `SolverTables`: dati derivati immutabili, in futuro condivisibili;
- `SolverState`: dati mutabili privati del singolo ramo/worker.

Non usare skip list. Il trail e una pila contigua e l'MRV iniziale e una
scansione lineare cache-friendly.

## 7. Tabelle private di compatibilita

Costruire prima `edge_mask`:

```c
edge_mask[d][color] |= UINT32_C(1) << tile_id;
```

Poi derivare:

```c
compat[d][tile_id] =
    edge_mask[opposite(d)][TILESET[tile_id].edge[d]];
```

Significato:

- `edge_mask[d][c]`: tessere con colore `c` sul lato `d`;
- `compat[d][t]`: tessere ammesse nella cella che si trova in direzione `d`
  rispetto a una cella contenente `t`.

Le tabelle sono cache, non sorgenti di verita. Aggiungere un test esaustivo
indiretto o una funzione privata di assert/test che, per ogni `d`, `a`, `b`,
stabilisca l'equivalenza:

```text
bit b in compat[d][a]  <=>  wang_tiles_match(&TILESET[a], d, &TILESET[b])
```

Non esportare queste tabelle nell'header pubblico.

## 8. Inizializzazione dello stato

Validare il `Region` prima di allocare lo stato:

- puntatore e `cells` non nulli;
- dimensioni positive;
- prodotto e dimensioni di allocazione senza overflow;
- colori `COLOR_NONE` oppure `< COLOR_COUNT`;
- nessun boundary color su celle inattive;
- nessun boundary color su un lato con vicino attivo.

Per ogni cella:

1. se inattiva, `domain = 0` e `neighbor_mask = 0`;
2. se attiva, partire da `WANG_DOMAIN_ALL`;
3. costruire i quattro bit del `neighbor_mask`;
4. per ogni lato esposto con colore `c != COLOR_NONE`:

```c
domain &= tables.edge_mask[dir][c];
```

5. contare la cella in `resolved_count` se il dominio e singleton;
6. se il dominio diventa zero, si ha una leaf fallita a profondita zero.

Una regione valida senza celle attive e `SAT`: il suo snapshot contiene solo
zeri, ma non esiste alcun conflitto perche nessuna cella e attiva.

Dopo i boundary mask, eseguire una propagazione iniziale fino al punto fisso.
E consentito registrare le modifiche nel trail e poi porre `trail_count = 0`
senza rollback: quello diventa lo stato radice della DFS.

## 9. Undo trail

Ogni cambiamento reale di un dominio deve:

1. assicurare capacita nel vettore `trail`;
2. appendere `{ cell_index, old_domain }`;
3. aggiornare `resolved_count` confrontando cardinalita vecchia e nuova;
4. scrivere il nuovo dominio;
5. aggiornare metriche e picchi, se abilitati.

Prima di provare un candidato:

```c
const size_t mark = state->trail_count;
```

Il rollback scorre il trail al contrario fino a `mark`. Deve aggiornare anche
`resolved_count` confrontando il dominio corrente con quello ripristinato.

E corretto registrare piu volte la stessa cella: il rollback inverso ricrea
esattamente ogni stato intermedio. Non deduplicare il trail.

Non copiare l'intero array dei domini a ogni decisione.

## 10. Propagazione e snapshot delle leaf

Usare una coda contigua di indici. Nel baseline sono ammessi duplicati: si
aggiungera un `in_queue` soltanto se le metriche mostrano un problema reale.

Quando cambia il dominio della cella `i`, per ogni vicino attivo `j` nella
direzione `d`:

```c
uint32_t supported = 0;
uint32_t candidates = domains[i];

while (candidates != 0) {
    const TileId tile = first_set_bit(candidates);
    supported |= tables.compat[d][tile];
    candidates &= candidates - 1;
}

const uint32_t new_domain = domains[j] & supported;
```

Se il dominio cambia, salvarlo nel trail e accodare `j`. Se diventa zero,
conservare lo zero nello stato, registrare la leaf prima del rollback e
segnalare conflitto.

Il solver deve mantenere sempre in RAM la leaf migliore, anche quando il trace
su file e disabilitato. Ordine deterministico di preferenza:

1. maggior `resolved_count`;
2. a parita, maggiore profondita decisionale;
3. a ulteriore parita, mantenere la prima incontrata.

Per una leaf migliore copiare l'intero array in `best_snapshot` e salvare
`conflict_cell`, profondita e numero di celle risolte.

Questo snapshot UNSAT e diagnostico/renderizzabile, non e una prova formale di
insoddisfacibilita.

## 11. DFS iterativa e MRV

Il baseline deve essere deterministico:

- scegliere una cella attiva con dominio non singleton di cardinalita minima;
- a parita scegliere l'indice row-major piu piccolo;
- provare i `TileId` in ordine crescente;
- visitare i vicini nell'ordine `N`, `E`, `S`, `W`.

Una scansione MRV lineare e intenzionale: i domini sono contigui e la lettura
e cache-friendly. Non mantenere una struttura ordinata mutabile. Se il
profiling mostrera che MRV domina il tempo, la prima alternativa da provare e
una struttura a 24 bucket, non una skip list.

La visita usa uno stack esplicito allocato nell'heap, con al massimo un frame
per cella attiva. Questo evita che una regione grande esaurisca lo stack del
processo. Un frame contiene la cella MRV del nodo, i candidati non ancora
provati e il marker del trail precedente al ramo che ha aperto il nodo.

Schema DFS:

```text
conta il nodo radice
se tutto e risolto: SAT
inserisci il frame MRV radice

finche lo stack non e vuoto:
    frame = cima dello stack

    se non restano candidati:
        rimuovi il frame
        se era la radice: UNSAT
        rollback(frame.entry_mark)
        conta il backtrack del ramo padre
        continua

    estrai il TileId minimo dai candidati del frame
    mark = trail_count
    restringi la cella al singleton e propaga

    se c'e conflitto:
        registra la leaf
        rollback(mark) e conta il backtrack
    altrimenti se tutto e risolto:
        SAT senza rollback
    altrimenti:
        conta il nuovo nodo
        inserisci { nuova cella MRV, suo dominio, mark }
```

La profondita di un ramo coincide con il numero di frame durante il tentativo
del candidato. Ordine di visita, metriche e semantica del rollback restano gli
stessi della formulazione ricorsiva.

Fallimenti di allocazione o del writer sono `ERROR`, non `UNSAT`.

Non implementare memoization nel baseline. Con ordine deterministico e domini
monotoni e improbabile raggiungere due volte lo stesso stato globale, mentre
una hash table introdurrebbe memoria e accessi casuali.

## 12. Verifica obbligatoria del risultato SAT

Prima di pubblicare `SAT`:

1. costruire temporaneamente un array `TileId[cell_count]`;
2. scrivere `TILE_NONE` sulle celle inattive;
3. estrarre il solo bit da ogni dominio attivo singleton;
4. chiamare `wang_verify_tiling()`;
5. accettare `SAT` soltanto se il risultato e `WANG_VERIFY_VALID`.

Se il verificatore rifiuta un output del solver, restituire `ERROR`: e un bug
interno, non `UNSAT`.

Solo dopo questa verifica copiare i domini finali nello snapshot pubblico.

## 13. Metriche opzionali

Aggiornare i contatori pubblici soltanto se e presente
`WANG_SOLVE_COLLECT_METRICS`. I campi devono essere tutti zero altrimenti.

Definizioni da mantenere stabili nei test e nella documentazione:

- `dfs_nodes`: stati di ricerca visitati, inclusa la radice;
- `decisions`: candidati singleton effettivamente tentati;
- `backtracks`: candidati falliti e ripristinati;
- `failed_leaves`: conflitti terminali osservati;
- `domain_reductions`: scritture che restringono davvero un dominio;
- `propagated_arcs`: archi cella-vicino elaborati;
- `mrv_cells_scanned`: celle attive ispezionate dalle scansioni MRV;
- `trail_peak`: massimo numero simultaneo di entry nel trail;
- `queue_peak`: massimo numero di indici non ancora estratti presenti
  simultaneamente nella coda di una propagazione;
- `max_depth`: massima profondita DFS raggiunta.

Il tempo non appartiene a `SolverMetrics`: benchmark e chiamante misurano il
tempo esternamente.

## 14. Trace binario delle leaf tramite mmap

### 14.1 Semantica

Il trace e opzionale. Se abilitato, contiene ogni leaf fallita incontrata
prima del rollback, fino a `failed_leaf_capacity`.

Il file e diagnostico, non un certificato formale UNSAT. Puo contenere leaf
anche quando il risultato finale e `SAT`, perche il solver puo aver fallito
rami precedenti.

Il numero esatto di leaf non e noto prima della ricerca. E noto invece:

```text
record_size = aligned_record_prefix + cell_count * sizeof(uint32_t)
allocated_file_size = file_header_size
                    + failed_leaf_capacity * record_size
```

Il file viene quindi preallocato alla capacita richiesta e troncato al numero
di record realmente scritto al termine.

### 14.2 Formato versione 1

Non scrivere direttamente struct C senza controllarne layout e dimensione.
Il formato v1 e little-endian e ha header di 64 byte.

```c
typedef struct {
    char magic[8];             /* "W23LEAF\0" */
    uint32_t version;          /* 1 */
    uint32_t header_size;      /* 64 */
    uint32_t width;
    uint32_t height;
    uint32_t tile_count;       /* 23 */
    uint32_t flags;            /* bit 0: trace_truncated */
    uint64_t cell_count;
    uint64_t record_size;
    uint64_t record_capacity;
    uint64_t record_count;
} FailedLeafFileHeader;
```

Il layout sopra e schematico. L'implementazione attuale scrive ogni campo a
offset esplicito con helper little-endian e non dipende da padding o
`sizeof(FailedLeafFileHeader)`. Se in futuro si decide di scrivere una struct
C direttamente, aggiungere almeno:

```c
_Static_assert(sizeof(FailedLeafFileHeader) == 64,
               "failed-leaf header must be 64 bytes");
```

Lo `_Static_assert` controlla il layout, non l'endianness. Scrivere i campi con
piccoli helper little-endian oppure rifiutare esplicitamente a compile/runtime
una piattaforma non little-endian; non produrre silenziosamente un file con
byte order diverso da quello dichiarato.

Ogni record comincia con 32 byte:

```c
typedef struct {
    uint64_t leaf_index;
    uint64_t conflict_cell;
    uint64_t decision_depth;
    uint64_t resolved_count;
} FailedLeafRecordHeader;
```

Seguono esattamente `cell_count` valori `uint32_t`, in ordine row-major. Il
record viene completato con zero padding fino al multiplo di 8 successivo:

```text
raw_record_size = 32 + 4 * cell_count
record_size = align_up(raw_record_size, 8)
```

Controllare tutti gli overflow prima di `open`, `ftruncate` e `mmap`.

### 14.3 Ciclo di vita del writer

Il writer e isolato in `src/solver/failed_leaf_trace.c` con header privato al
solver. Non renderlo parte dell'API generale di serializzazione.

Sequenza:

1. aprire `failed_leaf_path` con `O_RDWR | O_CREAT | O_TRUNC`, modo `0666`;
2. calcolare la dimensione massima in modo checked;
3. `ftruncate(fd, allocated_size)`;
4. `mmap(..., PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)`;
5. inizializzare l'header con `record_count = 0`;
6. per ogni leaf disponibile fare `memcpy` di header record e domini;
7. se arriva una leaf oltre capacita, non scriverla e impostare
   `trace_truncated = true`;
8. alla fine aggiornare header, `record_count` e flags;
9. fare `msync` della parte usata;
10. `munmap` dell'intera mapping;
11. troncare a:

```text
64 + record_count * record_size
```

12. chiudere il descrittore.

Non chiamare `ftruncate` verso il basso mentre la mapping e ancora usata.

Se setup, scrittura, sync o finalizzazione falliscono, pulire tutte le risorse
e restituire `WANG_SOLVE_ERROR`. Il risultato pubblico deve restare distrutto.
Il file parziale puo rimanere come diagnostica, ma non deve essere presentato
come trace completo.

### 14.4 Futuro multithread

Non usare oggi lock o atomiche. In futuro non fare scrivere piu worker nella
stessa mapping ridimensionabile: assegnare a ogni worker un segmento o file
privato e unire gli indici dopo la ricerca. Questa nota non autorizza alcuna
implementazione OpenMP in questo blocco.

## 15. Gestione degli errori e ownership

Ogni allocazione deve avere un controllo di overflow precedente. Su qualsiasi
errore:

- rollback/cleanup dello stato privato;
- finalizzazione o chiusura sicura del writer se aperto;
- `free` di domini, neighbor mask, trail, queue, snapshot e array temporanei;
- output pubblico azzerato;
- `WANG_SOLVE_ERROR`.

Costruire il risultato in una variabile locale e trasferirlo in
`out_result` soltanto al termine. Non pubblicare campi progressivamente.

Il solver deve rifiutare un `out_result` non distrutto per evitare leak o
overwrite silenziosi.

## 16. Test richiesti

### 16.1 Verificatore

Testare almeno:

- argomenti nulli e lunghezza errata;
- `TILE_NONE` su cella attiva;
- `TileId` fuori range;
- tessera assegnata a cella inattiva;
- singola cella con tutti i boundary corretti;
- mismatch di bordo in ciascuna direzione;
- due celle compatibili e incompatibili orizzontalmente;
- due celle compatibili e incompatibili verticalmente;
- regione con cella inattiva/buco;
- `Region` corrotto manualmente: colore invalido o boundary su lato interno.

### 16.2 Solver

Testare almeno:

- opzioni e output invalidi;
- regione vuota di celle attive -> `SAT`;
- singola cella forzata -> `SAT`, dominio singleton corretto;
- singola cella con boundary impossibile -> `UNSAT`, snapshot presente e
  `conflict_cell` valido;
- piccole regioni SAT e UNSAT confrontate con un brute force indipendente nei
  test;
- risultato SAT sempre accettato da `wang_verify_tiling()`;
- determinismo: due esecuzioni producono stesso status e stesso snapshot;
- metriche tutte zero senza flag e coerenti con flag;
- rollback dopo piu livelli, incluse modifiche multiple della stessa cella;
- una regione non vincolata che superi diecimila livelli decisionali, per
  esercitare lo stack DFS esplicito senza dipendere dallo stack del processo;
- input e `Region` non modificati;
- output distrutto dopo errore e destroy idempotente.

Il brute force di test deve enumerare direttamente i `TileId` su regioni molto
piccole e usare il verificatore; non deve chiamare il solver o le sue cache.

### 16.3 Trace mmap

Usare un path temporaneo posseduto dal test. Verificare:

- nessun file creato senza flag;
- errore con path nullo/vuoto o capacita zero;
- magic, versione, dimensioni e `record_size`;
- almeno una leaf scritta per un caso UNSAT;
- `record_count == out_result.traced_leaf_count`;
- dimensione finale esatta:

```text
64 + record_count * record_size
```

- il dominio della `conflict_cell` nel record e zero;
- cap raggiunto: ulteriori leaf non vengono scritte e
  `trace_truncated == true`;
- file leggibile dopo `munmap`/close;
- cleanup corretto su errore del writer.

Non lasciare file temporanei dopo i test.

### 16.4 Integrazione Yang-Zhang

Dopo che i test piccoli sono stabili:

- costruire la formula minima `{0,0,0}` con `yang_zhang_build()` e confrontare
  il risultato del solver con la semantica CM1-in-3 attesa (`UNSAT`);
- provare l'istanza a tre variabili documentata nel builder, che ammette
  l'assegnazione `(0,0,1)`, e richiedere un tiling `SAT` verificato;
- conservare una regressione in cui l'unico segnale vero entra nella prima
  riga di una clausola;
- enumerare tutte le 1701 formule canoniche fino a tre variabili e confrontare
  ogni risultato con l'oracolo Booleano;
- se questi test risultano troppo costosi per `make check`, marcarli come
  target di integrazione separato invece di indebolire i test unitari.

## 17. Controlli e profiling

Eseguire durante lo sviluppo:

```sh
make clean
make check
make valgrind-check
make cachegrind-check
```

Se Valgrind non e installato, `make check` resta il gate obbligatorio.

Usare le metriche e Cachegrind per rispondere prima di ottimizzare:

- quanta parte del lavoro e MRV scan;
- quanti duplicati entrano nella queue;
- quanto cresce il trail;
- quante riduzioni produce ogni decisione;
- quanto costa copiare ogni leaf nel trace.

Ottimizzazioni ammesse solo dopo misura:

- 24 bucket MRV;
- bitset di celle per bucket;
- deduplicazione della propagation queue;
- trace per delta o decision path invece di snapshot completi.

Non introdurre skip list o memoization globale come prima ottimizzazione.

## 18. Ordine di implementazione consigliato

1. Implementare `verify.h`, `verify_tiling.c` e `test_verify.c`.
2. Definire `solver.h` e testare lifetime/validazione del risultato.
3. Implementare e verificare `edge_mask` e `compat` private.
4. Inizializzare domini e neighbor mask dai boundary.
5. Implementare trail, rollback e propagation queue.
6. Implementare DFS/MRV con uno stack esplicito e senza trace.
7. Verificare obbligatoriamente ogni risultato SAT.
8. Implementare selezione e ritorno della migliore leaf UNSAT.
9. Aggiungere metriche dietro flag.
10. Implementare writer mmap e parser minimo del formato nei test.
11. Aggiungere regressioni deterministiche e confronti brute force.
12. Eseguire check, Valgrind e Cachegrind.
13. Solo a lavoro completato aggiornare il README da "not implemented" a
    stato realmente raggiunto.

## 19. Definition of done

Il blocco e completo quando:

- verifier e solver hanno contratti pubblici documentati;
- il solver restituisce `SAT`, `UNSAT` o `ERROR` senza ambiguita;
- `SAT` contiene domini singleton e passa il verifier indipendente;
- `UNSAT` contiene sempre una leaf fallita renderizzabile;
- il trace mmap opzionale contiene record validi, rispetta il cap ed e
  troncato alla dimensione effettiva;
- metriche assenti/presenti rispettano il flag;
- nessuna cache privata diventa una seconda sorgente di verita;
- il solver resta indipendente dalla geometria e dai metadata del builder;
- `make check` passa senza warning;
- i test non perdono memoria e non lasciano file temporanei.
