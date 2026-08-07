"""
Sistema Biomi - Definizioni e Colorazioni
"""

import random

# ========== BIOMI DISPONIBILI ==========
BIOMES = {
    'forest': {
        'name': 'Foresta',
        'colors': [
            (34, 139, 34),   # Verde scuro
            (50, 155, 50),   # Verde chiaro
            (40, 150, 40),   # Verde medio
            (45, 145, 45),   # Verde
            (30, 130, 30),   # Verde scuro 2
            (20, 120, 20),   # Verde molto scuro
            (55, 160, 55),   # Verde brillante
            (139, 90, 43),   # Marrone terra
            (160, 100, 50)   # Marrone chiaro
        ]
    },
    'desert': {
        'name': 'Deserto',
        'colors': [
            (238, 214, 175), # Sabbia chiara
            (228, 204, 165), # Sabbia
            (233, 209, 170), # Sabbia medio
            (243, 219, 180), # Sabbia chiara 2
            (223, 199, 160), # Sabbia scura
            (248, 224, 185), # Sabbia molto chiara
            (194, 178, 128), # Sabbia scura 2
            (139, 90, 43)    # Marrone terra
        ]
    },
    'mountain': {
        'name': 'Montagna',
        'colors': [
            (139, 137, 137), # Grigio
            (149, 147, 147), # Grigio chiaro
            (129, 127, 127), # Grigio scuro
            (144, 142, 142), # Grigio medio
            (134, 132, 132), # Grigio scuro 2
            (154, 152, 152), # Grigio chiaro 2
            (169, 169, 169), # Grigio DarkGray
            (105, 105, 105)  # Grigio DimGray
        ]
    },
    'water': {
        'name': 'Acqua',
        'colors': [
            (30, 144, 255),  # Azzurro
            (40, 154, 255),  # Azzurro chiaro
            (25, 139, 245),  # Azzurro scuro
            (35, 149, 250),  # Azzurro medio
            (20, 134, 240),  # Azzurro scuro 2
            (45, 159, 255),  # Azzurro brillante
            (0, 191, 255),   # DeepSkyBlue
            (135, 206, 250)  # LightSkyBlue
        ]
    },
    'city': {
        'name': 'Città',
        'colors': [
            (100, 100, 100), # Grigio muro
            (120, 120, 120), # Grigio chiaro
            (80, 80, 80),    # Grigio scuro
            (139, 90, 43),   # Marrone pietrini
            (160, 100, 50),  # Marrone chiaro
            (160, 82, 45),   # Terra battuta
            (105, 105, 105), # Grigio cemento
            (169, 169, 169), # Grigio asfalto
            (70, 70, 70)     # Grigio molto scuro
        ]
    },
    'swamp': {
        'name': 'Palude',
        'colors': [
            (34, 139, 34),   # Verde scuro
            (85, 107, 47),   # Verde oliva scuro
            (107, 142, 35),  # Verde oliva
            (75, 83, 32),    # Verde palude
            (139, 90, 43),   # Marrone fango
            (101, 67, 33),   # Marrone scuro
            (47, 79, 79),    # Verde-grigio scuro
            (0, 100, 0)      # Verde bosco scuro
        ]
    }
}


def get_biome_from_description(description_text=""):
    """
    FUTURO: Analizza descrizione e ritorna bioma appropriato
    
    Es: "una foresta densa e oscura" → 'forest'
        "deserto roccioso con dune" → 'desert'
        "città medievale con mura" → 'city'
    
    Per ora: return random tra biomi disponibili
    
    TODO: Implementare logica NLP/keyword matching quando necessario
    """
    return random.choice(list(BIOMES.keys()))


def get_random_color_from_biome(biome):
    """Ritorna un colore random dal bioma specificato"""
    if biome not in BIOMES:
        return (150, 150, 150)  # Grigio default
    
    return random.choice(BIOMES[biome]['colors'])


def get_biome_names():
    """Ritorna lista nomi biomi per UI"""
    return [(key, data['name']) for key, data in BIOMES.items()]
