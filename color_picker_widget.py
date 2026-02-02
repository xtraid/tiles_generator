#!/usr/bin/env python3
"""
COLOR PICKER WIDGET - Ruota HSV per scegliere colori
"""

import pygame
import math
from typing import Tuple, Optional


class ColorPickerHSV:
    """Widget color picker con ruota HSV"""
    
    def __init__(self, x: int, y: int, radius: int = 100):
        """
        Args:
            x, y: Posizione centro ruota
            radius: Raggio ruota
        """
        self.x = x
        self.y = y
        self.radius = radius
        
        # Stato colore corrente (HSV 0-1)
        self.hue = 0.0
        self.saturation = 1.0
        self.value = 1.0
        
        # Drag state
        self.dragging_hue = False
        self.dragging_sv = False
        
        # Font
        self.font = pygame.font.Font(None, 20)
    
    def set_color_rgb(self, r: int, g: int, b: int):
        """Imposta colore da RGB (0-255)"""
        self.hue, self.saturation, self.value = self._rgb_to_hsv(r, g, b)
    
    def get_color_rgb(self) -> Tuple[int, int, int]:
        """Ottiene colore come RGB (0-255)"""
        return self._hsv_to_rgb(self.hue, self.saturation, self.value)
    
    def _rgb_to_hsv(self, r: int, g: int, b: int) -> Tuple[float, float, float]:
        """Converte RGB (0-255) a HSV (0-1)"""
        r, g, b = r / 255.0, g / 255.0, b / 255.0
        max_c = max(r, g, b)
        min_c = min(r, g, b)
        diff = max_c - min_c
        
        # Hue
        if diff == 0:
            h = 0
        elif max_c == r:
            h = (60 * ((g - b) / diff) + 360) % 360
        elif max_c == g:
            h = (60 * ((b - r) / diff) + 120) % 360
        else:
            h = (60 * ((r - g) / diff) + 240) % 360
        
        # Saturation
        s = 0 if max_c == 0 else diff / max_c
        
        # Value
        v = max_c
        
        return h / 360.0, s, v
    
    def _hsv_to_rgb(self, h: float, s: float, v: float) -> Tuple[int, int, int]:
        """Converte HSV (0-1) a RGB (0-255)"""
        h = h * 360.0
        c = v * s
        x = c * (1 - abs((h / 60.0) % 2 - 1))
        m = v - c
        
        if h < 60:
            r, g, b = c, x, 0
        elif h < 120:
            r, g, b = x, c, 0
        elif h < 180:
            r, g, b = 0, c, x
        elif h < 240:
            r, g, b = 0, x, c
        elif h < 300:
            r, g, b = x, 0, c
        else:
            r, g, b = c, 0, x
        
        r = int((r + m) * 255)
        g = int((g + m) * 255)
        b = int((b + m) * 255)
        
        return max(0, min(255, r)), max(0, min(255, g)), max(0, min(255, b))
    
    def handle_event(self, event) -> bool:
        """Gestisce eventi mouse
        
        Returns:
            True se evento gestito
        """
        if event.type == pygame.MOUSEBUTTONDOWN:
            mx, my = event.pos
            dx = mx - self.x
            dy = my - self.y
            dist = math.sqrt(dx*dx + dy*dy)
            
            # Click su ruota HUE (anello esterno)
            if self.radius - 20 <= dist <= self.radius:
                self.dragging_hue = True
                self._update_hue_from_mouse(mx, my)
                return True
            
            # Click su quadrato SV (interno)
            elif dist < self.radius - 25:
                self.dragging_sv = True
                self._update_sv_from_mouse(mx, my)
                return True
        
        elif event.type == pygame.MOUSEBUTTONUP:
            self.dragging_hue = False
            self.dragging_sv = False
        
        elif event.type == pygame.MOUSEMOTION:
            if self.dragging_hue:
                mx, my = event.pos
                self._update_hue_from_mouse(mx, my)
                return True
            elif self.dragging_sv:
                mx, my = event.pos
                self._update_sv_from_mouse(mx, my)
                return True
        
        return False
    
    def _update_hue_from_mouse(self, mx: int, my: int):
        """Aggiorna hue da posizione mouse"""
        dx = mx - self.x
        dy = my - self.y
        angle = math.atan2(dy, dx)
        self.hue = (angle / (2 * math.pi)) % 1.0
    
    def _update_sv_from_mouse(self, mx: int, my: int):
        """Aggiorna saturation/value da posizione mouse"""
        # Quadrato SV centrato
        square_size = (self.radius - 30) * 1.4  # Sqrt(2) per diagonale
        
        dx = mx - self.x
        dy = my - self.y
        
        # Normalizza -1 a +1
        norm_x = max(-1, min(1, dx / (square_size / 2)))
        norm_y = max(-1, min(1, dy / (square_size / 2)))
        
        # Saturation: orizzontale (0 a sinistra, 1 a destra)
        self.saturation = (norm_x + 1) / 2.0
        
        # Value: verticale (1 in alto, 0 in basso)
        self.value = 1.0 - (norm_y + 1) / 2.0
    
    def draw(self, screen: pygame.Surface):
        """Disegna color picker"""
        # Ruota HUE (anello esterno)
        self._draw_hue_ring(screen)
        
        # Quadrato SV (interno)
        self._draw_sv_square(screen)
        
        # Indicatore HUE
        self._draw_hue_indicator(screen)
        
        # Indicatore SV
        self._draw_sv_indicator(screen)
        
        # Preview colore
        self._draw_color_preview(screen)
    
    def _draw_hue_ring(self, screen: pygame.Surface):
        """Disegna anello HUE colorato"""
        segments = 360
        for i in range(segments):
            angle1 = (i / segments) * 2 * math.pi
            angle2 = ((i + 1) / segments) * 2 * math.pi
            
            hue = i / segments
            r, g, b = self._hsv_to_rgb(hue, 1.0, 1.0)
            
            # Vertici trapezio
            inner_r = self.radius - 20
            outer_r = self.radius
            
            p1 = (self.x + inner_r * math.cos(angle1),
                  self.y + inner_r * math.sin(angle1))
            p2 = (self.x + outer_r * math.cos(angle1),
                  self.y + outer_r * math.sin(angle1))
            p3 = (self.x + outer_r * math.cos(angle2),
                  self.y + outer_r * math.sin(angle2))
            p4 = (self.x + inner_r * math.cos(angle2),
                  self.y + inner_r * math.sin(angle2))
            
            pygame.draw.polygon(screen, (r, g, b), [p1, p2, p3, p4])
    
    def _draw_sv_square(self, screen: pygame.Surface):
        """Disegna quadrato Saturation/Value"""
        square_size = (self.radius - 30) * 1.4
        steps = 50
        
        for i in range(steps):
            for j in range(steps):
                s = i / steps
                v = 1.0 - (j / steps)
                
                r, g, b = self._hsv_to_rgb(self.hue, s, v)
                
                x = self.x - square_size/2 + (i * square_size / steps)
                y = self.y - square_size/2 + (j * square_size / steps)
                
                rect = pygame.Rect(x, y, square_size/steps + 1, square_size/steps + 1)
                pygame.draw.rect(screen, (r, g, b), rect)
    
    def _draw_hue_indicator(self, screen: pygame.Surface):
        """Disegna indicatore posizione HUE"""
        angle = self.hue * 2 * math.pi
        mid_r = self.radius - 10
        
        x = self.x + mid_r * math.cos(angle)
        y = self.y + mid_r * math.sin(angle)
        
        pygame.draw.circle(screen, (255, 255, 255), (int(x), int(y)), 8)
        pygame.draw.circle(screen, (0, 0, 0), (int(x), int(y)), 8, 2)
    
    def _draw_sv_indicator(self, screen: pygame.Surface):
        """Disegna indicatore posizione SV"""
        square_size = (self.radius - 30) * 1.4
        
        x = self.x + (self.saturation - 0.5) * square_size
        y = self.y + (0.5 - self.value) * square_size
        
        pygame.draw.circle(screen, (255, 255, 255), (int(x), int(y)), 6)
        pygame.draw.circle(screen, (0, 0, 0), (int(x), int(y)), 6, 2)
    
    def _draw_color_preview(self, screen: pygame.Surface):
        """Disegna preview colore selezionato + RGB"""
        r, g, b = self.get_color_rgb()
        
        # Box preview
        preview_rect = pygame.Rect(self.x - 50, self.y + self.radius + 30, 100, 40)
        pygame.draw.rect(screen, (r, g, b), preview_rect)
        pygame.draw.rect(screen, (255, 255, 255), preview_rect, 2)
        
        # Testo RGB
        text = f"RGB({r}, {g}, {b})"
        surf = self.font.render(text, True, (255, 255, 255))
        text_rect = surf.get_rect(center=(self.x, self.y + self.radius + 80))
        screen.blit(surf, text_rect)
