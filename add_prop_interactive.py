#!/usr/bin/env python3
"""Aggiungi un prop in modo interattivo - 7 sezioni"""

from prop import Prop
from pathlib import Path

def list_available_images():
    """Mostra immagini disponibili in assets/"""
    assets = Path("assets")
    images = list(assets.glob("*.png")) + list(assets.glob("*.jpg"))
    return [img.name for img in images]

def get_int_input(prompt, min_val, max_val, default=None):
    """Chiede un numero con validazione"""
    while True:
        try:
            value = input(prompt).strip()
            
            # Se vuoto e c'è un default
            if not value and default is not None:
                return default
            
            # Converti a int
            num = int(value)
            
            # Verifica range
            if min_val <= num <= max_val:
                return num
            else:
                print(f"❌ Inserisci un numero tra {min_val} e {max_val}")
        except ValueError:
            print(f"❌ Inserisci un numero valido (tra {min_val} e {max_val})")

def get_float_input(prompt, default):
    """Chiede un float con validazione"""
    while True:
        try:
            value = input(prompt).strip()
            
            # Se vuoto usa default
            if not value:
                return default
            
            return float(value)
        except ValueError:
            print(f"❌ Inserisci un numero decimale valido")

def add_prop_interactive():
    print("=" * 60)
    print("AGGIUNGI NUOVO PROP - Modalità Interattiva")
    print("=" * 60)
    print()
    
    # Mostra immagini disponibili
    images = list_available_images()
    if not images:
        print("❌ Nessuna immagine trovata in assets/")
        print("   Metti prima le immagini PNG in assets/")
        return
    
    print("📁 Immagini disponibili in assets/:")
    for i, img in enumerate(images, 1):
        print(f"   {i}. {img}")
    print()
    
    # Input utente con validazione
    prop_id = input("ID del prop (es: goblin_01): ").strip()
    if not prop_id:
        print("❌ ID obbligatorio!")
        return
    
    name = input("Nome descrittivo (es: Goblin): ").strip()
    if not name:
        name = prop_id  # Usa l'ID se non specificato
    
    # Scegli immagine con validazione
    img_choice = get_int_input(
        f"Scegli immagine (1-{len(images)}): ", 
        1, len(images)
    ) - 1
    image_file = images[img_choice]
    
    # ========== SCEGLI SEZIONE (7 OPZIONI) ==========
    print("\n📍 Sezione (dove posizionare il prop):")
    print("   1. Centro (esagono centrale)")
    print("   2. Wedge 0 (trapezio E - Est)")
    print("   3. Wedge 1 (trapezio NE - Nord-Est)")
    print("   4. Wedge 2 (trapezio NW - Nord-Ovest)")
    print("   5. Wedge 3 (trapezio W - Ovest)")
    print("   6. Wedge 4 (trapezio SW - Sud-Ovest)")
    print("   7. Wedge 5 (trapezio SE - Sud-Est)")
    
    section_choice = get_int_input("Scegli sezione (1-7) [default 1]: ", 1, 7, default=1)
    
    # Converti scelta in section_type e section_index
    if section_choice == 1:
        section_type = "center"
        section_index = None
        section_name = "Centro"
    else:
        section_type = "wedge"
        section_index = section_choice - 2  # 2→0, 3→1, 4→2, 5→3, 6→4, 7→5
        direction_names = ["E", "NE", "NW", "W", "SW", "SE"]
        section_name = f"Wedge {section_index} ({direction_names[section_index]})"
    
    # Bloccante?
    blocking_choice = input("\nBloccante? (s/n) [default s]: ").strip().lower()
    blocking = blocking_choice != 'n'  # Default = True
    
    # Scala
    scale = get_float_input(
        "Scala (1.0 = normale, 0.5 = metà, 2.0 = doppia) [default 1.0]: ",
        default=1.0
    )
    
    # Z offset
    z_offset = get_float_input(
        "Altezza Z (0.0-1.0) [default 0.3]: ",
        default=0.3
    )
    
    # Crea il prop
    print()
    print("Creazione prop...")
    
    try:
        prop = Prop(
            prop_id=prop_id,
            name=name,
            section_type=section_type,
            section_index=section_index,
            visual={
                'type': 'image',
                'path': f"assets/{image_file}",
                'scale': scale
            },
            blocking=blocking,
            z_offset=z_offset
        )
        
        prop.save_to_file("props")
        
        print()
        print("=" * 60)
        print(f"✓ Creato: {name}")
        print(f"   ID: {prop_id}")
        print(f"   File: props/{prop_id}.json")
        print(f"   Immagine: assets/{image_file}")
        print(f"   Sezione: {section_name}")
        print(f"   Bloccante: {'Sì' if blocking else 'No'}")
        print(f"   Scala: {scale}x")
        print(f"   Z-offset: {z_offset}")
        print("=" * 60)
        print()
        print("🎮 Lancia il gioco e premi P per vederlo!")
        print("   Usa TAB per selezionare la sezione corretta prima di piazzarlo")
    
    except Exception as e:
        print()
        print(f"❌ ERRORE durante la creazione: {e}")

if __name__ == "__main__":
    try:
        add_prop_interactive()
    except KeyboardInterrupt:
        print("\n\n❌ Operazione annullata")
    except Exception as e:
        print(f"\n❌ Errore imprevisto: {e}")