#!/bin/bash
#SBATCH --job-name=snowcast_solar
#SBATCH --output=logs/solar_%a_%j.out
#SBATCH --error=logs/solar_%a_%j.err
#SBATCH --time=04:00:00
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=96
#SBATCH --array=0-2

# Description:
# This Slurm script runs the SnowCast solar calculation pipeline in parallel.
# It uses a job array to process each department in a separate job.
# The array range (0-2) corresponds to the indices of TARGET_DEPARTMENTS in src/config.py.
# Adjust --array=0-N where N is len(TARGET_DEPARTMENTS) - 1.

# Create logs directory if it doesn't exist
mkdir -p logs

# Load modules (Uncomment and adjust as needed for your cluster)
# module load python/3.9
# module load gdal
# module load cmake

# Activate virtual environment
if [ -f ".venv/bin/activate" ]; then
    source .venv/bin/activate
else
    echo "Warning: .venv not found. Assuming environment is already set up."
fi

# Print job info
echo "Job ID: $SLURM_JOB_ID"
echo "Task ID: $SLURM_ARRAY_TASK_ID"
echo "Node: $HOSTNAME"
echo "Date: $(date)"

# Run the script with the array task ID as the index
# This will pick the department at TARGET_DEPARTMENTS[SLURM_ARRAY_TASK_ID]
python -m src.solar.run_solar_parquet --index $SLURM_ARRAY_TASK_ID

echo "Job finished at $(date)"
