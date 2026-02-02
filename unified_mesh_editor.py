#!/usr/bin/env python3
"""
UNIFIED MESH EDITOR - Editor completo per mesh templates
Combina: palette editing, texture tiling, prop drag-and-drop
"""

import pygame
import sys
import math
from pathlib import Path
from tkinter import Tk, filedialog
from typing import Optional, List

# Import custom modules
try:
    from hex_tiles_main import HexTile
    from palette_manager import get_palette_manager
    from mesh_template import MeshTemplate, get_template_manager
    from color_picker_widget import ColorPickerHSV
    import patterns
except ImportError as e:
    print(f"❌ Import error: {e}")
    print("Assicurati che tutti i file siano presenti!")
    sys.exit(1)


class UnifiedMeshEditor:
    """Editor unificato per creare mesh templates completi"""
    
    def __init__(self):
        pygame.init()
        
        # Finestra grande per tutti i controlli
        self.screen = pygame.display.set_mode((1600, 1000))
        pygame.display.set_caption("UNIFIED MESH EDITOR - Palette + Texture + Props")
        self.clock = pygame.time.Clock()
        
        # Font
        self.font_large = pygame.font.Font(None, 36)
        self.font_medium = pygame.font.Font(None, 28)
        self.font_small = pygame.font.Font(None, 22)
        
        # Managers
        self.palette_manager = get_palette_manager()
        self.template_manager = get_template_manager()
        
        # Directory
        self.assets_dir = Path("assets")
        self.assets_dir.mkdir(exist_ok=True)
        
        # === STATO BIOMA ===
        self.available_biomes = self.palette_manager.get_available_biomes()
        self.current_biome = 'forest' if 'forest' in self.available_biomes else self.available_biomes[0]
        self.biome_palette = self.palette_manager.load_palette(self.current_biome)
        
        # === STATO SEZIONE ===
        self.section_type = "center"
        self.section_index = None
        
        # === STATO COLORI ===
        self.selected_color_indices = [0, 1, 2]  # Default: primi 3 colori
        self.editing_color_index = None  # Quale colore stiamo editando
        
        # === STATO TEXTURE ===
        self.texture_path = None
        self.texture_surface = None
        self.texture_scale = 1.0
        
        # Pattern generator
        self.pattern_type = 'solid'  # Da patterns.PATTERN_DEFINITIONS
        self.variation_type = 'none'  # Da patterns.VARIATION_TYPES
        self.variation_intensity = 0.3
        self.generated_texture = None  # Cache texture generata
        
        # === STATO IMMAGINE PROP ===
        self.image_path = None
        self.image_surface = None
        self.image_scale = 1.0
        self.image_z_offset = 0.5
        self.image_blocking = True
        self.image_offset_x = 0
        self.image_offset_y = 0
        
        # === STATO TEMPLATE ===
        self.template_id = ""
        self.template_name = ""
        
        # === UI STATE ===
        self.color_picker = None  # Aperto quando editing_color_index != None
        self.dragging_image = False
        self.drag_start_pos = None
        self.drag_offset_start = (0, 0)
        self.editing_field = None
        self.input_text = ""
        
        # === PREVIEW HEX ===
        self.hex_size = 120  # MATCH con main!
        self.preview_x = 1200
        self.preview_y = 400
        self.preview_tile = None
        self._update_preview_tile()
        
        # Messages
        self.message = ""
        self.message_timer = 0
    
    def _update_preview_tile(self):
        """Aggiorna preview tile con sezione corrente custom + resto random"""
        import random
        
        # STEP 1: Genera 7 colori RANDOM dalla palette del bioma (base)
        section_colors = [random.choice(self.biome_palette) for _ in range(7)]
        
        # STEP 2: Sovrascrivi SOLO la sezione corrente con colori selezionati
        if self.selected_color_indices:
            # Risolvi colori selezionati
            custom_colors = self.palette_manager.resolve_colors(
                self.current_biome, 
                self.selected_color_indices
            )
            
            if custom_colors:
                # Calcola colore medio dai colori selezionati (mesh)
                avg_r = sum(c[0] for c in custom_colors) // len(custom_colors)
                avg_g = sum(c[1] for c in custom_colors) // len(custom_colors)
                avg_b = sum(c[2] for c in custom_colors) // len(custom_colors)
                custom_color = (avg_r, avg_g, avg_b)
                
                # Applica SOLO alla sezione corrente
                if self.section_type == 'center':
                    section_colors[0] = custom_color  # Centro
                elif self.section_type == 'wedge' and self.section_index is not None:
                    section_colors[self.section_index + 1] = custom_color  # Wedge specifico
        
        # STEP 3: Crea tile con colori calcolati
        self.preview_tile = HexTile(
            self.hex_size,
            self.current_biome,
            pointy_top=True
        )
        
        # Override colori
        self.preview_tile.section_colors = section_colors[:7]
    
    def run(self):
        """Main loop"""
        running = True
        dt = 0
        
        while running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                
                # Color picker ha priorità
                if self.color_picker and self.color_picker.handle_event(event):
                    # Aggiorna colore nella palette
                    new_rgb = self.color_picker.get_color_rgb()
                    self.biome_palette[self.editing_color_index] = new_rgb
                    self._update_preview_tile()
                    continue
                
                elif event.type == pygame.KEYDOWN:
                    if not self._handle_keydown(event):
                        running = False
                
                elif event.type == pygame.MOUSEBUTTONDOWN:
                    self._handle_mouse_down(event)
                
                elif event.type == pygame.MOUSEBUTTONUP:
                    self.dragging_image = False
                
                elif event.type == pygame.MOUSEMOTION:
                    self._handle_mouse_motion(event)
            
            # Update message timer
            if self.message_timer > 0:
                self.message_timer -= dt
                if self.message_timer <= 0:
                    self.message = ""
            
            # Draw
            self._draw()
            pygame.display.flip()
            dt = self.clock.tick(60) / 1000.0
        
        pygame.quit()
    
    def _handle_keydown(self, event) -> bool:
        """Gestisce input tastiera"""
        # Editing text field
        if self.editing_field:
            if event.key == pygame.K_ESCAPE:
                self.editing_field = None
                self.input_text = ""
            elif event.key == pygame.K_RETURN:
                self._apply_input()
                self.editing_field = None
            elif event.key == pygame.K_BACKSPACE:
                self.input_text = self.input_text[:-1]
            else:
                self.input_text += event.unicode
            return True
        
        # Color picker aperto
        if self.color_picker:
            if event.key == pygame.K_ESCAPE:
                self.color_picker = None
            elif event.key == pygame.K_RETURN:
                # Salva palette modificata
                self.palette_manager.save_palette(self.current_biome, self.biome_palette)
                self.color_picker = None
                self._show_message("✓ Palette salvata")
            return True
        
        # Normal mode
        if event.key == pygame.K_ESCAPE:
            return False
        
        elif event.key == pygame.K_TAB:
            self._cycle_section()
        
        elif event.key == pygame.K_b:
            self.image_blocking = not self.image_blocking
            self._show_message(f"Blocking: {'ON' if self.image_blocking else 'OFF'}")
        
        elif event.key == pygame.K_r:
            self.image_offset_x = 0
            self.image_offset_y = 0
            self._show_message("Reset posizione immagine")
        
        elif event.key == pygame.K_s and pygame.key.get_mods() & pygame.KMOD_CTRL:
            self._save_template()
        
        return True
    
    def _handle_mouse_down(self, event):
        """Gestisce click mouse"""
        mx, my = event.pos
        
        # === BUTTONS ===
        if self._check_button_click(mx, my):
            return
        
        # === COLOR CHECKBOXES ===
        if self._check_color_checkbox_click(mx, my):
            return
        
        # === COLOR BOXES (per aprire picker) ===
        if self._check_color_box_click(mx, my):
            return
        
        # === TEXT FIELDS ===
        if self._check_text_field_click(mx, my):
            return
        
        # === BIOME DROPDOWN ===
        if self._check_biome_dropdown_click(mx, my):
            return
        
        # === DRAG IMMAGINE ===
        if self.image_surface:
            img_cx, img_cy = self._get_image_center()
            img_rect = self.image_surface.get_rect(center=(img_cx, img_cy))
            
            if img_rect.collidepoint(mx, my):
                self.dragging_image = True
                self.drag_start_pos = (mx, my)
                self.drag_offset_start = (self.image_offset_x, self.image_offset_y)
    
    def _handle_mouse_motion(self, event):
        """Gestisce movimento mouse durante drag"""
        if self.dragging_image and self.drag_start_pos:
            mx, my = event.pos
            dx = mx - self.drag_start_pos[0]
            dy = my - self.drag_start_pos[1]
            
            self.image_offset_x = self.drag_offset_start[0] + dx
            self.image_offset_y = self.drag_offset_start[1] + dy
    
    # === HELPER METHODS ===
    
    def _cycle_section(self):
        """Cicla tra sezioni con TAB"""
        if self.section_type == "center":
            self.section_type = "wedge"
            self.section_index = 0
        elif self.section_index < 5:
            self.section_index += 1
        else:
            self.section_type = "center"
            self.section_index = None
        
        self._auto_generate_template_id()
        self._update_preview_tile()  # AGGIUNTO: Rigenera preview con nuova sezione
        self._show_message(f"Sezione: {self._get_section_name()}")
    
    def _get_section_name(self) -> str:
        """Nome leggibile sezione"""
        if self.section_type == "center":
            return "Centro"
        else:
            directions = ["E", "NE", "NW", "W", "SW", "SE"]
            return f"Wedge {self.section_index} ({directions[self.section_index]})"
    
    def _auto_generate_template_id(self):
        """Auto-genera template_id"""
        base = self.current_biome
        section_str = self.section_type
        if self.section_index is not None:
            section_str += f"_{self.section_index}"
        
        # Aggiungi counter se esiste già
        counter = 1
        while True:
            self.template_id = f"{base}_{section_str}_{counter:02d}"
            if not (Path("mesh_templates") / f"{self.template_id}.json").exists():
                break
            counter += 1
    
    def _show_message(self, msg: str):
        """Mostra messaggio temporaneo"""
        self.message = msg
        self.message_timer = 3.0
    
    # === BUTTON HANDLERS ===
    
    def _check_button_click(self, mx: int, my: int) -> bool:
        """Check click su buttons, ritorna True se gestito"""
        # Browse Texture
        if 20 <= mx <= 220 and 180 <= my <= 230:
            self._browse_texture()
            return True
        
        # Browse Image
        if 240 <= mx <= 440 and 180 <= my <= 230:
            self._browse_image()
            return True
        
        # Generate Texture (NUOVO!)
        if 460 <= mx <= 660 and 180 <= my <= 230:
            self._generate_texture()
            return True
        
        # Cycle Pattern Type (coordinate corrette: y_start=820, y=850, button_y=875)
        if 20 <= mx <= 220 and 875 <= my <= 905:
            self._cycle_pattern_type()
            return True
        
        # Cycle Variation Type
        if 240 <= mx <= 440 and 875 <= my <= 905:
            self._cycle_variation_type()
            return True
        
        # Reset Position
        if 20 <= mx <= 220 and 250 <= my <= 300:
            self.image_offset_x = 0
            self.image_offset_y = 0
            self._show_message("Reset posizione")
            return True
        
        # Save Template
        if 240 <= mx <= 440 and 250 <= my <= 300:
            self._save_template()
            return True
        
        # Save Palette
        if 20 <= mx <= 220 and 320 <= my <= 370:
            self.palette_manager.save_palette(self.current_biome, self.biome_palette)
            self._show_message("✓ Palette salvata")
            return True
        
        return False
    
    def _cycle_pattern_type(self):
        """Cicla tra pattern types"""
        pattern_keys = list(patterns.PATTERN_DEFINITIONS.keys())
        current_idx = pattern_keys.index(self.pattern_type)
        next_idx = (current_idx + 1) % len(pattern_keys)
        self.pattern_type = pattern_keys[next_idx]
        
        pattern_name = patterns.PATTERN_DEFINITIONS[self.pattern_type]['name']
        self._show_message(f"Pattern: {pattern_name}")
    
    def _cycle_variation_type(self):
        """Cicla tra variation types"""
        # Aggiungi 'none' come prima opzione
        var_keys = ['none'] + list(patterns.VARIATION_TYPES.keys())
        
        try:
            current_idx = var_keys.index(self.variation_type)
        except ValueError:
            current_idx = 0
        
        next_idx = (current_idx + 1) % len(var_keys)
        self.variation_type = var_keys[next_idx]
        
        if self.variation_type == 'none':
            var_name = "Nessuna"
        else:
            var_name = patterns.VARIATION_TYPES[self.variation_type]['name']
        
        self._show_message(f"Variazione: {var_name}")
    
    def _generate_texture(self):
        """Genera texture procedurale usando patterns.py"""
        try:
            # Colori dalla palette selezionati
            colors = self.palette_manager.resolve_colors(
                self.current_biome, 
                self.selected_color_indices
            )
            
            if not colors:
                self._show_message("❌ Seleziona almeno un colore!")
                return
            
            # Config variazione
            variation_config = {
                'type': self.variation_type,
                'intensity': self.variation_intensity
            }
            
            # Dimensione texture (256x256 standard)
            size = (256, 256)
            
            # Genera!
            self.generated_texture = patterns.generate_pattern_texture(
                colors=colors,
                pattern_type=self.pattern_type,
                variation_config=variation_config,
                section_shape=self.section_type,
                size=size
            )
            
            # Usa come texture corrente
            self.texture_surface = self.generated_texture
            self.texture_path = None  # Non da file
            
            pattern_name = patterns.PATTERN_DEFINITIONS[self.pattern_type]['name']
            self._show_message(f"✓ Generata texture: {pattern_name}")
        
        except Exception as e:
            self._show_message(f"❌ Errore: {str(e)[:30]}")
            print(f"Errore generando texture: {e}")
    
    def _browse_texture(self):
        """Apri file dialog per texture"""
        root = Tk()
        root.withdraw()
        
        filepath = filedialog.askopenfilename(
            title="Seleziona Texture PNG",
            initialdir=self.assets_dir,
            filetypes=[("PNG files", "*.png"), ("All files", "*.*")]
        )
        
        root.destroy()
        
        if filepath:
            self._load_texture(filepath)
    
    def _browse_image(self):
        """Apri file dialog per immagine prop"""
        root = Tk()
        root.withdraw()
        
        filepath = filedialog.askopenfilename(
            title="Seleziona Immagine Prop PNG",
            initialdir=self.assets_dir,
            filetypes=[("PNG files", "*.png"), ("All files", "*.*")]
        )
        
        root.destroy()
        
        if filepath:
            self._load_image(filepath)
    
    def _load_texture(self, filepath: str):
        """Carica texture"""
        try:
            filepath = Path(filepath)
            
            # Path relativo
            try:
                rel_path = filepath.relative_to(Path.cwd())
                self.texture_path = str(rel_path)
            except:
                import shutil
                dest = self.assets_dir / filepath.name
                shutil.copy(filepath, dest)
                self.texture_path = f"assets/{filepath.name}"
            
            self.texture_surface = pygame.image.load(self.texture_path)
            self._show_message(f"✓ Texture: {filepath.name}")
        
        except Exception as e:
            self._show_message(f"✗ Errore: {e}")
    
    def _load_image(self, filepath: str):
        """Carica immagine prop"""
        try:
            filepath = Path(filepath)
            
            # Path relativo
            try:
                rel_path = filepath.relative_to(Path.cwd())
                self.image_path = str(rel_path)
            except:
                import shutil
                dest = self.assets_dir / filepath.name
                shutil.copy(filepath, dest)
                self.image_path = f"assets/{filepath.name}"
            
            # Carica e scala
            img = pygame.image.load(self.image_path)
            self.image_surface = pygame.transform.scale(
                img,
                (int(img.get_width() * self.image_scale),
                 int(img.get_height() * self.image_scale))
            )
            
            # Auto-genera nome se vuoto
            if not self.template_name:
                self.template_name = filepath.stem.replace('_', ' ').title()
            
            self._auto_generate_template_id()
            self._show_message(f"✓ Immagine: {filepath.name}")
        
        except Exception as e:
            self._show_message(f"✗ Errore: {e}")
    
    def _check_color_checkbox_click(self, mx: int, my: int) -> bool:
        """Check click su checkbox colori"""
        # Checkbox sono a x=30, y inizia da 450
        x_checkbox = 30
        y_start = 450
        spacing = 35
        
        for i in range(7):
            y = y_start + i * spacing
            checkbox_rect = pygame.Rect(x_checkbox, y, 20, 20)
            
            if checkbox_rect.collidepoint(mx, my):
                # Toggle
                if i in self.selected_color_indices:
                    self.selected_color_indices.remove(i)
                else:
                    self.selected_color_indices.append(i)
                self.selected_color_indices.sort()
                self._update_preview_tile()
                return True
        
        return False
    
    def _check_color_box_click(self, mx: int, my: int) -> bool:
        """Check click su color box per aprire picker"""
        x_box = 60
        y_start = 450
        spacing = 35
        
        for i in range(7):
            y = y_start + i * spacing
            box_rect = pygame.Rect(x_box, y, 60, 25)
            
            if box_rect.collidepoint(mx, my):
                # Apri color picker
                self.editing_color_index = i
                self.color_picker = ColorPickerHSV(800, 500, radius=150)
                self.color_picker.set_color_rgb(*self.biome_palette[i])
                return True
        
        return False
    
    def _check_text_field_click(self, mx: int, my: int) -> bool:
        """Check click su text fields"""
        if 150 <= mx <= 600:
            if 710 <= my <= 740:
                self.editing_field = 'template_id'
                self.input_text = self.template_id
                return True
            elif 760 <= my <= 790:
                self.editing_field = 'template_name'
                self.input_text = self.template_name
                return True
        return False
    
    def _check_biome_dropdown_click(self, mx: int, my: int) -> bool:
        """Check click su biome dropdown"""
        # Dropdown è a 20, 120, larghezza 300
        if 20 <= mx <= 320 and 120 <= my <= 160:
            # Cicla biomi
            current_idx = self.available_biomes.index(self.current_biome)
            next_idx = (current_idx + 1) % len(self.available_biomes)
            self.current_biome = self.available_biomes[next_idx]
            self.biome_palette = self.palette_manager.load_palette(self.current_biome)
            self._update_preview_tile()
            self._auto_generate_template_id()
            self._show_message(f"Bioma: {self.current_biome}")
            return True
        return False
    
    def _apply_input(self):
        """Applica input text"""
        if self.editing_field == 'template_id':
            self.template_id = self.input_text
        elif self.editing_field == 'template_name':
            self.template_name = self.input_text
        self.input_text = ""
    
    def _save_template(self):
        """Salva mesh template"""
        if not self.template_id.strip():
            self._show_message("❌ Template ID obbligatorio!")
            return
        
        if not self.selected_color_indices:
            self._show_message("❌ Seleziona almeno un colore!")
            return
        
        try:
            # Se texture generata (non da file), salva info pattern
            pattern_info = None
            if self.texture_surface and not self.texture_path:
                pattern_info = {
                    'pattern_type': self.pattern_type,
                    'variation_type': self.variation_type,
                    'variation_intensity': self.variation_intensity
                }
            
            template = MeshTemplate(
                template_id=self.template_id,
                name=self.template_name if self.template_name.strip() else self.template_id,
                biome=self.current_biome,
                section_type=self.section_type,
                section_index=self.section_index,
                color_indices=self.selected_color_indices,
                texture_path=self.texture_path,  # None se generata
                texture_scale=self.texture_scale,
                image_path=self.image_path,
                image_offset_x=self.image_offset_x,
                image_offset_y=self.image_offset_y,
                image_scale=self.image_scale,
                image_z_offset=self.image_z_offset,
                image_blocking=self.image_blocking
            )
            
            # Aggiungi pattern_info se presente (estensione del dict)
            if pattern_info:
                data = template.to_dict()
                data['pattern_info'] = pattern_info
                
                # Salva manualmente con pattern_info
                import json
                filepath = Path("mesh_templates") / f"{self.template_id}.json"
                with open(filepath, 'w', encoding='utf-8') as f:
                    json.dump(data, f, indent=2, ensure_ascii=False)
                
                self._show_message(f"✓ Salvato con pattern: {filepath.name}")
                print(f"✓ Template salvato: {filepath}")
            else:
                filepath = template.save_to_file()
                self._show_message(f"✓ Salvato: {filepath.name}")
                print(f"✓ Template salvato: {filepath}")
        
        except Exception as e:
            self._show_message(f"❌ Errore: {str(e)[:30]}")
            print(f"✗ Errore salvando: {e}")
    
    def _get_section_center(self) -> tuple:
        """Calcola centro sezione sul preview tile"""
        cx = self.preview_x + self.preview_tile.get_width() // 2
        cy = self.preview_y + self.hex_size
        
        if self.section_type == "center":
            return cx, cy
        
        # Wedge
        angle_start = math.pi / 3 * self.section_index - math.pi / 6
        angle_end = math.pi / 3 * (self.section_index + 1) - math.pi / 6
        angle = (angle_start + angle_end) / 2
        
        inner_radius = self.hex_size * 0.60
        outer_radius = self.hex_size
        prop_radius = (inner_radius + outer_radius) * 0.45
        
        wx = cx + prop_radius * math.cos(angle)
        wy = cy + prop_radius * math.sin(angle)
        
        return wx, wy
    
    def _get_image_center(self) -> tuple:
        """Calcola posizione immagine con offset + z_offset"""
        cx, cy = self._get_section_center()
        cy -= int(self.image_z_offset * self.hex_size)
        return int(cx + self.image_offset_x), int(cy + self.image_offset_y)
    
    # === DRAW METHODS ===
    
    def _draw(self):
        """Disegna interfaccia"""
        self.screen.fill((25, 25, 30))
        
        # Titolo
        title = self.font_large.render("UNIFIED MESH EDITOR", True, (255, 255, 100))
        self.screen.blit(title, (20, 20))
        
        subtitle = self.font_small.render("Palette + Texture + Props", True, (180, 180, 180))
        self.screen.blit(subtitle, (20, 60))
        
        # Colonna sinistra: controlli
        self._draw_left_column()
        
        # Colonna destra: preview
        self._draw_preview()
        
        # Color picker (se aperto)
        if self.color_picker:
            self._draw_color_picker_overlay()
        
        # Message
        if self.message:
            self._draw_message()
        
        # Instructions
        self._draw_instructions()
    
    def _draw_left_column(self):
        """Disegna colonna sinistra con controlli"""
        # Bioma dropdown
        self._draw_biome_selector()
        
        # Buttons
        self._draw_buttons()
        
        # Color palette
        self._draw_color_palette()
        
        # Sezione
        self._draw_section_info()
        
        # Template info
        self._draw_template_fields()
        
        # Pattern controls (NUOVO!)
        self._draw_pattern_controls()
    
    def _draw_biome_selector(self):
        """Disegna selector bioma"""
        label = self.font_medium.render("Bioma:", True, (255, 255, 255))
        self.screen.blit(label, (20, 90))
        
        # Dropdown
        dropdown_rect = pygame.Rect(20, 120, 300, 40)
        pygame.draw.rect(self.screen, (50, 100, 150), dropdown_rect, border_radius=5)
        pygame.draw.rect(self.screen, (100, 150, 200), dropdown_rect, 2, border_radius=5)
        
        biome_text = self.font_medium.render(self.current_biome.upper(), True, (255, 255, 255))
        text_rect = biome_text.get_rect(center=dropdown_rect.center)
        self.screen.blit(biome_text, text_rect)
        
        hint = self.font_small.render("(Click per cambiare)", True, (150, 150, 150))
        self.screen.blit(hint, (340, 130))
    
    def _draw_buttons(self):
        """Disegna buttons"""
        y = 180
        
        # Browse Texture
        btn_texture = pygame.Rect(20, y, 200, 50)
        pygame.draw.rect(self.screen, (100, 70, 150), btn_texture, border_radius=5)
        text = self.font_small.render("Browse Texture", True, (255, 255, 255))
        self.screen.blit(text, (btn_texture.centerx - text.get_width()//2, btn_texture.centery - text.get_height()//2))
        
        # Browse Image
        btn_image = pygame.Rect(240, y, 200, 50)
        pygame.draw.rect(self.screen, (70, 130, 100), btn_image, border_radius=5)
        text = self.font_small.render("Browse Immagine", True, (255, 255, 255))
        self.screen.blit(text, (btn_image.centerx - text.get_width()//2, btn_image.centery - text.get_height()//2))
        
        # Generate Texture (NUOVO!)
        btn_generate = pygame.Rect(460, y, 200, 50)
        pygame.draw.rect(self.screen, (150, 70, 180), btn_generate, border_radius=5)
        text = self.font_small.render("Generate Texture", True, (255, 255, 255))
        self.screen.blit(text, (btn_generate.centerx - text.get_width()//2, btn_generate.centery - text.get_height()//2))
        
        y += 70
        
        # Reset Position
        btn_reset = pygame.Rect(20, y, 200, 50)
        pygame.draw.rect(self.screen, (200, 100, 50), btn_reset, border_radius=5)
        text = self.font_small.render("Reset Position", True, (255, 255, 255))
        self.screen.blit(text, (btn_reset.centerx - text.get_width()//2, btn_reset.centery - text.get_height()//2))
        
        # Save Template
        btn_save = pygame.Rect(240, y, 200, 50)
        pygame.draw.rect(self.screen, (50, 180, 50), btn_save, border_radius=5)
        text = self.font_small.render("SAVE TEMPLATE", True, (255, 255, 255))
        self.screen.blit(text, (btn_save.centerx - text.get_width()//2, btn_save.centery - text.get_height()//2))
        
        y += 70
        
        # Save Palette
        btn_palette = pygame.Rect(20, y, 200, 50)
        pygame.draw.rect(self.screen, (180, 50, 150), btn_palette, border_radius=5)
        text = self.font_small.render("Salva Palette", True, (255, 255, 255))
        self.screen.blit(text, (btn_palette.centerx - text.get_width()//2, btn_palette.centery - text.get_height()//2))
    
    def _draw_pattern_controls(self):
        """Disegna controlli pattern generator"""
        y_start = 820
        
        # Titolo
        title = self.font_medium.render("Pattern Generator:", True, (255, 200, 100))
        self.screen.blit(title, (20, y_start))
        
        y = y_start + 30
        
        # Pattern Type
        label = self.font_small.render("Pattern:", True, (255, 255, 255))
        self.screen.blit(label, (20, y))
        
        pattern_name = patterns.PATTERN_DEFINITIONS[self.pattern_type]['name']
        btn_pattern = pygame.Rect(20, y + 25, 200, 30)
        pygame.draw.rect(self.screen, (70, 70, 120), btn_pattern, border_radius=3)
        pygame.draw.rect(self.screen, (120, 120, 170), btn_pattern, 2, border_radius=3)
        text = self.font_small.render(pattern_name, True, (255, 255, 255))
        self.screen.blit(text, (btn_pattern.centerx - text.get_width()//2, btn_pattern.centery - text.get_height()//2))
        
        # Variation Type
        label = self.font_small.render("Variazione:", True, (255, 255, 255))
        self.screen.blit(label, (240, y))
        
        if self.variation_type == 'none':
            var_name = "Nessuna"
        else:
            var_name = patterns.VARIATION_TYPES[self.variation_type]['name']
        
        btn_variation = pygame.Rect(240, y + 25, 200, 30)
        pygame.draw.rect(self.screen, (70, 100, 70), btn_variation, border_radius=3)
        pygame.draw.rect(self.screen, (120, 150, 120), btn_variation, 2, border_radius=3)
        text = self.font_small.render(var_name, True, (255, 255, 255))
        self.screen.blit(text, (btn_variation.centerx - text.get_width()//2, btn_variation.centery - text.get_height()//2))
        
        # Intensity slider (simplified - solo display per ora)
        y += 65
        label = self.font_small.render(f"Intensità: {self.variation_intensity:.2f}", True, (255, 255, 255))
        self.screen.blit(label, (20, y))
        
        hint = self.font_small.render("(Click sui bottoni per cambiare)", True, (150, 150, 150))
        self.screen.blit(hint, (20, y + 25))
    
    def _draw_color_palette(self):
        """Disegna palette colori con checkbox"""
        label = self.font_medium.render("Palette Colori (Click su box per editare):", True, (255, 255, 255))
        self.screen.blit(label, (20, 410))
        
        x_checkbox = 30
        x_box = 60
        x_text = 130
        y_start = 450
        spacing = 35
        
        for i in range(7):
            y = y_start + i * spacing
            
            # Checkbox
            checked = i in self.selected_color_indices
            checkbox_rect = pygame.Rect(x_checkbox, y, 20, 20)
            pygame.draw.rect(self.screen, (200, 200, 200), checkbox_rect, 2)
            if checked:
                pygame.draw.line(self.screen, (0, 255, 0), (x_checkbox + 3, y + 10), (x_checkbox + 9, y + 17), 3)
                pygame.draw.line(self.screen, (0, 255, 0), (x_checkbox + 9, y + 17), (x_checkbox + 17, y + 3), 3)
            
            # Color box
            r, g, b = self.biome_palette[i]
            box_rect = pygame.Rect(x_box, y, 60, 25)
            pygame.draw.rect(self.screen, (r, g, b), box_rect)
            pygame.draw.rect(self.screen, (255, 255, 255), box_rect, 1)
            
            # Text
            text = self.font_small.render(f"RGB({r}, {g}, {b})", True, (200, 200, 200))
            self.screen.blit(text, (x_text, y + 3))
    
    def _draw_section_info(self):
        """Disegna info sezione"""
        label = self.font_medium.render("Sezione:", True, (255, 255, 255))
        self.screen.blit(label, (20, 650))
        
        section_name = self._get_section_name()
        value = self.font_medium.render(section_name, True, (0, 255, 255))
        self.screen.blit(value, (150, 650))
        
        hint = self.font_small.render("(TAB per cambiare)", True, (150, 150, 150))
        self.screen.blit(hint, (150, 680))
    
    def _draw_template_fields(self):
        """Disegna campi template"""
        y = 710
        
        # Template ID
        label = self.font_small.render("Template ID:", True, (255, 255, 255))
        self.screen.blit(label, (20, y))
        
        text = self.input_text + "_" if self.editing_field == 'template_id' else self.template_id
        color = (255, 255, 100) if self.editing_field == 'template_id' else (200, 200, 200)
        value = self.font_small.render(text, True, color)
        self.screen.blit(value, (150, y))
        pygame.draw.line(self.screen, (80, 80, 90), (150, y + 25), (600, y + 25), 1)
        
        y += 50
        
        # Template Name
        label = self.font_small.render("Nome:", True, (255, 255, 255))
        self.screen.blit(label, (20, y))
        
        text = self.input_text + "_" if self.editing_field == 'template_name' else self.template_name
        color = (255, 255, 100) if self.editing_field == 'template_name' else (200, 200, 200)
        value = self.font_small.render(text, True, color)
        self.screen.blit(value, (150, y))
        pygame.draw.line(self.screen, (80, 80, 90), (150, y + 25), (600, y + 25), 1)
    
    def _draw_preview(self):
        """Disegna preview"""
        # Titolo
        title = self.font_large.render("PREVIEW", True, (255, 255, 255))
        self.screen.blit(title, (self.preview_x - 100, 100))
        
        # Hex tile
        self.preview_tile.draw_3d(self.screen, self.preview_x, self.preview_y)
        
        # Evidenzia sezione selezionata
        self._highlight_section()
        
        # Texture (TODO: tiling)
        if self.texture_surface:
            self._draw_textured_section()
        
        # Immagine prop
        if self.image_surface:
            img_cx, img_cy = self._get_image_center()
            img_rect = self.image_surface.get_rect(center=(img_cx, img_cy))
            self.screen.blit(self.image_surface, img_rect)
            
            if self.dragging_image:
                pygame.draw.rect(self.screen, (255, 255, 0), img_rect.inflate(6, 6), 3)
            elif self.image_blocking:
                pygame.draw.rect(self.screen, (255, 0, 0), img_rect.inflate(6, 6), 2)
        
        # Offset info
        offset_text = f"Offset: ({self.image_offset_x:+d}, {self.image_offset_y:+d})"
        color = (255, 255, 100) if self.dragging_image else (180, 180, 180)
        offset_surf = self.font_small.render(offset_text, True, color)
        self.screen.blit(offset_surf, (self.preview_x - 100, self.preview_y + 200))
    
    def _highlight_section(self):
        """Evidenzia sezione selezionata"""
        if self.section_type == "center":
            center_points = self.preview_tile.get_hex_points_absolute(
                self.preview_x, self.preview_y, scale=0.60
            )
            pygame.draw.polygon(self.screen, (0, 255, 255), center_points, 3)
        else:
            outer_points = self.preview_tile.get_hex_points_absolute(
                self.preview_x, self.preview_y, scale=1.0
            )
            center_points = self.preview_tile.get_hex_points_absolute(
                self.preview_x, self.preview_y, scale=0.60
            )
            
            trapezoid = self.preview_tile.get_trapezoid_points(
                center_points, outer_points, self.section_index
            )
            pygame.draw.polygon(self.screen, (0, 255, 255), trapezoid, 3)
    
    def _draw_textured_section(self):
        """Disegna texture sulla sezione corrente"""
        if not self.texture_surface:
            return
        
        try:
            # Ottieni punti sezione corrente
            if self.section_type == "center":
                section_points = self.preview_tile.get_hex_points_absolute(
                    self.preview_x, self.preview_y, scale=0.60
                )
            else:
                outer_points = self.preview_tile.get_hex_points_absolute(
                    self.preview_x, self.preview_y, scale=1.0
                )
                center_points = self.preview_tile.get_hex_points_absolute(
                    self.preview_x, self.preview_y, scale=0.60
                )
                section_points = self.preview_tile.get_trapezoid_points(
                    center_points, outer_points, self.section_index
                )
            
            # Calcola bounding box della sezione
            xs = [p[0] for p in section_points]
            ys = [p[1] for p in section_points]
            min_x, max_x = int(min(xs)), int(max(xs))
            min_y, max_y = int(min(ys)), int(max(ys))
            width = max_x - min_x
            height = max_y - min_y
            
            if width <= 0 or height <= 0:
                return
            
            # Crea maschera della sezione
            mask_surface = pygame.Surface((width, height), pygame.SRCALPHA)
            mask_surface.fill((0, 0, 0, 0))
            
            # Trasla punti rispetto a min_x, min_y
            translated_points = [(int(x - min_x), int(y - min_y)) for x, y in section_points]
            pygame.draw.polygon(mask_surface, (255, 255, 255, 255), translated_points)
            
            # Scala texture per riempire la sezione
            scaled_texture = pygame.transform.scale(
                self.texture_surface, 
                (width, height)
            )
            
            # Applica maschera
            textured_section = pygame.Surface((width, height), pygame.SRCALPHA)
            textured_section.blit(scaled_texture, (0, 0))
            
            # Usa maschera per trasparenza
            for x in range(width):
                for y in range(height):
                    mask_color = mask_surface.get_at((x, y))
                    if mask_color.a == 0:  # Fuori dalla sezione
                        textured_section.set_at((x, y), (0, 0, 0, 0))
            
            # Blit sulla screen con semi-trasparenza
            textured_section.set_alpha(200)  # 78% opacità
            self.screen.blit(textured_section, (min_x, min_y))
        
        except Exception as e:
            print(f"Errore applicando texture: {e}")
    
    def _draw_color_picker_overlay(self):
        """Disegna overlay color picker"""
        # Background scuro semi-trasparente
        overlay = pygame.Surface(self.screen.get_size(), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 180))
        self.screen.blit(overlay, (0, 0))
        
        # Color picker
        self.color_picker.draw(self.screen)
        
        # Instructions
        inst = self.font_medium.render("ENTER: Conferma | ESC: Annulla", True, (255, 255, 255))
        self.screen.blit(inst, (self.screen.get_width() // 2 - inst.get_width() // 2, 50))
    
    def _draw_message(self):
        """Disegna messaggio temporaneo"""
        msg_surf = self.font_medium.render(self.message, True, (255, 255, 255))
        msg_rect = msg_surf.get_rect(center=(self.screen.get_width() // 2, self.screen.get_height() - 50))
        bg_rect = msg_rect.inflate(40, 20)
        pygame.draw.rect(self.screen, (50, 50, 60), bg_rect, border_radius=10)
        self.screen.blit(msg_surf, msg_rect)
    
    def _draw_instructions(self):
        """Disegna istruzioni"""
        instr = self.font_small.render(
            "TAB: Sezione | B: Blocking | R: Reset | CTRL+S: Salva | ESC: Esci",
            True, (120, 120, 130)
        )
        self.screen.blit(instr, (20, self.screen.get_height() - 30))


if __name__ == "__main__":
    print("=" * 70)
    print("UNIFIED MESH EDITOR")
    print("=" * 70)
    print("\n🎨 Features:")
    print("   - Modifica palette colori bioma con color picker HSV")
    print("   - Seleziona colori per mesh con checkbox")
    print("   - Carica texture PNG per tappezzare forme")
    print("   - Drag-and-drop immagini prop pixel-perfect")
    print("   - Salva mesh template completo\n")
    print("=" * 70)
    
    try:
        editor = UnifiedMeshEditor()
        editor.run()
    except Exception as e:
        print(f"\n❌ ERRORE: {e}")
        import traceback
        traceback.print_exc()