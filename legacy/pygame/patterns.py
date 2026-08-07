"""
Pattern Engine - Generazione pattern procedurali per sezioni hex tiles

STRUTTURA:
1. Mesh base con colori (gradiente/random)
2. Pattern overlay
3. Variazioni (shading, positioning, noise)
"""

import pygame
import random
import math
from pathlib import Path


# ========== PATTERN DISPONIBILI ==========
# Questo sarÃ  espanso con file di istruzioni che l'utente fornirÃ 

PATTERN_DEFINITIONS = {
    'solid': {
        'name': 'Tinta Unita',
        'description': 'Colore solido senza pattern'
    },
    'bricks': {
        'name': 'Mattoni',
        'description': 'Pattern a mattoni rettangolari'
    },
    'cobblestone': {
        'name': 'Pietrini',
        'description': 'Pietrini rotondi irregolari'
    },
    'organic_scatter': {
        'name': 'Scatter Organico',
        'description': 'Blob organici casuali (foresta/erba)'
    },
    'tiles': {
        'name': 'Piastrelle',
        'description': 'Piastrelle quadrate regolari'
    },
    'herringbone': {
        'name': 'Spina di Pesce',
        'description': 'Pattern a spina di pesce'
    },
    'sand_ripples': {
        'name': 'Onde Sabbia',
        'description': 'Onde parallele (deserto)'
    },
    'rocky': {
        'name': 'Rocce',
        'description': 'Rocce frammentate irregolari'
    }
}

# Variazioni disponibili
VARIATION_TYPES = {
    'random_shading': {
        'name': 'Ombreggiatura Random',
        'description': 'Ogni elemento colore leggermente diverso',
        'default_intensity': 0.3
    },
    'random_positioning': {
        'name': 'Posizionamento Random',
        'description': 'Elementi leggermente sfalsati',
        'default_intensity': 0.2
    },
    'noise_overlay': {
        'name': 'Noise Overlay',
        'description': 'Texture rumore sovrapposta',
        'default_intensity': 0.15
    },
    'combo': {
        'name': 'Combinazione',
        'description': 'Tutte le variazioni insieme',
        'default_intensity': 0.2
    }
}


# ========== GENERATORE MESH BASE ==========

def generate_color_mesh(surface, colors, mesh_type='gradient'):
    """
    Genera mesh base multicolore
    
    Args:
        surface: pygame.Surface da riempire
        colors: lista colori da usare
        mesh_type: 'gradient', 'random_spots', 'horizontal_bands'
    """
    width, height = surface.get_size()
    
    if mesh_type == 'gradient':
        # Gradiente verticale tra i colori
        for y in range(height):
            progress = y / height
            color_index = int(progress * (len(colors) - 1))
            next_color_index = min(color_index + 1, len(colors) - 1)
            
            # Interpola tra due colori
            local_progress = (progress * (len(colors) - 1)) - color_index
            color = interpolate_color(colors[color_index], colors[next_color_index], local_progress)
            
            pygame.draw.line(surface, color, (0, y), (width, y))
    
    elif mesh_type == 'random_spots':
        # Riempi con colore base, poi spot random altri colori
        surface.fill(colors[0])
        
        num_spots = (width * height) // 100
        for _ in range(num_spots):
            x = random.randint(0, width - 1)
            y = random.randint(0, height - 1)
            radius = random.randint(3, 8)
            color = random.choice(colors)
            pygame.draw.circle(surface, color, (x, y), radius)
    
    elif mesh_type == 'horizontal_bands':
        # Bande orizzontali alternate
        band_height = max(3, height // len(colors))
        for i, color in enumerate(colors):
            y_start = i * band_height
            y_end = min((i + 1) * band_height, height)
            pygame.draw.rect(surface, color, (0, y_start, width, y_end - y_start))


def interpolate_color(color1, color2, t):
    """Interpola tra due colori (t da 0 a 1)"""
    r = int(color1[0] + (color2[0] - color1[0]) * t)
    g = int(color1[1] + (color2[1] - color1[1]) * t)
    b = int(color1[2] + (color2[2] - color1[2]) * t)
    return (r, g, b)


# ========== GENERATORI PATTERN ==========

def draw_bricks_pattern(surface, colors, variation_config, scale_factor=1.0):
    """Pattern mattoni rettangolari con scala adattata"""
    width, height = surface.get_size()
    brick_width = int(16 * scale_factor)
    brick_height = int(8 * scale_factor)
    mortar_size = max(1, int(2 * scale_factor))
    
    y = 0
    row = 0
    while y < height:
        x_offset = (brick_width // 2) if row % 2 == 1 else 0
        x = x_offset
        
        while x < width:
            # Colore mattone (con variazione)
            base_color = random.choice(colors)
            color = apply_color_variation(base_color, variation_config)
            
            # Posizione (con variazione)
            brick_x = x
            brick_y = y
            if variation_config['type'] in ['random_positioning', 'combo']:
                brick_x += random.randint(-2, 2)
                brick_y += random.randint(-1, 1)
            
            # Disegna mattone
            rect = pygame.Rect(brick_x, brick_y, brick_width - mortar_size, brick_height - mortar_size)
            pygame.draw.rect(surface, color, rect)
            
            # Bordo scuro mattone
            darker = tuple(max(0, c - 20) for c in color)
            pygame.draw.rect(surface, darker, rect, 1)
            
            x += brick_width
        
        y += brick_height
        row += 1


def draw_cobblestone_pattern(surface, colors, variation_config, scale_factor=1.0):
    """Pattern pietrini rotondi con scala adattata"""
    width, height = surface.get_size()
    stone_size = int(8 * scale_factor)
    
    for y in range(0, height, stone_size):
        for x in range(0, width, stone_size):
            # Variazione posizione
            pos_x = x + random.randint(-2, 2)
            pos_y = y + random.randint(-2, 2)
            
            # Variazione dimensione
            radius = stone_size // 2 + random.randint(-1, 1)
            
            # Colore con variazione
            base_color = random.choice(colors)
            color = apply_color_variation(base_color, variation_config)
            
            # Disegna pietrino
            pygame.draw.circle(surface, color, (pos_x, pos_y), radius)
            
            # Ombra
            darker = tuple(max(0, c - 30) for c in color)
            pygame.draw.circle(surface, darker, (pos_x, pos_y), radius, 1)


def draw_organic_scatter_pattern(surface, colors, variation_config, scale_factor=1.0):
    """Pattern blob organici (foresta/erba) con scala adattata"""
    width, height = surface.get_size()
    
    # Genera blob casuali - scala il numero in base alla dimensione
    base_density = 80
    num_blobs = int((width * height) / (base_density * scale_factor))
    
    for _ in range(num_blobs):
        x = random.randint(0, width)
        y = random.randint(0, height)
        
        # Forma organica (ellisse con rotazione random) - scala dimensioni
        w = int(random.randint(8, 20) * scale_factor)
        h = int(random.randint(6, 15) * scale_factor)
        
        base_color = random.choice(colors)
        color = apply_color_variation(base_color, variation_config)
        
        # Disegna ellisse
        rect = pygame.Rect(x - w//2, y - h//2, w, h)
        pygame.draw.ellipse(surface, color, rect)


def draw_tiles_pattern(surface, colors, variation_config, scale_factor=1.0):
    """Pattern piastrelle quadrate con scala adattata"""
    width, height = surface.get_size()
    tile_size = int(12 * scale_factor)
    grout_size = max(1, int(2 * scale_factor))
    
    for y in range(0, height, tile_size):
        for x in range(0, width, tile_size):
            base_color = random.choice(colors)
            color = apply_color_variation(base_color, variation_config)
            
            rect = pygame.Rect(x, y, tile_size - grout_size, tile_size - grout_size)
            pygame.draw.rect(surface, color, rect)
            
            # Riflesso
            lighter = tuple(min(255, c + 20) for c in color)
            pygame.draw.line(surface, lighter, 
                           (rect.left, rect.top), 
                           (rect.right, rect.top), 1)


def draw_sand_ripples_pattern(surface, colors, variation_config, scale_factor=1.0):
    """Pattern onde sabbia parallele con scala adattata"""
    width, height = surface.get_size()
    wave_height = int(6 * scale_factor)
    
    for y in range(0, height, wave_height):
        base_color = random.choice(colors)
        color = apply_color_variation(base_color, variation_config)
        
        # Onda sinusoidale - adatta frequenza alla scala
        points = []
        wave_freq = 0.1 / scale_factor  # Frequenza inversamente proporzionale
        for x in range(width):
            wave_y = y + int(math.sin(x * wave_freq) * 2 * scale_factor)
            points.append((x, wave_y))
        
        if len(points) > 1:
            line_width = max(1, int(2 * scale_factor))
            pygame.draw.lines(surface, color, False, points, line_width)


def draw_rocky_pattern(surface, colors, variation_config, scale_factor=1.0):
    """Pattern rocce frammentate con scala adattata"""
    width, height = surface.get_size()
    
    # Genera poligoni irregolari - scala densità
    base_density = 150
    num_rocks = int((width * height) / (base_density * scale_factor))
    
    for _ in range(num_rocks):
        cx = random.randint(0, width)
        cy = random.randint(0, height)
        
        # Poligono irregolare (4-6 lati) - scala dimensioni
        num_sides = random.randint(4, 6)
        points = []
        for i in range(num_sides):
            angle = (2 * math.pi * i) / num_sides + random.uniform(-0.3, 0.3)
            radius = int(random.randint(5, 15) * scale_factor)
            x = cx + int(math.cos(angle) * radius)
            y = cy + int(math.sin(angle) * radius)
            points.append((x, y))
        
        base_color = random.choice(colors)
        color = apply_color_variation(base_color, variation_config)
        
        if len(points) >= 3:
            pygame.draw.polygon(surface, color, points)
            darker = tuple(max(0, c - 25) for c in color)
            pygame.draw.polygon(surface, darker, points, 1)


# ========== APPLICAZIONE VARIAZIONI ==========

def apply_color_variation(base_color, variation_config):
    """Applica variazione a un colore base"""
    var_type = variation_config['type']
    intensity = variation_config.get('intensity', 0.2)
    
    if var_type == 'none':
        return base_color
    
    r, g, b = base_color
    
    if var_type in ['random_shading', 'combo']:
        # Variazione random su ogni canale
        variation_range = int(50 * intensity)
        r += random.randint(-variation_range, variation_range)
        g += random.randint(-variation_range, variation_range)
        b += random.randint(-variation_range, variation_range)
    
    # Clamp valori
    r = max(0, min(255, r))
    g = max(0, min(255, g))
    b = max(0, min(255, b))
    
    return (r, g, b)


def apply_noise_overlay(surface, intensity=0.15):
    """Applica overlay noise alla surface"""
    width, height = surface.get_size()
    noise_surface = pygame.Surface((width, height), pygame.SRCALPHA)
    
    for _ in range((width * height) // 10):
        x = random.randint(0, width - 1)
        y = random.randint(0, height - 1)
        alpha = int(255 * intensity)
        brightness = random.randint(-30, 30)
        color = (brightness, brightness, brightness, alpha)
        pygame.draw.circle(noise_surface, color, (x, y), 1)
    
    surface.blit(noise_surface, (0, 0), special_flags=pygame.BLEND_RGBA_ADD)


# ========== GENERATORE PRINCIPALE ==========

def generate_pattern_texture(colors, pattern_type, variation_config, section_shape, size):
    """
    Genera texture pattern completa
    
    Args:
        colors: lista colori da usare
        pattern_type: tipo pattern da PATTERN_DEFINITIONS
        variation_config: {'type': 'random_shading', 'intensity': 0.3}
        section_shape: 'center' o 'wedge'
        size: (width, height) della texture
    
    Returns:
        pygame.Surface con pattern generato
    """
    width, height = size
    surface = pygame.Surface((width, height), pygame.SRCALPHA)
    
    # 1. Genera mesh base colori
    generate_color_mesh(surface, colors, mesh_type='gradient')
    
    # 2. ADATTA SCALA PATTERN per forma wedge (trapezi sono più stretti/allungati)
    scale_factor = 1.0
    if section_shape == 'wedge':
        # Wedge sono più strette → usa elementi più piccoli per evitare compressione
        scale_factor = 0.6  # 60% delle dimensioni normali
    
    # 3. Applica pattern overlay (se non solid)
    if pattern_type != 'solid':
        # Crea surface trasparente per pattern
        pattern_surface = pygame.Surface((width, height), pygame.SRCALPHA)
        pattern_surface.fill((0, 0, 0, 0))
        
        # Disegna pattern con scala adattata
        if pattern_type == 'bricks':
            draw_bricks_pattern(pattern_surface, colors, variation_config, scale_factor)
        elif pattern_type == 'cobblestone':
            draw_cobblestone_pattern(pattern_surface, colors, variation_config, scale_factor)
        elif pattern_type == 'organic_scatter':
            draw_organic_scatter_pattern(pattern_surface, colors, variation_config, scale_factor)
        elif pattern_type == 'tiles':
            draw_tiles_pattern(pattern_surface, colors, variation_config, scale_factor)
        elif pattern_type == 'sand_ripples':
            draw_sand_ripples_pattern(pattern_surface, colors, variation_config, scale_factor)
        elif pattern_type == 'rocky':
            draw_rocky_pattern(pattern_surface, colors, variation_config, scale_factor)
        
        # Blend pattern su mesh base
        surface.blit(pattern_surface, (0, 0))
    
    # 4. Applica noise overlay se richiesto
    if variation_config['type'] in ['noise_overlay', 'combo']:
        apply_noise_overlay(surface, variation_config.get('intensity', 0.15))
    
    return surface


# ========== LOAD PATTERN INSTRUCTIONS (FUTURO) ==========

def load_pattern_instructions(filepath='pattern_instructions.txt'):
    """
    FUTURO: Carica istruzioni pattern da file
    
    Il file conterrÃ  istruzioni per nuovi pattern che l'utente vorrÃ  aggiungere.
    Formato esempio:
    
    [PATTERN: custom_pattern_01]
    name = Mio Pattern Custom
    description = Pattern custom con logica specifica
    algorithm = draw_custom_lines
    params = {
        'line_width': 2,
        'spacing': 8,
        'angle': 45
    }
    
    TODO: Implementare parser quando necessario
    """
    instructions_path = Path(filepath)
    
    if not instructions_path.exists():
        print(f"File istruzioni pattern non trovato: {filepath}")
        return {}
    
    # TODO: Parse file e aggiungi pattern a PATTERN_DEFINITIONS
    return {}
