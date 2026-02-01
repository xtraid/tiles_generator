import pygame
import json
from pathlib import Path

class Prop:
    """Rappresenta un oggetto posizionabile su una sezione specifica di un hex tile"""
    
    def __init__(self, prop_id, name, section_type, section_index=None, 
                 visual=None, blocking=True, z_offset=0.0):
        """
        Args:
            prop_id: identificatore univoco del tipo di prop
            name: nome descrittivo (es. "albero", "roccia", "casa")
            section_type: "center", "wedge", o "any"
            section_index: None per center, 0-5 per wedge
            visual: dict con info visuali {'type': 'circle', 'color': [r,g,b], ...}
            blocking: se True, blocca movimento/generazione tiles
            z_offset: offset verticale per rendering (0.0 = base tile)
        """
        self.prop_id = prop_id
        self.name = name
        self.section_type = section_type
        self.section_index = section_index
        self.visual = visual or {'type': 'circle', 'color': [100, 100, 100], 'radius': 10}
        self.blocking = blocking
        self.z_offset = z_offset
    
    def draw(self, screen, hex_tile, screen_x, screen_y):
        """Disegna il prop sulla sezione corretta dell'esagono
        
        Args:
            screen: superficie pygame su cui disegnare
            hex_tile: istanza di HexTile
            screen_x, screen_y: coordinate dello schermo dove si trova il tile
        """
        # Calcola il centro della sezione (trapezio o esagono centrale)
        cx, cy = self._get_section_center(hex_tile, screen_x, screen_y)
        
        # Applica z_offset
        cy -= int(self.z_offset * hex_tile.hex_size)
        
        # ====== SUPPORTO IMMAGINI ======
        if self.visual['type'] == 'image':
            image_path = self.visual.get('path', '')
            try:
                # Carica immagine dalla cartella assets/
                image = pygame.image.load(image_path)
                
                # Scala immagine se specificato
                scale = self.visual.get('scale', 1.0)
                if scale != 1.0:
                    new_size = (int(image.get_width() * scale), 
                               int(image.get_height() * scale))
                    image = pygame.transform.scale(image, new_size)
                
                # IMPORTANTE: Centra sempre sulla sezione (trapezio o centro)
                rect = image.get_rect(center=(int(cx), int(cy)))
                screen.blit(image, rect)
                
                # Bordo rosso se bloccante
                if self.blocking:
                    pygame.draw.rect(screen, (255, 0, 0), rect.inflate(6, 6), 2)
            
            except Exception as e:
                # Fallback: cerchio rosso se immagine non trovata
                pygame.draw.circle(screen, (255, 0, 0), (int(cx), int(cy)), 15)
                pygame.draw.line(screen, (255, 255, 255), 
                               (int(cx)-10, int(cy)-10), 
                               (int(cx)+10, int(cy)+10), 2)
                pygame.draw.line(screen, (255, 255, 255), 
                               (int(cx)-10, int(cy)+10), 
                               (int(cx)+10, int(cy)-10), 2)
        
        # ====== FORME GEOMETRICHE ======
        elif self.visual['type'] == 'circle':
            color = tuple(self.visual.get('color', [100, 100, 100]))
            radius = self.visual.get('radius', 10)
            pygame.draw.circle(screen, color, (int(cx), int(cy)), radius)
            pygame.draw.circle(screen, (0, 0, 0), (int(cx), int(cy)), radius, 2)
            
            if self.blocking:
                pygame.draw.circle(screen, (255, 0, 0), (int(cx), int(cy)), radius + 3, 2)
        
        elif self.visual['type'] == 'rect':
            color = tuple(self.visual.get('color', [100, 100, 100]))
            width = self.visual.get('width', 20)
            height = self.visual.get('height', 20)
            rect = pygame.Rect(int(cx - width/2), int(cy - height/2), width, height)
            pygame.draw.rect(screen, color, rect)
            pygame.draw.rect(screen, (0, 0, 0), rect, 2)
            
            if self.blocking:
                pygame.draw.rect(screen, (255, 0, 0), rect.inflate(6, 6), 2)
        
        elif self.visual['type'] == 'triangle':
            color = tuple(self.visual.get('color', [100, 100, 100]))
            size = self.visual.get('size', 15)
            points = [
                (cx, cy - size),
                (cx - size, cy + size),
                (cx + size, cy + size)
            ]
            pygame.draw.polygon(screen, color, points)
            pygame.draw.polygon(screen, (0, 0, 0), points, 2)
            
            if self.blocking:
                offset_points = [
                    (cx, cy - size - 3),
                    (cx - size - 3, cy + size + 3),
                    (cx + size + 3, cy + size + 3)
                ]
                pygame.draw.polygon(screen, (255, 0, 0), offset_points, 2)
    
    def _get_section_center(self, hex_tile, screen_x, screen_y):
        """Calcola il centro della sezione su cui è posizionato il prop"""
        if self.section_type == "center":
            # Centro dell'esagono centrale
            cx = screen_x + hex_tile.get_width() // 2
            cy = screen_y + hex_tile.hex_size
            return cx, cy
        
        elif self.section_type == "wedge" and self.section_index is not None:
            import math
            
            # Centro del tile
            tile_center_x = screen_x + hex_tile.get_width() // 2
            tile_center_y = screen_y + hex_tile.hex_size
            
            # IMPORTANTE: Angolo della wedge deve corrispondere ai vertici!
            # Nel codice HexTile i vertici usano: angle = pi/3 * i - pi/6
            # I trapezi sono "tra" i vertici, quindi il centro del trapezio
            # è a metà tra due vertici consecutivi
            if hex_tile.pointy_top:
                # Angolo del vertice START del trapezio
                angle_start = math.pi / 3 * self.section_index - math.pi / 6
                # Angolo del vertice END del trapezio
                angle_end = math.pi / 3 * (self.section_index + 1) - math.pi / 6
                # Centro del trapezio = media dei due angoli
                angle = (angle_start + angle_end) / 2
            else:
                angle = math.pi / 3 * self.section_index
            
            # Raggi: centro esagono piccolo e bordo esterno
            inner_radius = hex_tile.hex_size * hex_tile.center_scale  # 0.60
            outer_radius = hex_tile.hex_size  # 1.0
            
            # Centro del trapezio = metà strada tra inner e outer
            prop_radius = (inner_radius + outer_radius) *0.45
            
            # Posizione
            cx = tile_center_x + prop_radius * math.cos(angle)
            cy = tile_center_y + prop_radius * math.sin(angle)
            
            return cx, cy
        
        # Fallback: centro tile
        return screen_x + hex_tile.get_width() // 2, screen_y + hex_tile.hex_size
    
    def get_collision_bounds(self):
        """Ritorna area di collisione (per ora semplice)"""
        if self.visual['type'] == 'circle':
            radius = self.visual.get('radius', 10)
            return {'type': 'circle', 'radius': radius}
        return {'type': 'point'}
    
    def save_to_dict(self):
        """Serializza per salvare su file"""
        return {
            'prop_id': self.prop_id,
            'name': self.name,
            'section_type': self.section_type,
            'section_index': self.section_index,
            'visual': self.visual,
            'blocking': self.blocking,
            'z_offset': self.z_offset
        }
    
    @staticmethod
    def load_from_dict(data):
        """Deserializza da file"""
        return Prop(
            prop_id=data['prop_id'],
            name=data['name'],
            section_type=data['section_type'],
            section_index=data.get('section_index'),
            visual=data.get('visual'),
            blocking=data.get('blocking', True),
            z_offset=data.get('z_offset', 0.0)
        )
    
    def save_to_file(self, directory):
        """Salva il prop come file JSON"""
        Path(directory).mkdir(parents=True, exist_ok=True)
        filepath = Path(directory) / f"{self.prop_id}.json"
        
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(self.save_to_dict(), f, indent=2, ensure_ascii=False)
        
        return filepath