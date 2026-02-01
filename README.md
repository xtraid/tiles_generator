# Sistema Props - Guida Rapida

## Setup Iniziale

1. **Crea props di esempio:**
```bash
   python create_example_props.py
```

2. **Lancia il gioco:**
```bash
   python hex_tiles_main.py
```

3. **Lancia l'editor (opzionale):**
```bash
   python prop_editor.py
```

## Controlli in Gioco

### Movimento Base
- **Q/E/A/D/S/X**: Movimento orizzontale (6 direzioni)
- **W**: Sali di un layer (z+1)
- **Z**: Scendi di un layer (z-1)
- **SPACE**: Genera tiles vicine

### Sistema Props
- **TAB**: Cicla tra le 7 sezioni del tile corrente
  - 1 sezione centrale (center)
  - 6 sezioni radiali (wedge 0-5)
- **P**: Apri menu per piazzare un prop
- **R**: Rimuovi prop dalla sezione selezionata
- **F5**: Salva tutti i props piazzati
- **F9**: Carica props salvati

## Editor Props

### Navigazione
- **↑/↓**: Seleziona campo da modificare
- **←/→**: Modifica valore (per campi con opzioni)
- **ENTER**: Attiva modalità edit per inserire testo
- **CTRL+S**: Salva prop corrente
- **CTRL+N**: Crea nuovo prop
- **ESC**: Esci

### Campi Configurabili
- **prop_id**: ID univoco (es: "tree_oak_01")
- **name**: Nome descrittivo (es: "Quercia")
- **section_type**: "center" o "wedge"
- **section_index**: 0-5 per wedge, None per center
- **visual_type**: "circle", "rect", o "triangle"
- **color_r/g/b**: Colore RGB (0-255)
- **size_param**: Dimensione (radius per circle, width/height per rect, size per triangle)
- **blocking**: YES/NO - blocca generazione tiles?
- **z_offset**: Altezza verticale (0.0 = base)

## Struttura File
```
/props/                    # Props definitions (JSON)
  ├── tree_pine_01.json
  ├── rock_small_01.json
  └── ...

props_placement.json       # Salvataggio posizioni props (F5/F9)

prop.py                    # Classe Prop
prop_manager.py            # Gestione props
prop_editor.py             # Editor grafico
hex_tiles_main.py          # Gioco principale (modificato)
create_example_props.py    # Script per props di esempio
```

## Tips

1. **Props Bloccanti**: I props con `blocking=True` impediscono la generazione di tiles a quella coordinata Z
2. **Layering**: Usa `z_offset` per sovrapporre props (valori più alti appaiono sopra)
3. **Sezioni Wedge**: Utili per recinzioni, muri, decorazioni sui bordi
4. **Sezione Center**: Perfetta per oggetti centrali come alberi, case, rocce

## Esempi d'Uso

### Creare un Bosco
1. Genera alcune tiles con SPACE
2. Premi TAB fino a selezionare "center"
3. Premi P e seleziona "tree_pine_01"
4. Ripeti su più tiles

### Creare una Recinzione
1. Posizionati su un tile
2. Premi TAB fino a "wedge 0" (o altra direzione)
3. Premi P e seleziona "fence_wood_01"
4. Muoviti al tile successivo e ripeti

### Salvare il Lavoro
- Premi F5 per salvare
- La prossima volta premi F9 per caricare


