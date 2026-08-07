#!/usr/bin/env python3
"""
PALETTE MANAGER - Gestisce palette colori GLOBALI per biomi
Le palette sono REFERENZE: modificare un colore aggiorna tutte le mesh che lo usano
"""

import json
from pathlib import Path
from typing import List, Tuple

class PaletteManager:
    """Gestisce palette globali dei biomi"""
    
    def __init__(self, palettes_dir="biomes_palettes"):
        self.palettes_dir = Path(palettes_dir)
        self.palettes_dir.mkdir(exist_ok=True)
        
        # Cache palette caricate
        self.palettes = {}  # {biome_name: [color0, color1, ...]}
        
        # Inizializza palette default se non esistono
        self._init_default_palettes()
    
    def _init_default_palettes(self):
        """Crea palette default per biomi standard"""
        default_palettes = {
            'forest': [
                (34, 139, 34),   # 0: Verde scuro
                (50, 155, 50),   # 1: Verde chiaro
                (40, 150, 40),   # 2: Verde medio
                (45, 145, 45),   # 3: Verde
                (30, 130, 30),   # 4: Verde scuro 2
                (20, 120, 20),   # 5: Verde molto scuro
                (55, 160, 55)    # 6: Verde brillante
            ],
            'desert': [
                (238, 214, 175), # 0: Sabbia chiara
                (228, 204, 165), # 1: Sabbia
                (233, 209, 170), # 2: Sabbia medio
                (243, 219, 180), # 3: Sabbia chiara 2
                (223, 199, 160), # 4: Sabbia scura
                (248, 224, 185), # 5: Sabbia molto chiara
                (218, 194, 155)  # 6: Sabbia scura 2
            ],
            'mountain': [
                (139, 137, 137), # 0: Grigio
                (149, 147, 147), # 1: Grigio chiaro
                (129, 127, 127), # 2: Grigio scuro
                (144, 142, 142), # 3: Grigio medio
                (134, 132, 132), # 4: Grigio scuro 2
                (154, 152, 152), # 5: Grigio chiaro 2
                (124, 122, 122)  # 6: Grigio molto scuro
            ],
            'water': [
                (30, 144, 255),  # 0: Azzurro
                (40, 154, 255),  # 1: Azzurro chiaro
                (25, 139, 245),  # 2: Azzurro scuro
                (35, 149, 250),  # 3: Azzurro medio
                (20, 134, 240),  # 4: Azzurro scuro 2
                (45, 159, 255),  # 5: Azzurro brillante
                (15, 129, 235)   # 6: Azzurro molto scuro
            ],
            'snow': [
                (240, 248, 255), # 0: Bianco neve
                (230, 238, 245), # 1: Bianco ghiaccio
                (220, 228, 235), # 2: Grigio chiaro neve
                (210, 218, 225), # 3: Grigio neve
                (200, 208, 215), # 4: Grigio scuro neve
                (250, 250, 250), # 5: Bianco puro
                (190, 198, 205)  # 6: Grigio molto scuro
            ],
            'lava': [
                (255, 69, 0),    # 0: Rosso lava
                (255, 140, 0),   # 1: Arancione
                (220, 50, 0),    # 2: Rosso scuro
                (255, 100, 0),   # 3: Rosso-arancio
                (200, 40, 0),    # 4: Rosso molto scuro
                (255, 165, 0),   # 5: Arancione chiaro
                (180, 30, 0)     # 6: Rosso nero
            ],
            'swamp': [
                (85, 107, 47),   # 0: Verde palude
                (107, 142, 35),  # 1: Verde oliva
                (75, 97, 37),    # 2: Verde scuro palude
                (95, 117, 47),   # 3: Verde medio
                (65, 87, 27),    # 4: Verde molto scuro
                (105, 127, 57),  # 5: Verde chiaro
                (55, 77, 17)     # 6: Verde nero
            ],
            'crystal': [
                (147, 112, 219), # 0: Viola cristallo
                (138, 43, 226),  # 1: Viola intenso
                (186, 85, 211),  # 2: Viola medio
                (153, 50, 204),  # 3: Viola scuro
                (218, 112, 214), # 4: Rosa orchidea
                (199, 21, 133),  # 5: Rosa medio
                (128, 0, 128)    # 6: Viola puro
            ]
        }
        
        for biome_name, colors in default_palettes.items():
            filepath = self.palettes_dir / f"{biome_name}.json"
            if not filepath.exists():
                self.save_palette(biome_name, colors)
    
    def load_palette(self, biome_name: str) -> List[Tuple[int, int, int]]:
        """Carica palette da file o cache
        
        Returns:
            Lista di 7 colori RGB come tuple
        """
        # Check cache
        if biome_name in self.palettes:
            return self.palettes[biome_name]
        
        # Carica da file
        filepath = self.palettes_dir / f"{biome_name}.json"
        
        if not filepath.exists():
            print(f"⚠️  Palette '{biome_name}' non trovata, uso forest")
            biome_name = 'forest'
            filepath = self.palettes_dir / f"{biome_name}.json"
        
        try:
            with open(filepath, 'r') as f:
                data = json.load(f)
            
            colors = [tuple(c) for c in data['colors']]
            
            # Assicurati che ci siano 7 colori
            while len(colors) < 7:
                colors.append((128, 128, 128))
            
            self.palettes[biome_name] = colors[:7]
            return self.palettes[biome_name]
        
        except Exception as e:
            print(f"❌ Errore caricando palette {biome_name}: {e}")
            # Fallback
            default = [(128, 128, 128)] * 7
            self.palettes[biome_name] = default
            return default
    
    def save_palette(self, biome_name: str, colors: List[Tuple[int, int, int]]):
        """Salva palette su file
        
        Args:
            biome_name: Nome bioma
            colors: Lista di 7 colori RGB
        """
        # Assicurati 7 colori
        while len(colors) < 7:
            colors.append((128, 128, 128))
        
        colors = colors[:7]
        
        data = {
            'biome': biome_name,
            'colors': [[int(r), int(g), int(b)] for r, g, b in colors]
        }
        
        filepath = self.palettes_dir / f"{biome_name}.json"
        
        with open(filepath, 'w') as f:
            json.dump(data, f, indent=2)
        
        # Aggiorna cache
        self.palettes[biome_name] = colors
        
        print(f"✓ Palette '{biome_name}' salvata")
    
    def get_available_biomes(self) -> List[str]:
        """Ritorna lista nomi biomi disponibili"""
        return [f.stem for f in self.palettes_dir.glob("*.json")]
    
    def get_color_by_index(self, biome_name: str, index: int) -> Tuple[int, int, int]:
        """Ottiene colore specifico dalla palette
        
        Args:
            biome_name: Nome bioma
            index: Indice colore (0-6)
        
        Returns:
            Tupla RGB
        """
        palette = self.load_palette(biome_name)
        if 0 <= index < len(palette):
            return palette[index]
        return (128, 128, 128)
    
    def resolve_colors(self, biome_name: str, indices: List[int]) -> List[Tuple[int, int, int]]:
        """Risolve lista di indici in colori effettivi
        
        Args:
            biome_name: Nome bioma
            indices: Lista indici (es: [0, 2, 5])
        
        Returns:
            Lista colori RGB corrispondenti
        """
        palette = self.load_palette(biome_name)
        return [palette[i] if 0 <= i < len(palette) else (128, 128, 128) for i in indices]


# Istanza globale
_palette_manager = None

def get_palette_manager():
    """Ottiene istanza singleton del PaletteManager"""
    global _palette_manager
    if _palette_manager is None:
        _palette_manager = PaletteManager()
    return _palette_manager
