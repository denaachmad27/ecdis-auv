#!/usr/bin/env python3
"""
Rasterize Shapefile to Tiles

Converts shapefiles to raster tiles (PNG) for display in ECDIS.
Similar structure to satellite tiles: tiles/{layer}/{z}/{x}_{y}.png

Requirements:
    pip install gdal numpy pillow

Usage:
    python rasterize_shapefile_tiles.py --input "shp/KOTA SURABAYA" --output "thematic-tiles"
    python rasterize_shapefile_tiles.py --layer "SUNGAI_AR_25K.shp" --min-zoom 10 --max-zoom 16
"""

import os
import sys
import argparse
import math
from pathlib import Path

try:
    from osgeo import gdal, ogr, osr
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont
except ImportError as e:
    print(f"Error: Missing required library: {e}")
    print("Install with: pip install gdal numpy pillow")
    sys.exit(1)

# Enable GDAL exceptions
gdal.UseExceptions()


# Web Mercator tile system constants
WEB_MERCATOR_RADIUS = 6378137.0
WEB_MERCATOR_EXTENT = 20037508.34
TILE_SIZE = 256


def lon_to_tile_x(lon, zoom):
    """Convert longitude to tile X coordinate."""
    return int((lon + 180.0) / 360.0 * (1 << zoom))


def lat_to_tile_y(lat, zoom):
    """Convert latitude to tile Y coordinate."""
    lat_rad = lat * math.pi / 180.0
    return int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * (1 << zoom))


def tile_x_to_lon(x, zoom):
    """Convert tile X coordinate to longitude."""
    return x / (1 << zoom) * 360.0 - 180.0


def tile_y_to_lat(y, zoom):
    """Convert tile Y coordinate to latitude."""
    n = math.pi - 2.0 * math.pi * y / (1 << zoom)
    return 180.0 / math.pi * math.atan(0.5 * (math.exp(n) - math.exp(-n)))


def tile_to_webmercator_bounds(x, y, zoom):
    """Get Web Mercator bounds of a tile."""
    tile_size_meters = (WEB_MERCATOR_EXTENT * 2.0) / (1 << zoom)

    min_x = -WEB_MERCATOR_EXTENT + x * tile_size_meters
    max_x = -WEB_MERCATOR_EXTENT + (x + 1) * tile_size_meters
    min_y = WEB_MERCATOR_EXTENT - (y + 1) * tile_size_meters
    max_y = WEB_MERCATOR_EXTENT - y * tile_size_meters

    return min_x, max_x, min_y, max_y


def webmercator_to_geographic(x, y):
    """Convert Web Mercator to lat/lon."""
    lon = x / WEB_MERCATOR_EXTENT * 180.0
    y_rad = y / WEB_MERCATOR_EXTENT * math.pi
    lat = (2.0 * math.atan(math.exp(y_rad)) - math.pi / 2.0) * 180.0 / math.pi
    return lat, lon


def get_shapefile_bounds(shapefile_path):
    """Get the bounding box of a shapefile in lat/lon."""
    data_source = ogr.Open(shapefile_path)
    if not data_source:
        raise ValueError(f"Cannot open shapefile: {shapefile_path}")

    layer = data_source.GetLayer()
    extent = layer.GetExtent()

    # extent is in min_x, max_x, min_y, max_y (projected coordinates)
    # For WGS84 shapefiles, this is already lon/lat
    min_lon, max_lon, min_lat, max_lat = extent

    data_source = None
    return min_lon, max_lon, min_lat, max_lat


def get_layer_color(layer_name):
    """Get color for a thematic layer based on its name."""
    layer_name_upper = layer_name.upper()

    # Water features - blue
    if any(x in layer_name_upper for x in ['SUNGAI', 'DANAU', 'EMPANG', 'RAWA', 'PESISIR', 'PANTAI']):
        return (30, 144, 255, 200)  # Blue

    # Administrative - red/brown
    if any(x in layer_name_upper for x in ['ADMINISTRASI', 'BATAS', 'DESA']):
        return (200, 50, 50, 180)  # Red

    # Built-up areas - gray
    if any(x in layer_name_upper for x in ['BANGUNAN', 'PEMUKIMAN', 'PERUMAHAN']):
        return (128, 128, 128, 150)  # Gray

    # Roads - dark gray
    if any(x in layer_name_upper for x in ['JALAN', 'RELKA', 'JEMBATAN']):
        return (80, 80, 80, 200)  # Dark gray

    # Ports/harbor - purple
    if any(x in layer_name_upper for x in ['PELABUHAN', 'DERMAGA', 'TAMBANGAN']):
        return (128, 0, 128, 180)  # Purple

    # Agriculture - green
    if any(x in layer_name_upper for x in ['AGRI', 'KEBUN', 'LADANG', 'SAWAH', 'TANAM']):
        return (34, 139, 34, 150)  # Green

    # Forest - dark green
    if 'HUTAN' in layer_name_upper or 'BELUKAR' in layer_name_upper:
        return (0, 100, 0, 150)  # Dark green

    # Facilities - orange
    if any(x in layer_name_upper for x in ['KANTOR', 'SEKOLAH', 'PENDIDIKAN', 'KESEHATAN',
                                            'NIAGA', 'PASAR', 'POS', 'LISTRIK']):
        return (255, 140, 0, 180)  # Orange

    # Special features
    if any(x in layer_name_upper for x in ['MAKAM', 'CAGAR', 'BUDAYA', 'MENARA']):
        return (139, 69, 19, 180)  # Brown

    # Default color
    return (100, 100, 100, 150)


def rasterize_shapefile_to_tiles(shapefile_path, output_dir, layer_name=None,
                                  min_zoom=10, max_zoom=16, max_tiles_per_zoom=1000):
    """
    Rasterize a shapefile to tiles.

    Args:
        shapefile_path: Path to .shp file
        output_dir: Output directory for tiles
        layer_name: Name of the layer (default: derived from filename)
        min_zoom: Minimum zoom level
        max_zoom: Maximum zoom level
        max_tiles_per_zoom: Safety limit for tiles per zoom level
    """
    shapefile_path = Path(shapefile_path).resolve()
    if not shapefile_path.exists():
        raise FileNotFoundError(f"Shapefile not found: {shapefile_path}")

    if layer_name is None:
        layer_name = shapefile_path.stem

    print(f"Processing: {shapefile_path.name}")
    print(f"Layer: {layer_name}")
    print(f"Zoom range: {min_zoom}-{max_zoom}")

    # Get shapefile bounds
    min_lon, max_lon, min_lat, max_lat = get_shapefile_bounds(str(shapefile_path))
    print(f"Bounds: [{min_lon:.4f}, {max_lon:.4f}] x [{min_lat:.4f}, {max_lat:.4f}]")

    # Get color for this layer
    color = get_layer_color(layer_name)
    print(f"Color: RGBA{color}")

    # Open shapefile
    data_source = ogr.Open(str(shapefile_path))
    if not data_source:
        raise ValueError(f"Cannot open shapefile: {shapefile_path}")

    layer = data_source.GetLayer()
    geom_type = ogr.GeometryTypeToName(layer.GetGeomType())
    print(f"Geometry type: {geom_type}")
    print(f"Feature count: {layer.GetFeatureCount()}")

    # Create output directory structure
    layer_output_dir = Path(output_dir) / layer_name
    layer_output_dir.mkdir(parents=True, exist_ok=True)

    # Process each zoom level
    for zoom in range(min_zoom, max_zoom + 1):
        print(f"\nZoom level {zoom}...")

        # Calculate tile range for this zoom
        start_x = lon_to_tile_x(min_lon, zoom)
        end_x = lon_to_tile_x(max_lon, zoom)
        start_y = lat_to_tile_y(max_lat, zoom)  # Note: Y is inverted
        end_y = lat_to_tile_y(min_lat, zoom)

        # Clamp values
        max_tile = 1 << zoom
        start_x = max(0, start_x)
        end_x = min(max_tile - 1, end_x)
        start_y = max(0, start_y)
        end_y = min(max_tile - 1, end_y)

        tile_count = (end_x - start_x + 1) * (end_y - start_y + 1)

        if tile_count > max_tiles_per_zoom:
            print(f"  Skipping zoom {zoom}: too many tiles ({tile_count} > {max_tiles_per_zoom})")
            continue

        print(f"  Tile range: X[{start_x}-{end_x}] Y[{start_y}-{end_y}] = {tile_count} tiles")

        # Create zoom directory
        zoom_dir = layer_output_dir / str(zoom)
        zoom_dir.mkdir(exist_ok=True)

        # Reset layer reading
        layer.ResetReading()

        # Create a memory raster for the entire extent at this zoom
        # This is more efficient than processing each tile individually

        tiles_created = 0

        for tile_x in range(start_x, end_x + 1):
            for tile_y in range(start_y, end_y + 1):
                # Get tile bounds in Web Mercator
                wm_min_x, wm_max_x, wm_min_y, wm_max_y = tile_to_webmercator_bounds(
                    tile_x, tile_y, zoom
                )

                # Get tile bounds in lat/lon
                tile_min_lon, tile_max_lon = wm_min_x / WEB_MERCATOR_EXTENT * 180.0, wm_max_x / WEB_MERCATOR_EXTENT * 180.0
                tile_min_y_rad = wm_min_y / WEB_MERCATOR_EXTENT * math.pi
                tile_max_y_rad = wm_max_y / WEB_MERCATOR_EXTENT * math.pi
                tile_min_lat = (2.0 * math.atan(math.exp(tile_min_y_rad)) - math.pi / 2.0) * 180.0 / math.pi
                tile_max_lat = (2.0 * math.atan(math.exp(tile_max_y_rad)) - math.pi / 2.0) * 180.0 / math.pi

                # Create in-memory raster
                mem_ds = gdal.GetDriverByName('MEM').Create(
                    '', TILE_SIZE, TILE_SIZE, 1, gdal.GDT_Byte
                )
                mem_ds.SetGeoProjection([
                    tile_min_lon, (tile_max_lon - tile_min_lon) / TILE_SIZE, 0,
                    tile_max_lat, 0, -(tile_max_lat - tile_min_lat) / TILE_SIZE
                ])
                mem_ds.SetProjection('EPSG:4326')

                band = mem_ds.GetRasterBand(1)
                band.Fill(0)  # Transparent (no data)

                # Rasterize shapefile to this tile
                gdal.RasterizeLayer(
                    mem_ds,  # Output dataset
                    [1],     # Bands to update
                    layer,   # Input layer
                    burn_values=[255],  # Burn value
                    options=['ALL_TOUCHED=TRUE']
                )

                # Read raster data
                array = band.ReadAsArray()

                # Check if any pixels were drawn
                if array.max() > 0:
                    # Create PNG image
                    img = Image.new('RGBA', (TILE_SIZE, TILE_SIZE), (0, 0, 0, 0))
                    pixels = img.load()

                    for py in range(TILE_SIZE):
                        for px in range(TILE_SIZE):
                            if array[py, px] > 0:
                                pixels[px, py] = color

                    # Save tile
                    tile_filename = zoom_dir / f"{tile_x}_{tile_y}.png"
                    img.save(tile_filename, 'PNG')
                    tiles_created += 1

                mem_ds = None

        print(f"  Created {tiles_created} tiles")

    data_source = None
    print(f"\nDone! Tiles saved to: {layer_output_dir}")


def batch_rasterize_directory(input_dir, output_dir, zoom_range=(10, 16)):
    """Rasterize all shapefiles in a directory."""
    input_path = Path(input_dir)
    output_path = Path(output_dir)

    if not input_path.exists():
        raise FileNotFoundError(f"Input directory not found: {input_dir}")

    output_path.mkdir(parents=True, exist_ok=True)

    # Find all .shp files
    shapefiles = list(input_path.glob("*.shp"))

    if not shapefiles:
        print(f"No shapefiles found in {input_dir}")
        return

    print(f"Found {len(shapefiles)} shapefiles")
    print("=" * 60)

    for shp in shapefiles:
        try:
            rasterize_shapefile_to_tiles(
                shp,
                output_dir,
                min_zoom=zoom_range[0],
                max_zoom=zoom_range[1]
            )
        except Exception as e:
            print(f"Error processing {shp.name}: {e}")
            continue

        print("=" * 60)

    print(f"\nBatch processing complete!")


def main():
    parser = argparse.ArgumentParser(
        description='Rasterize shapefiles to tiles for ECDIS thematic display'
    )

    parser.add_argument(
        '--input', '-i',
        help='Input shapefile or directory containing shapefiles'
    )

    parser.add_argument(
        '--output', '-o',
        default='thematic-tiles',
        help='Output directory for tiles (default: thematic-tiles)'
    )

    parser.add_argument(
        '--layer', '-l',
        help='Layer name (for single file mode)'
    )

    parser.add_argument(
        '--min-zoom',
        type=int,
        default=10,
        help='Minimum zoom level (default: 10)'
    )

    parser.add_argument(
        '--max-zoom',
        type=int,
        default=16,
        help='Maximum zoom level (default: 16)'
    )

    parser.add_argument(
        '--batch', '-b',
        action='store_true',
        help='Batch mode: process all .shp files in input directory'
    )

    args = parser.parse_args()

    if not args.input:
        parser.print_help()
        sys.exit(1)

    if args.batch:
        batch_rasterize_directory(
            args.input,
            args.output,
            zoom_range=(args.min_zoom, args.max_zoom)
        )
    else:
        rasterize_shapefile_to_tiles(
            args.input,
            args.output,
            layer_name=args.layer,
            min_zoom=args.min_zoom,
            max_zoom=args.max_zoom
        )


if __name__ == '__main__':
    main()
