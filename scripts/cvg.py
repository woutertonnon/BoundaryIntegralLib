import os
import glob
import subprocess
import csv
import matplotlib.pyplot as plt
import argparse
import numpy as np
from concurrent.futures import ThreadPoolExecutor, as_completed

# ==============================================================================
# Configuration
# ==============================================================================
EXECUTABLE = "../release/mgconvergence"
MESH_FOLDER = "../meshes"
OUTPUT_FOLDER = "out"
PLOT_FOLDER = "plots"  # New directory for plots

NUM_JOBS = 4  # Number of parallel jobs

# Default number of refinements for all meshes
DEFAULT_REFINEMENTS = 4

# Penalty Range Configuration
PENALTY_MIN = 10.0
PENALTY_MAX = 40.0
PENALTY_STEPS = 4

# Mesh configuration: Using the default for all
MESH_CONFIG = {
    "cube.msh": DEFAULT_REFINEMENTS,
    "ball.msh": DEFAULT_REFINEMENTS,
    "ball_hole.msh": DEFAULT_REFINEMENTS,
    "corner.msh": DEFAULT_REFINEMENTS,
    "corner_structured.msh": DEFAULT_REFINEMENTS,
    "cylinder.msh": DEFAULT_REFINEMENTS
}

GMRES_RUNS = 8
NEV = 2
EW_TOL = 1e-3
GMRES_TOL = 1e-6

def parse_arguments():
    parser = argparse.ArgumentParser(description="Run StokesMG penalty sensitivity study.")
    parser.add_argument('--rerun', action='store_true', help="Force re-run of all simulations.")
    parser.add_argument('--plot-only', action='store_true', help="Only plot existing CSVs.")
    return parser.parse_args()

def run_single_job(cmd, mesh_filename, cycle, p_val):
    """Helper function to run a single C++ execution."""
    print(f"[RUNNING] {mesh_filename} | Cycle: {cycle} | Penalty: {p_val}")
    try:
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        print(f"[DONE]    {mesh_filename} | Cycle: {cycle} | Penalty: {p_val}")
    except subprocess.CalledProcessError as e:
        print(f"[FAIL]    {mesh_filename} | Cycle: {cycle} | Penalty: {p_val}")
        print(f"          Error Output: {e.stderr.strip()}")

def read_csv_as_dict(filepath):
    """Reads a CSV into a dictionary of numpy arrays (pandas replacement)."""
    with open(filepath, 'r') as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames:
            return {}

        data = {field: [] for field in reader.fieldnames}
        for row in reader:
            for field in reader.fieldnames:
                val = row[field].strip()
                if val == '' or val.lower() == 'nan':
                    data[field].append(np.nan)
                else:
                    data[field].append(float(val))

    return {k: np.array(v) for k, v in data.items()}

def run_study():
    args = parse_arguments()

    # Create necessary directories
    os.makedirs(OUTPUT_FOLDER, exist_ok=True)
    os.makedirs(PLOT_FOLDER, exist_ok=True)

    if args.rerun:
        print(f"(!) Emptying '{OUTPUT_FOLDER}' and '{PLOT_FOLDER}'...")
        for folder in [OUTPUT_FOLDER, PLOT_FOLDER]:
            for f in glob.glob(os.path.join(folder, "*")):
                try:
                    os.remove(f)
                except Exception:
                    pass

    mesh_files = glob.glob(os.path.join(MESH_FOLDER, "*.msh")) + glob.glob(os.path.join(MESH_FOLDER, "*.mesh"))
    penalties = np.linspace(PENALTY_MIN, PENALTY_MAX, PENALTY_STEPS)
    cycles = ['V', 'W']

    # 1. Build job list
    jobs_to_run = []
    for p in penalties:
        p_val = round(p, 2)
        for cycle in cycles:
            for mesh_path in mesh_files:
                mesh_filename = os.path.basename(mesh_path)
                mesh_name_no_ext = os.path.splitext(mesh_filename)[0]

                if mesh_filename not in MESH_CONFIG:
                    continue

                refinements = MESH_CONFIG[mesh_filename]
                csv_path = os.path.join(OUTPUT_FOLDER, f"{mesh_name_no_ext}_{cycle}_p{p_val}.csv")

                if not args.plot_only:
                    if not os.path.exists(csv_path) or args.rerun:
                        cmd = [
                            EXECUTABLE, "--mesh", mesh_path, "--refinements", str(refinements),
                            "--output", csv_path, "--nev", str(NEV), "--gmres", str(GMRES_RUNS),
                            "--eval_tol", str(EW_TOL), "--gmres_tol", str(GMRES_TOL),
                            "--cycle", cycle, "--penalty", str(p_val)
                        ]
                        jobs_to_run.append((cmd, mesh_filename, cycle, p_val))

    # 2. Parallel Execution
    if jobs_to_run:
        print(f"\nDispatching {len(jobs_to_run)} jobs across {NUM_JOBS} workers...\n" + "="*60)
        with ThreadPoolExecutor(max_workers=NUM_JOBS) as executor:
            futures = [executor.submit(run_single_job, *job) for job in jobs_to_run]
            for future in as_completed(futures):
                future.result()
        print("="*60 + "\nAll jobs completed.\n")
    else:
        print("No new simulations to run. Moving to plotting step...")

    # 3. Read results
    all_results = {}
    for p in penalties:
        p_val = round(p, 2)
        all_results[p_val] = {'V': {}, 'W': {}}
        for cycle in cycles:
            for mesh_path in mesh_files:
                mesh_filename = os.path.basename(mesh_path)
                mesh_name_no_ext = os.path.splitext(mesh_filename)[0]

                if mesh_filename not in MESH_CONFIG:
                    continue

                csv_path = os.path.join(OUTPUT_FOLDER, f"{mesh_name_no_ext}_{cycle}_p{p_val}.csv")
                if os.path.exists(csv_path):
                    all_results[p_val][cycle][mesh_name_no_ext] = read_csv_as_dict(csv_path)

    # 4. Plotting
    if all_results:
        plot_dof_convergence_for_penalty(all_results, cycles, round(PENALTY_MIN, 2))
        plot_penalty_per_mesh_refinements(all_results, cycles, penalties)

def plot_dof_convergence_for_penalty(all_results, cycles, target_penalty):
    if target_penalty not in all_results:
        return

    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    colors = plt.cm.tab10.colors
    fig.suptitle(f"Convergence Rates (Penalty = {target_penalty})", fontsize=16)

    for row_idx, cycle in enumerate(cycles):
        results = all_results[target_penalty][cycle]
        ax_conv, ax_gmres = axes[row_idx, 0], axes[row_idx, 1]

        for i, (name, df) in enumerate(results.items()):
            if not df: continue
            color = colors[i % len(colors)]
            target_col = 'AbsEval1' if 'AbsEval1' in df else 'AbsEval0'

            mask = ~np.isnan(df[target_col])
            dofs_clean = df['DOFs'][mask]
            evals_clean = df[target_col][mask]

            if len(dofs_clean) > 0:
                ax_conv.plot(dofs_clean, evals_clean, marker='o', label=name, color=color)
            if 'AvgGMRES' in df and len(df['AvgGMRES']) > 0:
                ax_gmres.plot(df['DOFs'], df['AvgGMRES'], marker='s', linestyle='--', label=name, color=color)

        ax_conv.set_title(f"{cycle}-Cycle Convergence Factor")
        ax_conv.set_ylabel(r"Largest Error Eigenvalue $|\lambda|_{max}$")
        ax_gmres.set_title(f"{cycle}-Cycle GMRES Iterations")
        ax_gmres.set_ylabel("Avg Iterations")

        for ax in [ax_conv, ax_gmres]:
            ax.set_xlabel("DOFs")
            ax.set_xscale('log')
            ax.grid(True, alpha=0.3)
            ax.legend()

    plt.tight_layout()
    plt.subplots_adjust(top=0.92)
    # Save to PLOT_FOLDER
    plt.savefig(os.path.join(PLOT_FOLDER, f"mesh_comparison_p{target_penalty}.pdf"))
    plt.close(fig)

def plot_penalty_per_mesh_refinements(all_results, cycles, penalties):
    sample_p = list(all_results.keys())[0]
    mesh_names = list(all_results[sample_p]['V'].keys())

    for mesh_name in mesh_names:
        fig, axes = plt.subplots(len(cycles), 2, figsize=(14, 6 * len(cycles)))
        fig.suptitle(f"Penalty Parameter Sensitivity - Mesh: {mesh_name}", fontsize=16)

        for row_idx, cycle in enumerate(cycles):
            ax_conv, ax_gmres = axes[row_idx, 0], axes[row_idx, 1]
            sample_df = all_results[sample_p][cycle].get(mesh_name)
            if not sample_df: continue

            ref_levels = np.unique(sample_df['Refinements'])
            colors = plt.cm.viridis(np.linspace(0, 1, len(ref_levels)))

            for i, ref in enumerate(ref_levels):
                p_plot, conv_plot, gmres_plot = [], [], []
                for p in penalties:
                    p_val = round(p, 2)
                    if mesh_name in all_results[p_val][cycle]:
                        df = all_results[p_val][cycle][mesh_name]
                        mask = df['Refinements'] == ref
                        if np.any(mask):
                            p_plot.append(p_val)
                            target_col = 'AbsEval1' if 'AbsEval1' in df else 'AbsEval0'
                            conv_plot.append(df[target_col][mask][0])
                            gmres_plot.append(df['AvgGMRES'][mask][0])

                if p_plot:
                    ax_conv.plot(p_plot, conv_plot, marker='o', label=f"Refinement {int(ref)}", color=colors[i])
                    ax_gmres.plot(p_plot, gmres_plot, marker='s', linestyle='--', label=f"Refinement {int(ref)}", color=colors[i])

            ax_conv.set_title(f"{cycle}-Cycle: Max Eigenvalue vs. Penalty")
            ax_gmres.set_title(f"{cycle}-Cycle: GMRES Iterations vs. Penalty")

            for ax in [ax_conv, ax_gmres]:
                ax.set_xlabel("Penalty Parameter")
                ax.grid(True, alpha=0.3)
                if ax.has_data(): ax.legend()

        plt.tight_layout()
        plt.subplots_adjust(top=0.92)
        # Save to PLOT_FOLDER
        plt.savefig(os.path.join(PLOT_FOLDER, f"penalty_sensitivity_{mesh_name}.pdf"))
        plt.close(fig)

if __name__ == "__main__":
    run_study()
