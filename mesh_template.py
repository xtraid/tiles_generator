#!/usr/bin/env python3
"""
MESH TEMPLATE - Rappresenta una configurazione salvabile di tile section
Include: bioma, indici colori, texture, immagine prop
"""

import json
from pathlib import Path
from typing import List, Optional, Tuple
from palette_manager import get_palette_manager


class MeshTemplate:
    """Template per una sezione di tile (center o wedge) con colori, texture, prop"""
    
    def __init__(
        self,
        template_id: str,
        name: str,
        biome: str,
        section_type: str,
        section_index: Optional[int] = None,
        color_indices: Optional[List[int]] = None,
        texture_path: Optional[str] = None,
        texture_scale: float = 1.0,
        image_path: Optional[str] = None,
        image_offset_x: int = 0,
        image_offset_y: int = 0,
        image_scale: float = 1.0,
        image_z_offset: float = 0.5,
        image_blocking: bool = True
    ):
        """
        Args:
            template_id: ID univoco (es: "forest_center_oak")
            name: Nome descrittivo (es: "Quercia Centro Foresta")
            biome: Nome bioma (es: "forest")
            section_type: "center" o "wedge"
            section_index: None per center, 0-5 per wedge
            color_indices: Lista indici da palette bioma (es: [0, 2, 5])
            texture_path: Path texture PNG per tappezzare forma
            texture_scale: Scala texture (1.0 = normale)
            image_path: Path immagine PNG prop
            image_offset_x, image_offset_y: Offset pixel-perfect
            image_scale: Scala immagine
            image_z_offset: Altezza verticale
            image_blocking: Se blocca movimento
        """
        self.template_id = template_id
        self.name = name
        self.biome = biome
        self.section_type = section_type
        self.section_index = section_index
        self.color_indices = color_indices or []
        self.texture_path = texture_path
        self.texture_scale = texture_scale
        self.image_path = image_path
        self.image_offset_x = image_offset_x
        self.image_offset_y = image_offset_y
        self.image_scale = image_scale
        self.image_z_offset = image_z_offset
        self.image_blocking = image_blocking
    
    def get_resolved_colors(self) -> List[Tuple[int, int, int]]:
        """Risolve indici in colori effettivi dalla palette globale"""
        pm = get_palette_manager()
        return pm.resolve_colors(self.biome, self.color_indices)
    
    def to_dict(self) -> dict:
        """Serializza per salvataggio"""
        return {
            'template_id': self.template_id,
            'name': self.name,
            'biome': self.biome,
            'section_type': self.section_type,
            'section_index': self.section_index,
            'color_indices': self.color_indices,
            'texture_path': self.texture_path,
            'texture_scale': self.texture_scale,
            'image_path': self.image_path,
            'image_offset_x': self.image_offset_x,
            'image_offset_y': self.image_offset_y,
            'image_scale': self.image_scale,
            'image_z_offset': self.image_z_offset,
            'image_blocking': self.image_blocking
        }
    
    @staticmethod
    def from_dict(data: dict) -> 'MeshTemplate':
        """Deserializza da dict"""
        return MeshTemplate(
            template_id=data['template_id'],
            name=data['name'],
            biome=data['biome'],
            section_type=data['section_type'],
            section_index=data.get('section_index'),
            color_indices=data.get('color_indices', []),
            texture_path=data.get('texture_path'),
            texture_scale=data.get('texture_scale', 1.0),
            image_path=data.get('image_path'),
            image_offset_x=data.get('image_offset_x', 0),
            image_offset_y=data.get('image_offset_y', 0),
            image_scale=data.get('image_scale', 1.0),
            image_z_offset=data.get('image_z_offset', 0.5),
            image_blocking=data.get('image_blocking', True)
        )
    
    def save_to_file(self, directory: str = "mesh_templates") -> Path:
        """Salva template su file JSON"""
        Path(directory).mkdir(parents=True, exist_ok=True)
        filepath = Path(directory) / f"{self.template_id}.json"
        
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(self.to_dict(), f, indent=2, ensure_ascii=False)
        
        return filepath
    
    @staticmethod
    def load_from_file(filepath: Path) -> 'MeshTemplate':
        """Carica template da file JSON"""
        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)
        return MeshTemplate.from_dict(data)


class MeshTemplateManager:
    """Gestisce collezione di mesh templates"""
    
    def __init__(self, templates_dir="mesh_templates"):
        self.templates_dir = Path(templates_dir)
        self.templates_dir.mkdir(exist_ok=True)
        self.templates = {}  # {template_id: MeshTemplate}
    
    def load_all_templates(self):
        """Carica tutti i templates dalla cartella"""
        self.templates.clear()
        
        for json_file in self.templates_dir.glob("*.json"):
            try:
                template = MeshTemplate.load_from_file(json_file)
                self.templates[template.template_id] = template
            except Exception as e:
                print(f"⚠️  Errore caricando {json_file.name}: {e}")
        
        print(f"✓ Caricati {len(self.templates)} mesh templates")
    
    def get_template(self, template_id: str) -> Optional[MeshTemplate]:
        """Ottiene template per ID"""
        return self.templates.get(template_id)
    
    def get_templates_for_section(
        self,
        biome: str,
        section_type: str,
        section_index: Optional[int]
    ) -> List[MeshTemplate]:
        """Filtra templates compatibili con bioma e sezione"""
        result = []
        for template in self.templates.values():
            if (template.biome == biome and
                template.section_type == section_type and
                template.section_index == section_index):
                result.append(template)
        return result
    
    def save_template(self, template: MeshTemplate):
        """Salva template"""
        template.save_to_file(self.templates_dir)
        self.templates[template.template_id] = template


# Istanza globale
_template_manager = None

def get_template_manager():
    """Ottiene istanza singleton del MeshTemplateManager"""
    global _template_manager
    if _template_manager is None:
        _template_manager = MeshTemplateManager()
        _template_manager.load_all_templates()
    return _template_manager
