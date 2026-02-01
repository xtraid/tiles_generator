#!/usr/bin/env python3
"""Script per creare alcuni props di esempio"""

from prop import Prop
from pathlib import Path

def create_example_props():
    """Crea una collezione di props di esempio"""
    
    props_dir = Path("props")
    props_dir.mkdir(exist_ok=True)
    
    # 1. Albero (centro)
    tree = Prop(
        prop_id="tree_pine_01",
        name="Pino",
        section_type="center",
        section_index=None,
        visual={'type': 'circle', 'color': [34, 139, 34], 'radius': 18},
        blocking=True,
        z_offset=0.5
    )
    tree.save_to_file(props_dir)
    print(f"✓ Creato: {tree.name}")
    
    # 2. Roccia piccola (centro)
    rock_small = Prop(
        prop_id="rock_small_01",
        name="Roccia Piccola",
        section_type="center",
        section_index=None,
        visual={'type': 'circle', 'color': [105, 105, 105], 'radius': 12},
        blocking=True,
        z_offset=0.2
    )
    rock_small.save_to_file(props_dir)
    print(f"✓ Creato: {rock_small.name}")
    
    # 3. Casa (centro, grande)
    house = Prop(
        prop_id="house_wood_01",
        name="Casa di Legno",
        section_type="center",
        section_index=None,
        visual={'type': 'rect', 'color': [139, 69, 19], 'width': 30, 'height': 30},
        blocking=True,
        z_offset=0.8
    )
    house.save_to_file(props_dir)
    print(f"✓ Creato: {house.name}")
    
    # 4. Fiore (non bloccante, centro)
    flower = Prop(
        prop_id="flower_01",
        name="Fiore",
        section_type="center",
        section_index=None,
        visual={'type': 'circle', 'color': [255, 192, 203], 'radius': 8},
        blocking=False,
        z_offset=0.1
    )
    flower.save_to_file(props_dir)
    print(f"✓ Creato: {flower.name}")
    
    # 5. Recinzione (wedge - può essere piazzata sui lati)
    fence = Prop(
        prop_id="fence_wood_01",
        name="Recinzione",
        section_type="wedge",
        section_index=0,  # Default, può essere cambiato
        visual={'type': 'rect', 'color': [139, 90, 43], 'width': 25, 'height': 8},
        blocking=True,
        z_offset=0.3
    )
    fence.save_to_file(props_dir)
    print(f"✓ Creato: {fence.name}")
    
    # 6. Pietra preziosa (non bloccante, decorativa)
    gem = Prop(
        prop_id="gem_blue_01",
        name="Gemma Blu",
        section_type="center",
        section_index=None,
        visual={'type': 'triangle', 'color': [0, 191, 255], 'size': 12},
        blocking=False,
        z_offset=0.15
    )
    gem.save_to_file(props_dir)
    print(f"✓ Creato: {gem.name}")
    
    # 7. Cespuglio (wedge)
    bush = Prop(
        prop_id="bush_01",
        name="Cespuglio",
        section_type="wedge",
        section_index=0,
        visual={'type': 'circle', 'color': [50, 150, 50], 'radius': 14},
        blocking=False,
        z_offset=0.2
    )
    bush.save_to_file(props_dir)
    print(f"✓ Creato: {bush.name}")
    
    # 8. Torcia (centro, alta)
    torch = Prop(
        prop_id="torch_01",
        name="Torcia",
        section_type="center",
        section_index=None,
        visual={'type': 'triangle', 'color': [255, 140, 0], 'size': 10},
        blocking=False,
        z_offset=1.0
    )
    torch.save_to_file(props_dir)
    print(f"✓ Creato: {torch.name}")
    
    print(f"\n✓ Creati 8 props di esempio in '{props_dir}/'")
    print("Esegui hex_tiles_main.py per usarli!")


if __name__ == "__main__":
    create_example_props()