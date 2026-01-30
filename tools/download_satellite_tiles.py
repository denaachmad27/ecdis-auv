#!/usr/bin/env python3
"""
Satellite Tile Downloader for ECDIS AUV
Downloads ESRI World Imagery tiles for offline use
"""

import os
import requests
import math
from pathlib import Path

# Configuration
# Area around Surabaya, Selat Madura, Indonesia (expanded coverage)
MIN_LAT = -7.3
MAX_LAT = -7.0
MIN_LON = 112.6
MAX_LON = 112.9

# Zoom levels to download (0-16, higher = more detail)
# Low zoom (0-7): Regional overview
# Medium zoom (8-12): City level
# High zoom (13-16): Street level detail
MIN_ZOOM = 0
MAX_ZOOM = 16

# Output directory
OUTPUT_DIR = "../tiles"  # Relative to script location

# ESRI World Imagery URL
TILE_URL = "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"

def lon_to_tile_x(lon, zoom):
    """Convert longitude to tile X coordinate"""
    return int((lon + 180.0) / 360.0 * (1 << zoom))

def lat_to_tile_y(lat, zoom):
    """Convert latitude to tile Y coordinate"""
    lat_rad = math.radians(lat)
    return int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * (1 << zoom))

def download_tile(x, y, z, output_dir, session):
    """Download a single tile"""
    tile_dir = os.path.join(output_dir, str(z))
    os.makedirs(tile_dir, exist_ok=True)

    tile_path = os.path.join(tile_dir, f"{x}_{y}.png")

    # Skip if already exists
    if os.path.exists(tile_path):
        return True

    url = TILE_URL.format(z=z, y=y, x=x)

    try:
        response = session.get(url, timeout=30)
        if response.status_code == 200:
            with open(tile_path, 'wb') as f:
                f.write(response.content)
            print(f"Downloaded: {tile_path}")
            return True
        else:
            print(f"Failed {url}: HTTP {response.status_code}")
            return False
    except Exception as e:
        print(f"Error downloading {url}: {e}")
        return False

def main():
    """Main download function"""
    # Get script directory
    script_dir = Path(__file__).parent.resolve()
    output_path = script_dir / OUTPUT_DIR
    output_path.mkdir(parents=True, exist_ok=True)

    print(f"Downloading satellite tiles to: {output_path}")
    print(f"Area: lat [{MIN_LAT}, {MAX_LAT}], lon [{MIN_LON}, {MAX_LON}]")
    print(f"Zoom levels: {MIN_ZOOM}-{MAX_ZOOM}")

    session = requests.Session()
    session.headers.update({
        'User-Agent': 'ECDIS-AUV/1.0'
    })

    total_tiles = 0
    downloaded = 0

    # Calculate tiles for each zoom level
    for zoom in range(MIN_ZOOM, MAX_ZOOM + 1):
        start_x = lon_to_tile_x(MIN_LON, zoom)
        end_x = lon_to_tile_x(MAX_LON, zoom)
        start_y = lat_to_tile_y(MAX_LAT, zoom)  # Note: y is inverted for tiles
        end_y = lat_to_tile_y(MIN_LAT, zoom)

        # Clamp to valid range
        max_tile = 1 << zoom
        start_x = max(0, start_x)
        end_x = min(max_tile - 1, end_x)
        start_y = max(0, start_y)
        end_y = min(max_tile - 1, end_y)

        zoom_tiles = (end_x - start_x + 1) * (end_y - start_y + 1)
        total_tiles += zoom_tiles

        print(f"\nZoom {zoom}: {start_x}-{end_x}, {start_y}-{end_y} = {zoom_tiles} tiles")

        for x in range(start_x, end_x + 1):
            for y in range(start_y, end_y + 1):
                if download_tile(x, y, zoom, str(output_path), session):
                    downloaded += 1

    print(f"\nComplete! Downloaded {downloaded}/{total_tiles} tiles")
    print(f"\nTo use: Copy the 'tiles' folder to your exe directory.")

if __name__ == "__main__":
    main()
