import json
from pathlib import Path
from prop import Prop

class PropManager:
    """Gestisce tutti i props nella griglia
    
    MODIFICHE AVANZATE:
    - Salvataggio/caricamento world state completo (tiles + biomi + props + pattern)
    """
    
    def __init__(self, props_directory="props"):
        self.props = {}  # {(q, r, z, section_type, section_index): Prop}
        self.prop_definitions = {}  # {prop_id: Prop template}
        self.props_directory = props_directory
    
    def load_prop_definitions(self, directory_path=None):
        """Carica tutti i prop da file JSON nella cartella props/"""
        if directory_path is None:
            directory_path = self.props_directory
        
        props_path = Path(directory_path)
        if not props_path.exists():
            props_path.mkdir(parents=True, exist_ok=True)
            print(f"Creata cartella props: {props_path}")
            return
        
        loaded_count = 0
        for json_file in props_path.glob("*.json"):
            try:
                with open(json_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                prop = Prop.load_from_dict(data)
                self.prop_definitions[prop.prop_id] = prop
                loaded_count += 1
            except Exception as e:
                print(f"Errore caricando {json_file}: {e}")
        
        print(f"Caricati {loaded_count} prop definitions da {props_path}")
    
    def add_prop(self, q, r, z, section_type, section_index, prop_id):
        """Aggiunge un prop a una sezione
        
        Returns:
            Prop instance se aggiunto con successo, None altrimenti
        """
        if prop_id not in self.prop_definitions:
            print(f"Prop ID '{prop_id}' non trovato nelle definitions")
            return None
        
        # Crea una copia del prop template
        template = self.prop_definitions[prop_id]
        prop = Prop(
            prop_id=template.prop_id,
            name=template.name,
            section_type=section_type,
            section_index=section_index,
            visual=template.visual.copy() if template.visual else {},
            blocking=template.blocking,
            z_offset=template.z_offset,
            biome=template.biome,
            coloration=template.coloration.copy() if template.coloration else None
        )
        
        key = (q, r, z, section_type, section_index)
        self.props[key] = prop
        return prop
    
    def remove_prop(self, q, r, z, section_type, section_index):
        """Rimuove prop da una sezione
        
        Returns:
            Prop rimosso se esistente, None altrimenti
        """
        key = (q, r, z, section_type, section_index)
        return self.props.pop(key, None)
    
    def get_props_at_tile(self, q, r, z):
        """Ritorna tutti i props su una tile
        
        Returns:
            Lista di tuple (section_type, section_index, prop)
        """
        result = []
        for key, prop in self.props.items():
            kq, kr, kz, sec_type, sec_idx = key
            if (kq, kr, kz) == (q, r, z):
                result.append((sec_type, sec_idx, prop))
        return result
    
    def check_collision(self, q, r, z):
        """Controlla se ci sono props bloccanti a questa coordinata
        
        Returns:
            True se c'è almeno un prop bloccante, False altrimenti
        """
        props_here = self.get_props_at_tile(q, r, z)
        return any(prop.blocking for _, _, prop in props_here)
    
    def draw_props(self, screen, hex_grid, camera_x, camera_y, current_layer_z=None):
        """Disegna tutti i props del layer corrente (o tutti se current_layer_z=None)
        
        Args:
            screen: superficie pygame
            hex_grid: istanza di HexGrid
            camera_x, camera_y: offset camera
            current_layer_z: se specificato, disegna solo props a quel livello Z
        """
        for key, prop in self.props.items():
            q, r, z, section_type, section_index = key
            
            # Filtra per layer se specificato
            if current_layer_z is not None and z != current_layer_z:
                continue
            
            # Verifica che la tile esista
            if (q, r, z) not in hex_grid.tiles:
                continue
            
            tile = hex_grid.tiles[(q, r, z)]
            x, y = hex_grid.axial_to_pixel(q, r, z)
            
            screen_x = int(x + camera_x)
            screen_y = int(y + camera_y)
            
            prop.draw(screen, tile, screen_x, screen_y)
    
    def save_world_state(self, hex_grid, filepath="world_state.json"):
        """Salva stato completo del mondo: tiles (biomi, colori) + props (con pattern)
        
        NUOVO: Sistema di salvataggio avanzato
        
        Args:
            hex_grid: istanza di HexGrid con tiles
            filepath: percorso file di salvataggio
        """
        data = {
            'tiles': [],
            'props': []
        }
        
        # Salva tiles con biomi e colori delle sezioni
        for (q, r, z), tile in hex_grid.tiles.items():
            tile_data = {
                'coords': [q, r, z],
                'biome': tile.biome,
                'section_colors': tile.section_colors
            }
            data['tiles'].append(tile_data)
        
        # Salva props con offset e coloration
        for key, prop in self.props.items():
            q, r, z, sec_type, sec_idx = key
            prop_data = {
                'coords': [q, r, z],
                'section_type': sec_type,
                'section_index': sec_idx,
                'prop_id': prop.prop_id
            }
            
            # Aggiungi visual data (con offset)
            if prop.visual:
                prop_data['visual'] = prop.visual
            
            # Aggiungi coloration se presente
            if prop.coloration:
                prop_data['coloration'] = prop.coloration
            
            data['props'].append(prop_data)
        
        # Salva su file
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        
        print(f"\n✓ Salvato world state: {filepath}")
        print(f"   Tiles: {len(data['tiles'])}")
        print(f"   Props: {len(data['props'])}")
    
    def load_world_state(self, hex_grid, filepath="world_state.json"):
        """Carica stato completo del mondo
        
        NUOVO: Ripristina tiles con biomi, colori e pattern
        
        Args:
            hex_grid: istanza di HexGrid da popolare
            filepath: percorso file di caricamento
        """
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            # Pulisci stato corrente
            hex_grid.tiles.clear()
            self.props.clear()
            
            # Carica tiles
            from hex_tiles_main import HexTile
            for tile_data in data.get('tiles', []):
                q, r, z = tile_data['coords']
                biome = tile_data['biome']
                
                # Crea tile con bioma
                tile = HexTile(hex_grid.hex_size, biome, pointy_top=hex_grid.pointy_top)
                
                # Ripristina colori sezioni
                tile.section_colors = tile_data.get('section_colors', tile.section_colors)
                
                hex_grid.tiles[(q, r, z)] = tile
            
            # Carica props
            for prop_data in data.get('props', []):
                q, r, z = prop_data['coords']
                section_type = prop_data['section_type']
                section_index = prop_data.get('section_index')
                prop_id = prop_data['prop_id']
                
                # Aggiungi prop
                prop = self.add_prop(q, r, z, section_type, section_index, prop_id)
                
                if prop:
                    # Ripristina visual (con offset custom)
                    if 'visual' in prop_data:
                        prop.visual.update(prop_data['visual'])
                    
                    # Ripristina coloration
                    if 'coloration' in prop_data:
                        prop.coloration = prop_data['coloration']
                        
                        # IMPORTANTE: Riapplica pattern alla sezione
                        if (q, r, z) in hex_grid.tiles:
                            tile = hex_grid.tiles[(q, r, z)]
                            tile.set_section_pattern(
                                section_type,
                                section_index,
                                prop.coloration
                            )
            
            print(f"\n✓ Caricato world state: {filepath}")
            print(f"   Tiles: {len(data.get('tiles', []))}")
            print(f"   Props: {len(data.get('props', []))}")
            
        except FileNotFoundError:
            print(f"✗ File {filepath} non trovato")
        except Exception as e:
            print(f"✗ Errore caricando world state: {e}")
            import traceback
            traceback.print_exc()
    
    # ========== METODI LEGACY (compatibilità) ==========
    
    def save_all_props(self, filepath="props_placement.json"):
        """Salva tutti i props piazzati su file (LEGACY - usa save_world_state invece)"""
        data = []
        for key, prop in self.props.items():
            q, r, z, section_type, section_index = key
            data.append({
                'q': q,
                'r': r,
                'z': z,
                'section_type': section_type,
                'section_index': section_index,
                'prop_id': prop.prop_id
            })
        
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2)
        
        print(f"Salvati {len(data)} props in {filepath}")
    
    def load_all_props(self, filepath="props_placement.json"):
        """Carica tutti i props piazzati da file (LEGACY)"""
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            self.props.clear()
            for item in data:
                self.add_prop(
                    item['q'], item['r'], item['z'],
                    item['section_type'], item['section_index'],
                    item['prop_id']
                )
            
            print(f"Caricati {len(data)} props da {filepath}")
        except FileNotFoundError:
            print(f"File {filepath} non trovato")
        except Exception as e:
            print(f"Errore caricando props: {e}")
