import re
import os
from pathlib import Path
import subprocess
import shutil

# Flags de contrôle
ENABLE_PARSING = True  # Mettre à False pour désactiver la mise à jour du code
ENABLE_BUILD = True    # Mettre à False pour désactiver la compilation CMake

def extract_section(file_path, start_marker, end_marker):
    """Extrait une section de code entre deux marqueurs."""
    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            content = file.read()
            pattern = f"{start_marker}\n(.*?)\n[^\\n]*{end_marker}"
            match = re.search(pattern, content, re.DOTALL)
            if match:
                return match.group(1).rstrip()
            return None
    except FileNotFoundError:
        print(f"Erreur: Fichier non trouvé - {file_path}")
        return None
    except Exception as e:
        print(f"Erreur lors de la lecture de {file_path}: {str(e)}")
        return None

def clean_copied_code(code):
    """Nettoie le code copié pour le rendre compatible avec gen.cpp."""
    # Remplace [this] par [] dans les lambdas
    code = re.sub(r'\[this\]', '[]', code)
    
    # Remplace le contenu des lambdas par un simple return true
    code = re.sub(
        r'(\[[^\]]*\]\([^)]*\)\s*{)\s*(.*?)\s*}(?=\s*\))',
        r'\1 return true; }',
        code,
        flags=re.DOTALL  # Important pour matcher les sauts de ligne
    )
    
    return code

def update_gen_cpp(gen_cpp_path, sections):
    """Met à jour le fichier gen.cpp avec les sections extraites."""
    try:
        with open(gen_cpp_path, 'r', encoding='utf-8') as file:
            content = file.read()

        for marker, section in sections.items():
            if section:
                # Nettoie le code avant de l'insérer
                cleaned_section = clean_copied_code(section)
                pattern = f"{marker}_START\n(.*?)\n[^\\n]*{marker}_END"
                replacement = f"{marker}_START\n{cleaned_section}\n    //{marker}_END"
                content = re.sub(pattern, replacement, content, flags=re.DOTALL)

        with open(gen_cpp_path, 'w', encoding='utf-8') as file:
            file.write(content)
            
        print(f"Le fichier {gen_cpp_path} a été mis à jour avec succès.")
        
    except Exception as e:
        print(f"Erreur lors de la mise à jour de gen.cpp: {str(e)}")

def run_cmake_build(tools_dir):
    """Configure et build avec CMake."""
    build_dir = tools_dir / "build"
    
    # Supprime le dossier build s'il existe
    if build_dir.exists():
        shutil.rmtree(build_dir)
    
    # Créer le dossier build
    build_dir.mkdir()
    
    try:
        print("\nConfiguration CMake...")
        process = subprocess.Popen(
            ['cmake', '..'],
            cwd=str(build_dir),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True
        )
        
        for line in process.stdout:
            print(line, end='')
        process.wait()
        if process.returncode != 0:
            return False
            
        print("\nBuild avec CMake...")
        process = subprocess.Popen(
            ['cmake', '--build', '.'],
            cwd=str(build_dir),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True
        )
        
        for line in process.stdout:
            print(line, end='')
        process.wait()
        if process.returncode != 0:
            return False

        print("\nExécution de gen...")
        process = subprocess.Popen(
            ['./gen'],
            cwd=str(build_dir),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True
        )
        
        for line in process.stdout:
            print(line, end='')
        process.wait()
        if process.returncode != 0:
            return False
            
        print("\nNettoyage du dossier build...")
        # Supprime le dossier build
        shutil.rmtree(build_dir)
            
        print("Build et génération terminés avec succès")
        return True
        
    except FileNotFoundError:
        print("Erreur: cmake n'est pas installé ou n'est pas dans le PATH")
        return False
    except Exception as e:
        print(f"Erreur inattendue: {str(e)}")
        return False

def main():
    # Obtention du chemin du répertoire racine (parent de /tools)
    root_path = Path(__file__).parent.parent
    tools_path = root_path / "tools"

    if ENABLE_PARSING:
        # Définition des chemins relatifs à la racine
        gen_cpp_path = tools_path / "gen.cpp"
        main_cpp_path = root_path / "src" / "main.cpp"
        wifi_manager_api_path = root_path / "lib" / "WiFiManager" / "src" / "WiFiManagerAPI.h"

        print(f"Traitement des fichiers :")
        print(f"- Main CPP: {main_cpp_path}")
        print(f"- WiFiManagerAPI: {wifi_manager_api_path}")
        print(f"- Gen CPP: {gen_cpp_path}")

        # Extraction des sections
        sections = {
            "@MAIN_COPY": extract_section(
                main_cpp_path,
                "//@API_DOC_SECTION_START",
                "//@API_DOC_SECTION_END"
            ),
            "@APIMODULE_COPY": extract_section(
                wifi_manager_api_path,
                "//@API_DOC_SECTION_START",
                "//@API_DOC_SECTION_END"
            )
        }

        # Vérification des sections extraites
        for marker, content in sections.items():
            if content is None:
                print(f"Attention: La section {marker} n'a pas pu être extraite")
            else:
                print(f"Section {marker} extraite avec succès ({len(content)} caractères)")

        # Mise à jour de gen.cpp
        update_gen_cpp(gen_cpp_path, sections)

    # Build avec CMake
    if ENABLE_BUILD:
        if not run_cmake_build(tools_path):
            exit(1)

if __name__ == "__main__":
    main()
