import pygame
import math
from collections import defaultdict
from prop_manager import PropManager

class HexTile(pygame.Surface):
    """Surface esagonale con palette/descrizione e supporto 2.5D con 7 sezioni"""
    def __init__(self, size, palette_description, pointy_top=True, **kwargs):
        self.hex_size = size
        self.pointy_top = pointy_top
        
        # Aumentiamo l'altezza per includere la banda laterale
        self.band_height = int(size / 4)
        
        if pointy_top:
            width = int(size * math.sqrt(3))
            height = int(size * 2) + self.band_height
        else:
            width = int(size * 2)
            height = int(size * math.sqrt(3)) + self.band_height
        
        super().__init__((width, height), pygame.SRCALPHA, **kwargs)
        self.palette = palette_description
        
        # Fattore di scala per l'esagono centrale (60% = 0.60)
        self.center_scale = 0.60
        
        self.fill((0, 0, 0, 0))
    
    def get_hex_points_absolute(self, screen_x, screen_y, scale=1.0):
        """Ritorna i punti dell'esagono in coordinate assolute dello schermo
        
        Args:
            screen_x: coordinata x sullo schermo
            screen_y: coordinata y sullo schermo
            scale: fattore di scala (1.0 = dimensione normale, 0.35 = esagono centrale)
        """
        cx = screen_x + self.get_width() // 2
        cy = screen_y + self.hex_size
        size = self.hex_size * scale
        points = []
        
        for i in range(6):
            if self.pointy_top:
                angle = math.pi / 3 * i - math.pi / 6
            else:
                angle = math.pi / 3 * i
            
            x = cx + size * math.cos(angle)
            y = cy + size * math.sin(angle)
            points.append((x, y))
        
        return points
    
    def get_trapezoid_points(self, center_points, outer_points, section_index):
        """Calcola i 4 vertici del trapezio per una sezione radiale
        
        Args:
            center_points: vertici dell'esagono centrale
            outer_points: vertici dell'esagono esterno
            section_index: indice della sezione (0-5)
        
        Returns:
            Lista di 4 punti che formano il trapezio
        """
        # Indice del vertice successivo (con wrap-around)
        next_index = (section_index + 1) % 6
        
        # Trapezio formato da:
        # - Due vertici consecutivi dell'esagono centrale
        # - Due vertici consecutivi dell'esagono esterno
        trapezoid = [
            center_points[section_index],      # Vertice centro 1
            center_points[next_index],         # Vertice centro 2
            outer_points[next_index],          # Vertice esterno 2
            outer_points[section_index]        # Vertice esterno 1
        ]
        
        return trapezoid
    
    def draw_3d(self, screen, screen_x, screen_y):
        """Disegna il tile in modalità 2.5D con 7 sezioni: prima banda laterale, poi faccia top"""
        # 1. Disegna la banda laterale
        if self.band_height > 0:
            self._draw_band(screen, screen_x, screen_y)
        
        # 2. Disegna la faccia top suddivisa in 7 sezioni
        self._draw_top_face(screen, screen_x, screen_y)
    
    def _draw_top_face(self, screen, screen_x, screen_y):
        """Disegna la faccia top suddivisa in esagono centrale + 6 trapezi"""
        # Calcola vertici esagono esterno (100%)
        outer_points = self.get_hex_points_absolute(screen_x, screen_y, scale=1.0)
        
        # Calcola vertici esagono centrale (35%)
        center_points = self.get_hex_points_absolute(screen_x, screen_y, scale=self.center_scale)
        
        # Ottieni colori dalla palette
        center_color = self.palette.get('center', (100, 100, 100))
        section_colors = self.palette.get('sections', [(150, 150, 150)] * 6)
        
        # Disegna i 6 trapezi radiali
        for i in range(6):
            trapezoid = self.get_trapezoid_points(center_points, outer_points, i)
            section_color = section_colors[i] if i < len(section_colors) else (150, 150, 150)
            
            # Disegna il trapezio
            pygame.draw.polygon(screen, section_color, trapezoid)
            # Bordo nero per distinguere le sezioni
            pygame.draw.polygon(screen, (0, 0, 0), trapezoid, 1)
        
        # Disegna l'esagono centrale sopra i trapezi
        pygame.draw.polygon(screen, center_color, center_points)
        pygame.draw.polygon(screen, (0, 0, 0), center_points, 1)
        
        # Bordo esterno dell'intero esagono
        pygame.draw.polygon(screen, (0, 0, 0), outer_points, 2)
    
    def _draw_band(self, screen, screen_x, screen_y):
        """Disegna la banda laterale con colore più scuro (effetto ombra)
        Usa il colore medio delle sezioni per la banda"""
        # Calcola colore medio per la banda (usa le sections, non il centro)
        section_colors = self.palette.get('sections', [(150, 150, 150)] * 6)
        
        # Media dei colori delle sezioni visibili (quelle sul fronte)
        if len(section_colors) >= 3:
            # Usa le 3 sezioni frontali (0, 1, 2) per calcolare il colore medio
            avg_r = sum(c[0] for c in section_colors[0:3]) // 3
            avg_g = sum(c[1] for c in section_colors[0:3]) // 3
            avg_b = sum(c[2] for c in section_colors[0:3]) // 3
            base_color = (avg_r, avg_g, avg_b)
        else:
            base_color = section_colors[0] if section_colors else (150, 150, 150)
        
        hex_points = self.get_hex_points_absolute(screen_x, screen_y, scale=1.0)
        
        if self.pointy_top:
            # Lato SE (E-SE, vertici 0-1)
            dark_color1 = tuple(int(c * 0.70) for c in base_color)
            self._draw_band_face(screen, hex_points[0], hex_points[1], dark_color1)
            
            # Lato S (SE-SW, vertici 1-2)
            dark_color2 = tuple(int(c * 0.55) for c in base_color)
            self._draw_band_face(screen, hex_points[1], hex_points[2], dark_color2)
            
            # Lato SW (SW-W, vertici 2-3)
            dark_color3 = tuple(int(c * 0.70) for c in base_color)
            self._draw_band_face(screen, hex_points[2], hex_points[3], dark_color3)
    
    def _draw_band_face(self, screen, point1, point2, color):
        """Disegna una singola faccia della banda come quadrilatero"""
        p1_top = point1
        p2_top = point2
        p1_bottom = (point1[0], point1[1] + self.band_height)
        p2_bottom = (point2[0], point2[1] + self.band_height)
        
        quad = [p1_top, p2_top, p2_bottom, p1_bottom]
        pygame.draw.polygon(screen, color, quad)
        pygame.draw.polygon(screen, (0, 0, 0), quad, 1)


class Camera:
    """Gestisce la camera con transizioni smooth"""
    def __init__(self, screen_width, screen_height, transition_time=0.25):
        self.screen_width = screen_width
        self.screen_height = screen_height
        self.transition_time = transition_time
        
        self.current_x = 0.0
        self.current_y = 0.0
        self.target_x = 0.0
        self.target_y = 0.0
        
        self.transition_progress = 1.0
    
    def set_target(self, target_x, target_y):
        """Imposta il nuovo target per la camera"""
        self.target_x = target_x
        self.target_y = target_y
        self.transition_progress = 0.0
    
    def update(self, dt):
        """Aggiorna la posizione della camera con interpolazione smooth"""
        if self.transition_progress < 1.0:
            self.transition_progress += dt / self.transition_time
            self.transition_progress = min(1.0, self.transition_progress)
            
            self.current_x += (self.target_x - self.current_x) * min(1.0, dt / self.transition_time * 4)
            self.current_y += (self.target_y - self.current_y) * min(1.0, dt / self.transition_time * 4)
    
    def get_offset(self):
        """Ritorna l'offset corrente della camera"""
        return self.current_x, self.current_y


class HexGrid:
    """Gestisce griglia esagonale 2.5D con sistema a LAYER basati su Z"""
    
    DIRECTIONS_POINTY = [
        (1, 0),   # E (Est)
        (1, -1),  # NE (Nord-Est)
        (0, -1),  # NW (Nord-Ovest)
        (-1, 0),  # W (Ovest)
        (-1, 1),  # SW (Sud-Ovest)
        (0, 1)    # SE (Sud-Est)
    ]
    
    def __init__(self, hex_size, palettes, pointy_top=True):
        self.hex_size = hex_size
        self.palettes = palettes
        self.pointy_top = pointy_top
        self.tiles = {}
        self.current_pos = (0, 0, 0)
    
        # NUOVO: Aggiungi PropManager
        self.prop_manager = PropManager()
        self.prop_manager.load_prop_definitions()
    
        # NUOVO: Sezione correntemente selezionata per piazzare props
        self.selected_section_type = "center"
        self.selected_section_index = None
    
        self.DIRECTIONS = self.DIRECTIONS_POINTY
     
        self.generate_tile(0, 0, 0)
    
    def axial_to_pixel(self, q, r, z):
        """Converte coordinate axial 3D (q, r, z) in coordinate pixel dello schermo"""
        if self.pointy_top:
            x = self.hex_size * math.sqrt(3) * (q + r/2)
            y_base = self.hex_size * 3/2 * r
        else:
            x = self.hex_size * 3/2 * q
            y_base = self.hex_size * math.sqrt(3) * (r + q/2)
        
        # Offset verticale per l'elevazione
        y = y_base - (z * self.hex_size / 4)
        
        return x, y
    
    def generate_tile(self, q, r, z):
        """Genera una tile alle coordinate (q, r, z) se non esiste già
        MODIFICATO: Controlla collisioni con props bloccanti"""
        if (q, r, z) in self.tiles:
            return self.tiles[(q, r, z)]
    
        # NUOVO: Controlla se ci sono props bloccanti
        if self.prop_manager.check_collision(q, r, z):
            # Non generare tile se c'è un prop bloccante
            return None
    
        palette_index = (abs(q) + abs(r) + abs(z)) % len(self.palettes)
        palette = self.palettes[palette_index]
    
        tile = HexTile(self.hex_size, palette, pointy_top=self.pointy_top)
    
        self.tiles[(q, r, z)] = tile
        return tile
    
    def get_topmost_accessible_tile(self, q, r, z_start):
        """Trova la tile più alta accessibile sotto i props bloccanti"""
        z = z_start
        min_z = -10  # Limite inferiore arbitrario
    
        while self.prop_manager.check_collision(q, r, z) and z > min_z:
            z -= 1
    
        if z <= min_z:
            return None
    
        return (q, r, z)
    
    def place_prop(self, prop_id):
        """Piazza un prop sulla sezione correntemente selezionata"""
        q, r, z = self.current_pos
        
        result = self.prop_manager.add_prop(
            q, r, z,
            self.selected_section_type,
            self.selected_section_index,
            prop_id
        )
        
        if result:
            print(f"✓ Piazzato {prop_id} su ({q},{r},{z}) - {self.selected_section_type} {self.selected_section_index}")
        else:
            print(f"✗ Impossibile piazzare {prop_id}")
        
        return result
    
    def remove_prop(self):
        """Rimuove prop dalla sezione correntemente selezionata"""
        q, r, z = self.current_pos
        
        removed = self.prop_manager.remove_prop(
            q, r, z,
            self.selected_section_type,
            self.selected_section_index
        )
        
        if removed:
            print(f"✓ Rimosso prop da ({q},{r},{z}) - {self.selected_section_type} {self.selected_section_index}")
        else:
            print(f"✗ Nessun prop da rimuovere")
        
        return removed
    
    def cycle_section(self):
        """Cicla tra le 7 sezioni (center + 6 wedges)"""
        if self.selected_section_type == "center":
            self.selected_section_type = "wedge"
            self.selected_section_index = 0
        elif self.selected_section_type == "wedge":
            if self.selected_section_index < 5:
                self.selected_section_index += 1
            else:
                self.selected_section_type = "center"
                self.selected_section_index = None
        
        section_name = self.selected_section_type
        if self.selected_section_index is not None:
            section_name += f" {self.selected_section_index}"
        print(f"Sezione selezionata: {section_name}")
    
    def _highlight_selected_section(self, screen, tile, screen_x, screen_y):
        """Evidenzia la sezione correntemente selezionata"""
        if self.selected_section_type == "center":
            center_points = tile.get_hex_points_absolute(screen_x, screen_y, 
                                                         scale=tile.center_scale)
            pygame.draw.polygon(screen, (0, 255, 255), center_points, 3)
        
        elif self.selected_section_type == "wedge" and self.selected_section_index is not None:
            outer_points = tile.get_hex_points_absolute(screen_x, screen_y, scale=1.0)
            center_points = tile.get_hex_points_absolute(screen_x, screen_y, 
                                                         scale=tile.center_scale)
            trapezoid = tile.get_trapezoid_points(center_points, outer_points, 
                                                  self.selected_section_index)
            pygame.draw.polygon(screen, (0, 255, 255), trapezoid, 3)
    
    def get_neighbor(self, q, r, direction_index):
        """Ottiene coordinate del vicino orizzontale nella direzione specificata"""
        dq, dr = self.DIRECTIONS[direction_index]
        return (q + dq, r + dr)
    
    def move_horizontal(self, direction_index):
        """Muove la posizione corrente in orizzontale (mantiene stesso z)"""
        q, r, z = self.current_pos
        new_q, new_r = self.get_neighbor(q, r, direction_index)
        self.current_pos = (new_q, new_r, z)
        self.generate_tile(new_q, new_r, z)
        return self.current_pos
    
    def move_up(self):
        """Muove verso l'alto (z+1) - crea tile sopra quella corrente"""
        q, r, z = self.current_pos
        new_z = z + 1
        self.current_pos = (q, r, new_z)
        self.generate_tile(q, r, new_z)
        return self.current_pos
    
    def move_down(self):
        """Muove verso il basso (z-1) - cancella tile corrente dopo lo spostamento"""
        q, r, z = self.current_pos
        new_z = z - 1
        
        self.generate_tile(q, r, new_z)
        
        if (q, r, z) in self.tiles:
            del self.tiles[(q, r, z)]
        
        self.current_pos = (q, r, new_z)
        return self.current_pos
    
    def generate_neighbors(self, q, r, z):
        """Genera tutte le tiles vicine a (q, r, z) allo stesso livello z"""
        for i in range(6):
            nq, nr = self.get_neighbor(q, r, i)
            self.generate_tile(nq, nr, z)
    
    def get_camera_target(self, screen_width, screen_height):
        """Calcola la posizione target della camera per centrare l'esagono corrente"""
        q, r, z = self.current_pos
        hex_x, hex_y = self.axial_to_pixel(q, r, z)
        
        camera_x = screen_width // 2 - hex_x
        camera_y = screen_height // 2 - hex_y
        
        return camera_x, camera_y
    
    def draw(self, screen, camera_x=0, camera_y=0):
        """Disegna tiles e props con sistema a LAYER"""
        layers = defaultdict(list)
        for (q, r, z) in self.tiles.keys():
            layers[z].append((q, r, z))
    
        sorted_z_levels = sorted(layers.keys())
    
        for z_level in sorted_z_levels:
            tiles_in_layer = sorted(layers[z_level], 
                                key=lambda pos: (pos[1], pos[0]))
        
            # Disegna tiles del layer
            for (q, r, z) in tiles_in_layer:
                tile = self.tiles[(q, r, z)]
                x, y = self.axial_to_pixel(q, r, z)
            
                screen_x = int(x + camera_x)
                screen_y = int(y + camera_y)
            
                tile.draw_3d(screen, screen_x, screen_y)
            
                # Evidenzia tile corrente
                if (q, r, z) == self.current_pos:
                    points = tile.get_hex_points_absolute(screen_x, screen_y, scale=1.0)
                    pygame.draw.polygon(screen, (255, 255, 0), points, 4)
                
                    # NUOVO: Evidenzia sezione selezionata
                    self._highlight_selected_section(screen, tile, screen_x, screen_y)
        
            # NUOVO: Disegna props del layer
            self.prop_manager.draw_props(screen, self, camera_x, camera_y, z_level)


# ========== FUNZIONE HELPER ==========
def _draw_prop_menu(screen, available_props, selected_index, prop_definitions, font):
    """Disegna il menu di selezione props"""
    menu_width = 400
    menu_height = min(400, len(available_props) * 30 + 60)
    menu_x = (screen.get_width() - menu_width) // 2
    menu_y = (screen.get_height() - menu_height) // 2
    
    # Background
    menu_rect = pygame.Rect(menu_x, menu_y, menu_width, menu_height)
    pygame.draw.rect(screen, (50, 50, 50), menu_rect)
    pygame.draw.rect(screen, (255, 255, 255), menu_rect, 3)
    
    # Titolo
    title = font.render("SELEZIONA PROP", True, (255, 255, 255))
    screen.blit(title, (menu_x + 20, menu_y + 15))
    
    # Lista props
    y = menu_y + 50
    visible_start = max(0, selected_index - 10)
    visible_end = min(len(available_props), visible_start + 12)
    
    for i in range(visible_start, visible_end):
        prop_id = available_props[i]
        prop = prop_definitions.get(prop_id)
        
        if i == selected_index:
            # Evidenzia selezione
            highlight_rect = pygame.Rect(menu_x + 10, y - 2, menu_width - 20, 26)
            pygame.draw.rect(screen, (100, 100, 255), highlight_rect)
        
        # Testo
        if prop:
            text = f"{prop_id} - {prop.name}"
            color = (255, 255, 0) if i == selected_index else (200, 200, 200)
        else:
            text = f"{prop_id} (errore)"
            color = (255, 100, 100)
        
        surf = font.render(text, True, color)
        screen.blit(surf, (menu_x + 20, y))
        
        y += 30
    
    # Footer
    footer = font.render("↑/↓: Naviga | ENTER: Seleziona | ESC: Annulla", True, (180, 180, 180))
    screen.blit(footer, (menu_x + 20, menu_y + menu_height - 35))


# ========== MAIN ==========
def main():
    pygame.init()
    screen = pygame.display.set_mode((800, 600))
    pygame.display.set_caption("Hex Tiles - 2.5D con 7 Sezioni + Props")
    clock = pygame.time.Clock()
    
    palettes = [
        {
            'name': 'Grass Mixed',
            'center': (34, 139, 34),
            'sections': [
                (50, 155, 50),
                (34, 139, 34),
                (20, 120, 20),
                (40, 150, 40),
                (30, 130, 30),
                (45, 145, 45)
            ]
        },
        {
            'name': 'Forest Varied',
            'center': (0, 100, 0),
            'sections': [
                (10, 110, 10),
                (0, 90, 0),
                (5, 105, 5),
                (0, 95, 0),
                (15, 115, 15),
                (0, 100, 0)
            ]
        },
        {
            'name': 'Water Deep',
            'center': (30, 144, 255),
            'sections': [
                (40, 154, 255),
                (25, 139, 245),
                (35, 149, 250),
                (30, 144, 255),
                (20, 134, 240),
                (45, 159, 255)
            ]
        },
        {
            'name': 'Mountain Rocky',
            'center': (139, 137, 137),
            'sections': [
                (149, 147, 147),
                (129, 127, 127),
                (144, 142, 142),
                (134, 132, 132),
                (154, 152, 152),
                (139, 137, 137)
            ]
        },
        {
            'name': 'Sand Beach',
            'center': (238, 214, 175),
            'sections': [
                (248, 224, 185),
                (228, 204, 165),
                (243, 219, 180),
                (233, 209, 170),
                (253, 229, 190),
                (238, 214, 175)
            ]
        },
        {
            'name': 'Mixed Terrain',
            'center': (100, 200, 100),
            'sections': [
                (150, 150, 50),
                (200, 100, 100),
                (100, 100, 200),
                (150, 200, 150),
                (200, 200, 100),
                (100, 150, 200)
            ]
        }
    ]
    
    hex_grid = HexGrid(120, palettes, pointy_top=True)
    camera = Camera(screen.get_width(), screen.get_height(), transition_time=0.25)
    
    initial_target = hex_grid.get_camera_target(screen.get_width(), screen.get_height())
    camera.current_x, camera.current_y = initial_target
    camera.target_x, camera.target_y = initial_target
    
    font = pygame.font.Font(None, 22)
    
    # NUOVO: Menu props
    show_prop_menu = False
    available_props = list(hex_grid.prop_manager.prop_definitions.keys())
    selected_prop_index = 0
    
    running = True
    dt = 0
    
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    if show_prop_menu:
                        show_prop_menu = False
                    else:
                        running = False
                
                # NUOVO: Gestione menu props
                elif show_prop_menu:
                    if event.key == pygame.K_UP:
                        selected_prop_index = (selected_prop_index - 1) % len(available_props)
                    elif event.key == pygame.K_DOWN:
                        selected_prop_index = (selected_prop_index + 1) % len(available_props)
                    elif event.key == pygame.K_RETURN:
                        if available_props:
                            prop_id = available_props[selected_prop_index]
                            hex_grid.place_prop(prop_id)
                        show_prop_menu = False
                
                # Controlli normali (quando menu non è aperto)
                elif not show_prop_menu:
                    # Movimento orizzontale
                    if event.key == pygame.K_q:  # NW
                        hex_grid.move_horizontal(2)
                        target = hex_grid.get_camera_target(screen.get_width(), screen.get_height())
                        camera.set_target(*target)
                    elif event.key == pygame.K_e:  # NE
                        hex_grid.move_horizontal(1)
                        target = hex_grid.get_camera_target(screen.get_width(), screen.get_height())
                        camera.set_target(*target)
                    elif event.key == pygame.K_a:  # W
                        hex_grid.move_horizontal(3)
                        target = hex_grid.get_camera_target(screen.get_width(), screen.get_height())
                        camera.set_target(*target)
                    elif event.key == pygame.K_d:  # E
                        hex_grid.move_horizontal(0)
                        target = hex_grid.get_camera_target(screen.get_width(), screen.get_height())
                        camera.set_target(*target)
                    elif event.key == pygame.K_s:  # SW
                        hex_grid.move_horizontal(4)
                        target = hex_grid.get_camera_target(screen.get_width(), screen.get_height())
                        camera.set_target(*target)
                    elif event.key == pygame.K_x:  # SE
                        hex_grid.move_horizontal(5)
                        target = hex_grid.get_camera_target(screen.get_width(), screen.get_height())
                        camera.set_target(*target)
                    
                    # Movimento verticale
                    elif event.key == pygame.K_w:  # Su (z+1)
                        hex_grid.move_up()
                        target = hex_grid.get_camera_target(screen.get_width(), screen.get_height())
                        camera.set_target(*target)
                    
                    elif event.key == pygame.K_z:  # Giù (z-1)
                        hex_grid.move_down()
                        target = hex_grid.get_camera_target(screen.get_width(), screen.get_height())
                        camera.set_target(*target)
                    
                    # Genera vicini
                    elif event.key == pygame.K_SPACE:
                        q, r, z = hex_grid.current_pos
                        
                        # MODIFICATO: Controlla collisioni prima di generare
                        accessible = hex_grid.get_topmost_accessible_tile(q, r, z)
                        if accessible and accessible != hex_grid.current_pos:
                            hex_grid.current_pos = accessible
                            print(f"Spostato a tile accessibile: {accessible}")
                            target = hex_grid.get_camera_target(screen.get_width(), screen.get_height())
                            camera.set_target(*target)
                        
                        hex_grid.generate_neighbors(q, r, z)
                    
                    # NUOVO: Controlli props
                    elif event.key == pygame.K_p:  # Piazza prop
                        if available_props:
                            show_prop_menu = True
                            print("Menu props aperto - Usa ↑/↓ per selezionare, ENTER per piazzare")
                        else:
                            print("Nessun prop disponibile! Usa prop_editor.py per crearli")
                    
                    elif event.key == pygame.K_r:  # Rimuovi prop
                        hex_grid.remove_prop()
                    
                    elif event.key == pygame.K_TAB:  # Cicla sezioni
                        hex_grid.cycle_section()
                    
                    # NUOVO: Salva/Carica props
                    elif event.key == pygame.K_F5:  # Salva
                        hex_grid.prop_manager.save_all_props()
                    
                    elif event.key == pygame.K_F9:  # Carica
                        hex_grid.prop_manager.load_all_props()
        
        camera.update(dt)
        camera_x, camera_y = camera.get_offset()
        
        screen.fill((200, 200, 200))
        hex_grid.draw(screen, camera_x, camera_y)
        
        # MODIFICATO: Istruzioni con nuovi controlli
        instructions = [
            "=== HEX TILES - 7 SEZIONI + PROPS ===",
            "Q: NW  E: NE",
            "A: W   D: E",
            "S: SW  X: SE",
            "",
            "W: Sali layer (z+1)",
            "Z: Scendi layer (z-1)",
            "",
            "SPACE: Genera vicini",
            "TAB: Cicla sezioni (center/wedges)",
            "P: Piazza prop",
            "R: Rimuovi prop",
            "",
            "F5: Salva props | F9: Carica props",
            "ESC: Esci",
            "",
            f"Tiles: {len(hex_grid.tiles)} | Props: {len(hex_grid.prop_manager.props)}",
            f"Pos (q,r,z): {hex_grid.current_pos}",
            f"Layers: {len(set(z for q,r,z in hex_grid.tiles.keys()))}",
            f"Sezione: {hex_grid.selected_section_type} {hex_grid.selected_section_index or ''}"
        ]
        
        for i, text in enumerate(instructions):
            surf = font.render(text, True, (0, 0, 0))
            screen.blit(surf, (10, 10 + i * 22))
        
        # NUOVO: Disegna menu props se aperto
        if show_prop_menu:
            _draw_prop_menu(screen, available_props, selected_prop_index, 
                          hex_grid.prop_manager.prop_definitions, font)
        
        pygame.display.flip()
        dt = clock.tick(120) / 1000.0
    
    pygame.quit()


if __name__ == "__main__":
    main()
    