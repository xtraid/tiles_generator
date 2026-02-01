#!/usr/bin/env python3
"""Script per creare props usando immagini PNG"""

from prop import Prop
from pathlib import Path

def create_image_props():
    """Crea props che usano immagini dalla cartella assets/"""
    
    props_dir = Path("props")
    assets_dir = Path("assets")
    
    # Crea cartella assets se non esiste
    assets_dir.mkdir(exist_ok=True)
    
    print("=" * 60)
    print("CREAZIONE PROPS CON IMMAGINI")
    print("=" * 60)
    print(f"\n📁 Cartella immagini: {assets_dir.absolute()}")
    print("\n⚠️  IMPORTANTE: Metti i tuoi file PNG nella cartella 'assets/'")
    print("    Esempi: assets/tree.png, assets/house.png, etc.\n")
    
    # ========== PROPS CENTRO ==========
    
    # 1. Albero (centro)
    tree = Prop(
        prop_id="tree_png_01",
        name="Albero PNG",
        section_type="center",
        section_index=None,
        visual={
            'type': 'image',
            'path': 'assets/tree.png',
            'scale': 1.0  # Dimensione originale
        },
        blocking=True,
        z_offset=0.5
    )
    tree.save_to_file(props_dir)
    print(f"✓ Creato: {tree.name} (centro)")
    
    # 2. Casa (centro)
    house = Prop(
        prop_id="house_png_01",
        name="Casa PNG",
        section_type="center",
        section_index=None,
        visual={
            'type': 'image',
            'path': 'assets/house.png',
            'scale': 1.2  # 120% più grande
        },
        blocking=True,
        z_offset=0.8
    )
    house.save_to_file(props_dir)
    print(f"✓ Creato: {house.name} (centro)")
    
    # 3. Roccia (centro)
    rock = Prop(
        prop_id="rock_png_01",
        name="Roccia PNG",
        section_type="center",
        section_index=None,
        visual={
            'type': 'image',
            'path': 'assets/rock.png',
            'scale': 0.8  # 80% più piccola
        },
        blocking=True,
        z_offset=0.2
    )
    rock.save_to_file(props_dir)
    print(f"✓ Creato: {rock.name} (centro)")
    
    # 4. Fiore (centro, non bloccante)
    flower = Prop(
        prop_id="flower_png_01",
        name="Fiore PNG",
        section_type="center",
        section_index=None,
        visual={
            'type': 'image',
            'path': 'assets/flower.png',
            'scale': 0.6
        },
        blocking=False,
        z_offset=0.1
    )
    flower.save_to_file(props_dir)
    print(f"✓ Creato: {flower.name} (centro)")
    
    # 5. Personaggio (centro, non bloccante)
    character = Prop(
        prop_id="character_png_01",
        name="Personaggio PNG",
        section_type="center",
        section_index=None,
        visual={
            'type': 'image',
            'path': 'assets/character.png',
            'scale': 1.0
        },
        blocking=False,
        z_offset=0.3
    )
    character.save_to_file(props_dir)
    print(f"✓ Creato: {character.name} (centro)")
    
    # ========== PROPS WEDGE (trapezi) ==========
    
    # 6. Muro (wedge - sui lati del trapezio)
    wall = Prop(
        prop_id="wall_png_01",
        name="Muro PNG",
        section_type="wedge",
        section_index=0,  # Wedge 0 (puoi cambiare con TAB)
        visual={
            'type': 'image',
            'path': 'assets/wall.png',
            'scale': 0.7
        },
        blocking=True,
        z_offset=0.4
    )
    wall.save_to_file(props_dir)
    print(f"✓ Creato: {wall.name} (wedge - centrato sul trapezio)")
    
    # 7. Recinzione (wedge)
    fence = Prop(
        prop_id="fence_png_01",
        name="Recinzione PNG",
        section_type="wedge",
        section_index=0,
        visual={
            'type': 'image',
            'path': 'assets/fence.png',
            'scale': 1.0
        },
        blocking=True,
        z_offset=0.3
    )
    fence.save_to_file(props_dir)
    print(f"✓ Creato: {fence.name} (wedge - centrato sul trapezio)")
    
    print("\n" + "=" * 60)
    print(f"✓ Creati 7 props con immagini in '{props_dir}/'")
    print("=" * 60)
    print("\n📋 PROSSIMI PASSI:")
    print("1. Metti i tuoi file PNG nella cartella 'assets/'")
    print("   - tree.png, house.png, rock.png, etc.")
    print("2. Esegui: python hex_tiles_main.py")
    print("3. Premi P per piazzare i props!")
    print("\n💡 TIP: Se manca un'immagine, vedrai una X rossa come placeholder")


if __name__ == "__main__":
    create_image_props()
    