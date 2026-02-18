import os
import glob
import subprocess
import pandas as pd
import matplotlib.pyplot as plt
import argparse

# ==============================================================================
# Configuration
# ==============================================================================
EXECUTABLE = "../release/mgconvergence"       # Path to your compiled C++ executable
MESH_FOLDER = "meshes"        # Folder containing the mesh files
OUTPUT_FOLDER = "out"         # Folder to save persistent CSV results


MESH_CONFIG = {
    "cube.msh": 4,
    "ball.msh": 4,
    "ball_hole.msh": 4,
    "corner.msh": 4,
    "corner_structured.msh": 4,
    "cylinder.msh": 4
}

GMRES_RUNS = 8
NEV = 2
EW_TOL = 1e-4
GMRES_TOL = 1e-6

def parse_arguments():
    parser = argparse.ArgumentParser(description="Run StokesMG convergence study.")
    parser.add_argument('--rerun', action='store_true',
                        help="Force re-run of all simulations. WARNING: DELETES ALL files in 'out' folder.")
    parser.add_argument('--plot-only', action='store_true',
                        help="Only plot existing CSVs. Do not run any new simulations.")
    return parser.parse_args()

def run_study():
    args = parse_arguments()

    # 1. Setup Output Folder
    os.makedirs(OUTPUT_FOLDER, exist_ok=True)

    # --- CLEANUP IF RERUN ---
    if args.rerun:
        print(f"(!) --rerun flag set. Emptying '{OUTPUT_FOLDER}' folder...")
        files = glob.glob(os.path.join(OUTPUT_FOLDER, "*"))
        for f in files:
            try:
                os.remove(f)
            except Exception as e:
                print(f"Error deleting {f}: {e}")
        print("Folder emptied.\n")

    # 2. Find Meshes
    mesh_files = glob.glob(os.path.join(MESH_FOLDER, "*.msh"))
    mesh_files += glob.glob(os.path.join(MESH_FOLDER, "*.mesh"))

    if not mesh_files:
        print(f"Error: No mesh files found in folder '{MESH_FOLDER}'")
        return

    all_results = {'V': {}, 'W': {}}
    cycles = ['V', 'W']

    # 3. Main Loop
    for cycle in cycles:
        print(f"\n" + "="*40)
        print(f" Processing {cycle}-Cycle Data")
        print(f"="*40)

        for mesh_path in mesh_files:
            mesh_filename = os.path.basename(mesh_path)
            mesh_name_no_ext = os.path.splitext(mesh_filename)[0]

            if mesh_filename not in MESH_CONFIG:
                continue

            refinements = MESH_CONFIG[mesh_filename]
            csv_path = os.path.join(OUTPUT_FOLDER, f"{mesh_name_no_ext}_{cycle}.csv")

            # Check if we should use cache (Only if NOT rerun, since rerun deletes files anyway)
            if os.path.exists(csv_path) and not args.rerun:
                try:
                    df = pd.read_csv(csv_path)
                    # Verify if the cached file has enough refinements
                    if df['Refinements'].max() >= refinements:
                        all_results[cycle][mesh_name_no_ext] = df
                        print(f"[CACHED] {mesh_filename} ({cycle})")
                        continue
                except Exception:
                    pass

            if args.plot_only:
                # If file doesn't exist and we are in plot-only mode, skip
                if not os.path.exists(csv_path):
                    continue

            print(f"[RUNNING] {mesh_filename} (Cycle: {cycle}, Refs: {refinements})...")
            cmd = [
                EXECUTABLE, "--mesh", mesh_path, "--refinements", str(refinements),
                "--output", csv_path, "--nev", str(NEV), "--gmres", str(GMRES_RUNS),
                "--eval_tol", str(EW_TOL), "--gmres_tol", str(GMRES_TOL), "--cycle", cycle
            ]

            try:
                subprocess.run(cmd, check=True)
                if os.path.exists(csv_path):
                    all_results[cycle][mesh_name_no_ext] = pd.read_csv(csv_path)
            except Exception as e:
                print(f"[FAIL] {mesh_filename}: {e}")

    # 4. Plotting
    if not any(all_results['V']) and not any(all_results['W']):
        print("\nNo results available.")
        return

    plot_results_2x2(all_results)

def plot_results_2x2(all_results):
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    colors = plt.cm.tab10.colors
    cycles = ['V', 'W']

    for row_idx, cycle in enumerate(cycles):
        results = all_results[cycle]
        ax_conv, ax_gmres = axes[row_idx, 0], axes[row_idx, 1]

        for i, (name, df) in enumerate(results.items()):
            color = colors[i % len(colors)]
            target_col = 'AbsEval1' if 'AbsEval1' in df.columns else 'AbsEval0'

            df_clean = df.dropna(subset=[target_col])
            ax_conv.plot(df_clean['DOFs'], df_clean[target_col],
                         marker='o', label=name, color=color)

            if 'AvgGMRES' in df.columns:
                ax_gmres.plot(df['DOFs'], df['AvgGMRES'],
                              marker='s', linestyle='--', label=name, color=color)

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
    output_img = "mesh_comparison_2x2.pdf"
    plt.savefig(output_img)
    print(f"\nPlots saved to {output_img}")
    plt.show()

if __name__ == "__main__":
    run_study()
