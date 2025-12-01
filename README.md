# SnowCast

SnowCast est un pipeline de calcul des heures de lever et coucher du soleil pour des départements français, basé sur des modèles numériques d'élévation (DEM). Le projet calcule les heures solaires pour chaque pixel d'une grille raster sur une année complète, en tenant compte de la topographie locale.

## Vue d'ensemble

Le pipeline SnowCast suit ces étapes principales :

1. **Préparation des données** : extraction des DEM par département depuis un raster source
2. **Calcul solaire** : exécution d'un calculateur C++ optimisé (OpenMP) pour calculer les heures de lever/coucher du soleil
3. **Stockage** : conversion des résultats en format Parquet partitionné par département
4. **Visualisation** : génération de cartes de visualisation des heures de coucher du soleil

## Prérequis

### Système

- **Linux** (testé sur Debian/Ubuntu)
- **CMake** >= 3.15
- **Compilateur C++** avec support C++17 (g++, clang++)
- **GDAL** (bibliothèque système, généralement via `libgdal-dev`)
- **OpenMP** (pour le parallélisme)

### Python

- **Python** >= 3.8
- Les dépendances Python sont listées dans `requirements.txt`

## Installation

### 1. Installation des dépendances système

```bash
# Sur Debian/Ubuntu
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libgdal-dev \
    gdal-bin \
    libopenmpi-dev
```

### 2. Installation des dépendances Python

```bash
pip install -r requirements.txt
```

### 3. Compilation du calculateur C++

```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Le binaire `solar_calculator` sera généré dans `build/solar_calculator`.

### 4. Préparation des données d'entrée

Placez vos fichiers de données dans la structure suivante :

```
data/
├── raw/
│   ├── dem/
│   │   └── dem.tif          # Raster DEM source (format GeoTIFF)
│   └── departements/
│       └── depts.shp        # Shapefile des départements français
├── processed/               # Généré automatiquement
├── parquet/                 # Généré automatiquement
└── results/                  # Généré automatiquement
```

## Structure du projet

```
SnowCast/
├── CMakeLists.txt           # Configuration de compilation C++
├── requirements.txt          # Dépendances Python
├── README.md                 # Ce fichier
│
├── build/                    # Répertoire de compilation (généré)
│   └── solar_calculator      # Binaire C++ compilé
│
├── data/                     # Données d'entrée et de sortie
│   ├── raw/                  # Données sources (à fournir)
│   ├── processed/            # DEM découpés par département
│   ├── parquet/              # Résultats en format Parquet
│   └── results/              # Résultats GeoTIFF et visualisations
│
└── src/                      # Code source
    ├── config.py             # Configuration centralisée
    │
    ├── data/                 # Préparation des données DEM
    │   ├── process_dem.py
    │   └── extract_dem_gdalwarp.py
    │
    ├── solar/                # Calculs solaires
    │   ├── run_solar_calculation.py
    │   └── run_solar_parquet.py
    │
    ├── viz/                  # Visualisation
    │   └── visualize_sunset_map.py
    │
    ├── utils/                # Utilitaires
    │   └── inspect_parquet.py
    │
    └── *.cpp, *.h            # Code source C++ (main.cpp, ProcessDEM, SolarCalculator)
```

## Utilisation

### Workflow complet

#### 1. Préparation des DEM par département

Extrait les DEM pour chaque département cible depuis le raster source :

```bash
# Méthode 1 : Utilisation de rasterio (recommandé)
python -m src.data.process_dem

# Méthode 2 : Utilisation de gdalwarp (alternative)
python -m src.data.extract_dem_gdalwarp
```

Les fichiers `dem_dept_XX.tif` seront créés dans `data/processed/`.

#### 2. Calcul des heures solaires

Deux modes sont disponibles :

**Mode GeoTIFF** (sortie classique) :
```bash
python -m src.solar.run_solar_calculation
```

**Mode Parquet** (recommandé pour le stockage efficace) :
```bash
python -m src.solar.run_solar_parquet
```

Les paramètres de calcul (année, nombre de threads, fuseau horaire) peuvent être modifiés directement dans les scripts ou via la configuration.

#### 3. Inspection des résultats Parquet

Pour vérifier la structure et le contenu des fichiers Parquet générés :

```bash
python -m src.utils.inspect_parquet
```

#### 4. Visualisation

Génère une carte de l'heure de coucher du soleil pour un département et une date donnés :

```bash
python -m src.viz.visualize_sunset_map --dept 38 --date 2025-12-04
```

Options disponibles :
- `--dept` : Code du département (défaut: 38)
- `--date` : Date au format YYYY-MM-DD (défaut: 2025-12-04)
- `--output` : Chemin de sortie de l'image PNG (optionnel)

## Configuration

Les paramètres principaux sont définis dans `src/config.py` :

- **Départements cibles** : `TARGET_DEPARTMENTS = ["38", "73", "74"]`
- **Chemins des données** : DEM source, shapefile des départements
- **Paramètres de compression** : Pour les GeoTIFF de sortie
- **Noms des départements** : Pour les logs et messages

## Format des données

### Format Parquet

Les résultats sont stockés en format Parquet partitionné par département :

```
data/parquet/
├── dept=38/
│   ├── data.parquet         # Données (day, sunrise, sunset)
│   └── metadata.json        # Métadonnées (dimensions, géoréférencement)
├── dept=73/
│   └── ...
└── dept=74/
    └── ...
```

**Schéma Parquet** :
- `day` (int32) : Jour de l'année (1-365/366)
- `sunrise` (list[int16]) : Heures de lever en minutes après minuit (par pixel)
- `sunset` (list[int16]) : Heures de coucher en minutes après minuit (par pixel)

Les valeurs `-1` indiquent l'absence de lever/coucher (ex: nuit polaire, jour polaire).

### Format GeoTIFF

Les résultats GeoTIFF contiennent les heures solaires calculées pour chaque pixel, avec les métadonnées géographiques appropriées.

## Performance

Le calculateur C++ utilise :
- **OpenMP** pour le parallélisme multi-threads
- **Optimisations** : `-O3 -march=native`
- **Threads par défaut** : 96 (configurable dans les scripts)

Les temps de calcul dépendent de la résolution du DEM et du nombre de pixels par département.

## Dépendances Python

- `geopandas` >= 0.14.0 : Manipulation de données géospatiales
- `rasterio` >= 1.3.0 : Lecture/écriture de rasters
- `numpy` >= 1.24.0 : Calculs numériques
- `pyarrow` >= 14.0.0 : Format Parquet
- `pandas` >= 2.0.0 : Manipulation de données
- `matplotlib` >= 3.7.0 : Visualisation

## Notes techniques

- Le calculateur C++ lit les DEM au format GeoTIFF via GDAL
- Les calculs solaires prennent en compte la topographie locale (ombres portées)
- Le format Parquet permet une lecture efficace et sélective des données
- Les visualisations utilisent des projections géographiques (WGS84, EPSG:4326)

## Exemples d'utilisation

### Calcul complet pour tous les départements

```bash
# 1. Préparer les DEM
python -m src.data.process_dem

# 2. Calculer et stocker en Parquet
python -m src.solar.run_solar_parquet

# 3. Visualiser un résultat
python -m src.viz.visualize_sunset_map --dept 38 --date 2025-06-21
```

### Inspection d'un département spécifique

```bash
# Vérifier les données Parquet
python -c "from src.utils.inspect_parquet import inspect_parquet; inspect_parquet('38')"
```

## Sources des données et licences

- **Modèle numérique d'élévation (DEM)**  
  - Les DEM utilisés dans ce projet proviennent du programme Copernicus.  
  - Données Copernicus (Copernicus DEM), distribuées par l'Agence Spatiale Européenne (ESA) au nom de l'Union européenne.  
  - Toute réutilisation doit respecter les termes de la licence Copernicus.

- **Limites de pays**  
  - Les limites de pays (couches `countries`) proviennent de Natural Earth.  
  - Natural Earth est un jeu de données de domaine public : *\"Free vector and raster map data @ naturalearthdata.com\"*.

- **Limites de départements français**  
  - Les limites administratives des départements proviennent des données IGN.
  - Toute réutilisation doit respecter les termes de la licence IGN.
