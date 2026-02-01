import pygame
import sys
from pathlib import Path
from prop import Prop

class PropEditor:
    """Editor grafico per creare e modificare props"""
    
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((1000, 700))
        pygame.display.set_caption("Prop Editor")
        self.clock = pygame.time.Clock()
        
        self.font = pygame.font.Font(None, 24)
        self.font_small = pygame.font.Font(None, 20)
        
        # Props directory
        self.props_dir = Path("props")
        self.props_dir.mkdir(exist_ok=True)
        
        # Stato editor
        self.current_prop = self._create_default_prop()
        self.prop_list = self._load_all_props()
        
        # UI state
        self.selected_field = 0
        self.fields = ['prop_id', 'name', 'section_type', 'section_index', 
                      'visual_type', 'color_r', 'color_g', 'color_b', 
                      'size_param', 'blocking', 'z_offset']
        
        self.section_types = ['center', 'wedge']
        self.visual_types = ['circle', 'rect', 'triangle']
        
        self.input_text = ""
        self.editing_mode = False
        
        # Preview hex tile (mock)
        from hex_tiles_main import HexTile
        self.preview_tile = HexTile(80, {
            'center': (150, 150, 150),
            'sections': [(180, 180, 180)] * 6
        }, pointy_top=True)
    
    def _create_default_prop(self):
        """Crea un prop di default"""
        return Prop(
            prop_id="new_prop",
            name="Nuovo Prop",
            section_type="center",
            section_index=None,
            visual={'type': 'circle', 'color': [100, 200, 100], 'radius': 15},
            blocking=True,
            z_offset=0.0
        )
    
    def _load_all_props(self):
        """Carica tutti i props dalla cartella"""
        props = []
        for json_file in self.props_dir.glob("*.json"):
            try:
                import json
                with open(json_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                prop = Prop.load_from_dict(data)
                props.append(prop)
            except Exception as e:
                print(f"Errore caricando {json_file}: {e}")
        return props
    
    def run(self):
        """Main loop dell'editor"""
        running = True
        
        while running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        running = False
                    
                    elif event.key == pygame.K_UP:
                        self.selected_field = (self.selected_field - 1) % len(self.fields)
                        self.editing_mode = False
                    
                    elif event.key == pygame.K_DOWN:
                        self.selected_field = (self.selected_field + 1) % len(self.fields)
                        self.editing_mode = False
                    
                    elif event.key == pygame.K_RETURN:
                        if self.editing_mode:
                            self._apply_input()
                            self.editing_mode = False
                        else:
                            self.editing_mode = True
                            self._prepare_input()
                    
                    elif event.key == pygame.K_LEFT:
                        self._decrease_value()
                    
                    elif event.key == pygame.K_RIGHT:
                        self._increase_value()
                    
                    elif event.key == pygame.K_s and pygame.key.get_mods() & pygame.KMOD_CTRL:
                        self._save_current_prop()
                    
                    elif event.key == pygame.K_n and pygame.key.get_mods() & pygame.KMOD_CTRL:
                        self.current_prop = self._create_default_prop()
                    
                    elif self.editing_mode:
                        if event.key == pygame.K_BACKSPACE:
                            self.input_text = self.input_text[:-1]
                        else:
                            self.input_text += event.unicode
            
            self._draw()
            pygame.display.flip()
            self.clock.tick(60)
        
        pygame.quit()
    
    def _prepare_input(self):
        """Prepara l'input text con il valore corrente"""
        field = self.fields[self.selected_field]
        
        if field == 'prop_id':
            self.input_text = self.current_prop.prop_id
        elif field == 'name':
            self.input_text = self.current_prop.name
        elif field == 'section_index':
            self.input_text = str(self.current_prop.section_index) if self.current_prop.section_index is not None else ""
        elif field == 'color_r':
            self.input_text = str(self.current_prop.visual.get('color', [0,0,0])[0])
        elif field == 'color_g':
            self.input_text = str(self.current_prop.visual.get('color', [0,0,0])[1])
        elif field == 'color_b':
            self.input_text = str(self.current_prop.visual.get('color', [0,0,0])[2])
        elif field == 'size_param':
            if self.current_prop.visual['type'] == 'circle':
                self.input_text = str(self.current_prop.visual.get('radius', 10))
            else:
                self.input_text = str(self.current_prop.visual.get('size', 15))
        elif field == 'z_offset':
            self.input_text = str(self.current_prop.z_offset)
    
    def _apply_input(self):
        """Applica l'input text al campo corrente"""
        field = self.fields[self.selected_field]
        
        try:
            if field == 'prop_id':
                self.current_prop.prop_id = self.input_text
            elif field == 'name':
                self.current_prop.name = self.input_text
            elif field == 'section_index':
                if self.input_text.strip():
                    self.current_prop.section_index = int(self.input_text)
                else:
                    self.current_prop.section_index = None
            elif field in ['color_r', 'color_g', 'color_b']:
                color = list(self.current_prop.visual.get('color', [0,0,0]))
                idx = ['color_r', 'color_g', 'color_b'].index(field)
                color[idx] = max(0, min(255, int(self.input_text)))
                self.current_prop.visual['color'] = color
            elif field == 'size_param':
                val = max(1, int(self.input_text))
                if self.current_prop.visual['type'] == 'circle':
                    self.current_prop.visual['radius'] = val
                elif self.current_prop.visual['type'] == 'rect':
                    self.current_prop.visual['width'] = val
                    self.current_prop.visual['height'] = val
                else:
                    self.current_prop.visual['size'] = val
            elif field == 'z_offset':
                self.current_prop.z_offset = float(self.input_text)
        except ValueError:
            pass
        
        self.input_text = ""
    
    def _increase_value(self):
        """Aumenta il valore del campo corrente"""
        field = self.fields[self.selected_field]
        
        if field == 'section_type':
            idx = self.section_types.index(self.current_prop.section_type)
            idx = (idx + 1) % len(self.section_types)
            self.current_prop.section_type = self.section_types[idx]
            if self.current_prop.section_type == 'center':
                self.current_prop.section_index = None
            else:
                self.current_prop.section_index = 0
        
        elif field == 'section_index' and self.current_prop.section_type == 'wedge':
            if self.current_prop.section_index is None:
                self.current_prop.section_index = 0
            else:
                self.current_prop.section_index = (self.current_prop.section_index + 1) % 6
        
        elif field == 'visual_type':
            idx = self.visual_types.index(self.current_prop.visual['type'])
            idx = (idx + 1) % len(self.visual_types)
            self.current_prop.visual['type'] = self.visual_types[idx]
        
        elif field == 'blocking':
            self.current_prop.blocking = not self.current_prop.blocking
    
    def _decrease_value(self):
        """Diminuisce il valore del campo corrente"""
        field = self.fields[self.selected_field]
        
        if field == 'section_type':
            idx = self.section_types.index(self.current_prop.section_type)
            idx = (idx - 1) % len(self.section_types)
            self.current_prop.section_type = self.section_types[idx]
            if self.current_prop.section_type == 'center':
                self.current_prop.section_index = None
            else:
                self.current_prop.section_index = 0
        
        elif field == 'section_index' and self.current_prop.section_type == 'wedge':
            if self.current_prop.section_index is None:
                self.current_prop.section_index = 5
            else:
                self.current_prop.section_index = (self.current_prop.section_index - 1) % 6
        
        elif field == 'visual_type':
            idx = self.visual_types.index(self.current_prop.visual['type'])
            idx = (idx - 1) % len(self.visual_types)
            self.current_prop.visual['type'] = self.visual_types[idx]
        
        elif field == 'blocking':
            self.current_prop.blocking = not self.current_prop.blocking
    
    def _save_current_prop(self):
        """Salva il prop corrente"""
        try:
            filepath = self.current_prop.save_to_file(self.props_dir)
            print(f"✓ Prop salvato: {filepath}")
            self.prop_list = self._load_all_props()
        except Exception as e:
            print(f"✗ Errore salvando prop: {e}")
    
    def _draw(self):
        """Disegna l'interfaccia dell'editor"""
        self.screen.fill((40, 40, 40))
        
        # Titolo
        title = self.font.render("PROP EDITOR", True, (255, 255, 255))
        self.screen.blit(title, (20, 20))
        
        # Istruzioni
        instructions = [
            "↑/↓: Seleziona campo | ←/→: Modifica | ENTER: Edit text | CTRL+S: Salva | CTRL+N: Nuovo",
            "ESC: Esci"
        ]
        for i, text in enumerate(instructions):
            surf = self.font_small.render(text, True, (200, 200, 200))
            self.screen.blit(surf, (20, 50 + i * 20))
        
        # Campi
        y = 120
        for i, field in enumerate(self.fields):
            color = (255, 255, 100) if i == self.selected_field else (200, 200, 200)
            
            # Nome campo
            label = field.replace('_', ' ').title() + ":"
            surf = self.font.render(label, True, color)
            self.screen.blit(surf, (50, y))
            
            # Valore
            value_text = self._get_field_value_text(field)
            if self.editing_mode and i == self.selected_field:
                value_text = self.input_text + "_"
            
            value_surf = self.font.render(value_text, True, color)
            self.screen.blit(value_surf, (250, y))
            
            y += 35
        
        # Preview
        self._draw_preview()
        
        # Lista props salvati
        self._draw_props_list()
    
    def _get_field_value_text(self, field):
        """Ottiene il testo del valore per un campo"""
        if field == 'prop_id':
            return self.current_prop.prop_id
        elif field == 'name':
            return self.current_prop.name
        elif field == 'section_type':
            return self.current_prop.section_type
        elif field == 'section_index':
            return str(self.current_prop.section_index) if self.current_prop.section_index is not None else "None"
        elif field == 'visual_type':
            return self.current_prop.visual['type']
        elif field == 'color_r':
            return str(self.current_prop.visual.get('color', [0,0,0])[0])
        elif field == 'color_g':
            return str(self.current_prop.visual.get('color', [0,0,0])[1])
        elif field == 'color_b':
            return str(self.current_prop.visual.get('color', [0,0,0])[2])
        elif field == 'size_param':
            if self.current_prop.visual['type'] == 'circle':
                return str(self.current_prop.visual.get('radius', 10))
            elif self.current_prop.visual['type'] == 'rect':
                return str(self.current_prop.visual.get('width', 20))
            else:
                return str(self.current_prop.visual.get('size', 15))
        elif field == 'blocking':
            return "YES" if self.current_prop.blocking else "NO"
        elif field == 'z_offset':
            return str(self.current_prop.z_offset)
        return ""
    
    def _draw_preview(self):
        """Disegna preview del prop su hex tile"""
        preview_x = 600
        preview_y = 150
        
        # Titolo
        title = self.font.render("PREVIEW:", True, (255, 255, 255))
        self.screen.blit(title, (preview_x, preview_y - 30))
        
        # Disegna hex tile
        self.preview_tile.draw_3d(self.screen, preview_x, preview_y)
        
        # Disegna prop
        self.current_prop.draw(self.screen, self.preview_tile, preview_x, preview_y)
    
    def _draw_props_list(self):
        """Disegna lista dei props salvati"""
        list_x = 600
        list_y = 400
        
        title = self.font.render("PROPS SALVATI:", True, (255, 255, 255))
        self.screen.blit(title, (list_x, list_y - 30))
        
        y = list_y
        for i, prop in enumerate(self.prop_list[:8]):  # Max 8 props
            text = f"{prop.prop_id} - {prop.name}"
            surf = self.font_small.render(text, True, (180, 180, 180))
            self.screen.blit(surf, (list_x, y))
            y += 25
        
        if len(self.prop_list) > 8:
            more = self.font_small.render(f"... e altri {len(self.prop_list) - 8}", True, (150, 150, 150))
            self.screen.blit(more, (list_x, y))


if __name__ == "__main__":
    editor = PropEditor()
    editor.run()