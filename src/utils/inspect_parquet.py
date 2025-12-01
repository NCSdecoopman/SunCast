"""
Small helper to inspect Parquet output for a given department.

The goal is to quickly verify that the C++ streaming output and the
Python writer agree on array sizes, value ranges, etc.
"""
from pathlib import Path

import pyarrow.parquet as pq
import pandas as pd

from ..config import PROJECT_ROOT

PARQUET_DIR = PROJECT_ROOT / "data" / "parquet"


def inspect_parquet(dept_code: str) -> None:
    """
    Print basic information about the Parquet partition for one department.

    This shows the schema, number of rows, array lengths and simple
    statistics for the first day in the dataset.
    """
    path = PARQUET_DIR / f"dept={dept_code}"
    print(f"\nInspecting {path}")

    if not path.exists():
        print("  Not found!")
        return

    parquet_file = path / "data.parquet"
    if not parquet_file.exists():
        print("  Parquet file not found!")
        return

    table = pq.read_table(parquet_file)
    print(f"  Schema: {table.schema}")
    print(f"  Rows: {table.num_rows}")
    print(f"  Columns: {table.column_names}")

    # Check first row (typically Day 1)
    df = table.to_pandas()
    if df.empty:
        print("  No rows in table.")
        return

    row = df.iloc[0]
    print(f"  Day: {row['day']}")

    sunrise = row["sunrise"]
    sunset = row["sunset"]

    print(f"  Sunrise array length: {len(sunrise)}")
    print(f"  Sunset array length: {len(sunset)}")

    # Basic stats, ignoring sentinel -1 for “no sunrise”
    valid_sunrise = sunrise[sunrise != -1]
    if len(valid_sunrise) > 0:
        print(
            "  Sunrise (min/mean/max): "
            f"{valid_sunrise.min()}/{valid_sunrise.mean():.1f}/{valid_sunrise.max()}"
        )
        print(
            "  Sunrise (hours): "
            f"{valid_sunrise.min() / 60:.2f} - {valid_sunrise.max() / 60:.2f}"
        )
    else:
        print("  No valid sunrise data")


def main() -> None:
    """Inspect default test departments."""
    for dept in ["38", "73", "74"]:
        inspect_parquet(dept)


if __name__ == "__main__":
    main()
