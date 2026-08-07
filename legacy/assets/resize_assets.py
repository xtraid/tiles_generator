#!/usr/bin/env python3
"""Ridimensiona tutte le immagini in assets/ a 64x64 pixel"""

from pathlib import Path
from PIL import Image
import shutil

def resize_all_images(target_size=(64, 64), backup=True):
    """
    Ridimensiona tutte le PNG in assets/ a 64x64
    
    Args:
        target_size: tupla (width, height) - default (64, 64)
        backup: se True, crea backup degli originali
    """
    assets_dir = Path("assets")
    
    # Crea cartella assets se non esiste
    if not assets_dir.exists():
        print("❌ Cartella assets/ non trovata!")
        assets_dir.mkdir()
        print("✓ Creata cartella assets/")
        return
    
    # Trova tutte le immagini PNG
    images = list(assets_dir.glob("*.png")) + list(assets_dir.glob("*.jpg")) + list(assets_dir.glob("*.jpeg"))
    
    if not images:
        print("❌ Nessuna immagine trovata in assets/")
        return
    
    print("=" * 60)
    print(f"RIDIMENSIONAMENTO IMMAGINI → {target_size[0]}x{target_size[1]}")
    print("=" * 60)
    print(f"\nTrovate {len(images)} immagini\n")
    
    # Crea cartella backup se richiesto
    if backup:
        backup_dir = assets_dir / "originals"
        backup_dir.mkdir(exist_ok=True)
        print(f"📁 Backup originali in: {backup_dir}/\n")
    
    success = 0
    errors = 0
    
    for img_path in images:
        try:
            # Apri immagine
            img = Image.open(img_path)
            original_size = img.size
            
            # Backup dell'originale
            if backup:
                backup_path = backup_dir / img_path.name
                if not backup_path.exists():  # Non sovrascrivere backup esistenti
                    shutil.copy2(img_path, backup_path)
            
            # Ridimensiona mantenendo trasparenza
            if img.mode == 'RGBA':
                # Mantieni canale alpha per trasparenza
                img_resized = img.resize(target_size, Image.Resampling.LANCZOS)
            else:
                # Converti a RGBA se non lo è già
                img = img.convert('RGBA')
                img_resized = img.resize(target_size, Image.Resampling.LANCZOS)
            
            # Salva immagine ridimensionata
            img_resized.save(img_path, 'PNG')
            
            print(f"✓ {img_path.name:30} {original_size[0]:4}x{original_size[1]:<4} → {target_size[0]}x{target_size[1]}")
            success += 1
            
        except Exception as e:
            print(f"✗ {img_path.name:30} ERRORE: {e}")
            errors += 1
    
    print("\n" + "=" * 60)
    print(f"✓ Completato: {success} immagini ridimensionate")
    if errors > 0:
        print(f"✗ Errori: {errors}")
    if backup:
        print(f"📁 Originali salvati in: {backup_dir}/")
    print("=" * 60)


if __name__ == "__main__":
    # Ridimensiona tutto a 64x64 con backup
    resize_all_images(target_size=(64, 64), backup=True)
    
    # Per ridimensionare a dimensioni diverse:
    # resize_all_images(target_size=(128, 128), backup=True)
    
    # Per NON fare backup (ATTENZIONE: sovrascrive originali!):
    # resize_all_images(target_size=(64, 64), backup=False)