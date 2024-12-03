from pathlib import Path
import re
import json
import yaml

class APIParser:
   def __init__(self, repo_root: str):
       self.repo_root = Path(repo_root)
       self.api_info = {}
       self.modules = {}

   def parse(self):
       """Parse tous les fichiers pour extraire la structure de l'API"""
       print("Parsing API info from main.cpp...")
       self.parse_main()
       print("Scanning for API modules...")
       self.parse_modules() 
       return self

   def parse_main(self):
        """Extrait APIInfo depuis main.cpp"""
        main_file = self.repo_root / "src" / "main.cpp"
        print(f"Reading {main_file}...")
        
        content = main_file.read_text()
        print("Content read, searching for APIServer declaration...")
        
        # Nouvelle regex avec re.DOTALL
        match = re.search(
        r'APIServer\s+apiServer\s*\(\s*{[^}]*?"([^"]+)"[^}]*?"([^"]+)"[^}]*?"([^"]+)"[^}]*?}\s*\)',
        content,
        re.DOTALL
        )
        
        if match:
            self.api_info = {
                "title": match.group(1),
                "version": match.group(2),
                "description": match.group(3)
            }
            print(f"Found API info: {self.api_info}")
        else:
            print("No API info found in main.cpp!")
            # Show context
            server_index = content.find("APIServer")
            if server_index != -1:
                context = content[server_index:server_index+500].replace('\n', '\\n')
                print(f"Context: {context}")

   def parse_modules(self):
        """Parse tous les fichiers pour trouver les modules API"""
        print("\nScanning for API modules in lib/...")
        for file in (self.repo_root / "lib").rglob("*.h"):
            print(f"Checking {file}...")
            if self.is_api_module(file):
                print(f"Found API module in {file}")
                module = self.parse_module(file)
                if module:
                    self.modules[module["name"]] = module
                    print(f"Parsed module: {module}")

   def is_api_module(self, file_path: Path) -> bool:
        """Vérifie si le fichier est un module API"""
        content = file_path.read_text()
        # Afficher le contenu si on trouve registerModule
        if content.find("registerModule") != -1:
            print(f"\nFound registerModule in {file_path}:")
            index = content.find("registerModule")
            print(content[index-50:index+100])
        return bool(re.search(r'registerModule\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"', content))

   def parse_module(self, file_path: Path):
       """Parse un fichier de module API"""
       content = file_path.read_text()
       
       # Cherche registerModule(...)
       module_match = re.search(
           r'registerModule\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"',
           content
       )
       if not module_match:
           return None

       module = {
           "name": module_match.group(1),
           "version": module_match.group(2),
           "description": module_match.group(3),
           "routes": []
       }

       # Parse toutes les routes du module
       route_matches = re.finditer(
            r'registerMethod\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"',
            content
       )

       for match in route_matches:
           path = match.group(2)  # La route complète
           module["routes"].append(path)

       return module

class OpenAPIGenerator:
   def __init__(self, parser: APIParser, output_dir: str):
       self.parser = parser
       self.output_dir = Path(output_dir)
       self.output_dir.mkdir(exist_ok=True)

   def generate(self):
       """Génère les fichiers openapi.json et openapi.yaml"""
       openapi = self.build_structure()
       self.save_json(openapi)
       self.save_yaml(openapi)
       print(f"OpenAPI files generated in {self.output_dir}")

   def build_structure(self):
       """Construit la structure OpenAPI"""
       return {
           "openapi": "3.1.1",
           "info": self.parser.api_info,
           "servers": [{"url": "/"}],
           "tags": self.build_tags(),
           "paths": self.build_paths()
       }

   def build_tags(self):
       """Construit la section tags"""
       return [{
           "name": name,
           "description": f"{module['description']} (v{module['version']})"
       } for name, module in self.parser.modules.items()]

   def build_paths(self):
       """Construit la section paths"""
       paths = {}
       for module in self.parser.modules.values():
           for route in module["routes"]:
               paths[f"/{route}"] = {
                   "get": {
                       "tags": [module["name"]],
                       "summary": f"Route {route}",
                       "responses": {
                           "200": {
                               "description": "Successful operation"
                           }
                       }
                   }
               }
       return paths

   def save_json(self, openapi: dict):
       """Sauvegarde en JSON"""
       json_file = self.output_dir / "openapi.json"
       json_file.write_text(
           json.dumps(openapi, indent=2)
       )

   def save_yaml(self, openapi: dict):
       """Sauvegarde en YAML"""
       yaml_file = self.output_dir / "openapi.yaml"
       yaml_file.write_text(
           yaml.dump(openapi, sort_keys=False)
       )

def main():
   # Parse la structure de l'API
   parser = APIParser(".").parse()
   
   # Génère la documentation
   generator = OpenAPIGenerator(parser, "data")
   generator.generate()

if __name__ == "__main__":
   main()